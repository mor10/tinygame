import { createServer } from "node:http";
import { randomBytes, randomUUID } from "node:crypto";
import { readFile, mkdir, writeFile } from "node:fs/promises";
import { watch } from "node:fs";
import { resolve, join, extname, relative, isAbsolute } from "node:path";
import { spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
import { validateDevice } from "./web/input.mjs";

const builds = new Map();
export function build(root, onOutput = () => {}, board = "147b") {
  const key = `${root}\0${board}`;
  if (builds.has(key)) return builds.get(key);
  const promise = new Promise((resolveBuild, reject) => {
    const child = spawn(process.env.PYTHON || "python3", ["tools/tg.py", "sim", "--build-only", "--board", board], { cwd: root });
    let output = "";
    const consume = chunk => { output = (output + chunk).slice(-60000); onOutput(chunk.toString()); };
    child.stdout.on("data", consume); child.stderr.on("data", consume);
    child.on("error", reject);
    child.on("close", code => code === 0 ? resolveBuild({ output }) : reject(new Error(`Simulator build failed (${code})\n${output}`)));
  }).finally(() => builds.delete(key));
  builds.set(key, promise);
  return promise;
}

export function validGame(game) {
  return typeof game === "string" && /^[a-z][a-z0-9_]{0,14}$/.test(game);
}
export function screenshotBytes(png, board = "147b") {
  const [width, height] = board === "169" ? [280, 240] : [320, 172];
  if (typeof png !== "string" || !/^data:image\/png;base64,[A-Za-z0-9+/=]+$/.test(png)) throw new Error("Expected a PNG data URL");
  const bytes = Buffer.from(png.split(",")[1], "base64");
  if (bytes.length > 1024 * 1024 || bytes.length < 33
    || bytes.subarray(0, 8).toString("hex") !== "89504e470d0a1a0a"
    || bytes.readUInt32BE(16) !== width || bytes.readUInt32BE(20) !== height) {
    throw new Error(`Expected a ${width}x${height} PNG screenshot (maximum 1 MB)`);
  }
  return bytes;
}

export async function startSimulator({ root, artifacts, game, port = 0, autoBuild = true, board = "147b" }) {
  root = resolve(root);
  if (game !== undefined && !validGame(game)) throw new Error("Invalid game id");
  if (!["147b", "169"].includes(board)) throw new Error("Invalid board profile");
  const web = join(root, "sim", "web");
  const prefix = `/${randomBytes(24).toString("hex")}/`;
  const clients = new Set(), pending = new Map();
  let bridge, bridgeStop, closed = false, timer, rebuilding = false, dirty = false;
  let nextLog = 0, pageSeen = 0, firmwareReady = false;
  const logs = [];
  const state = { status: "starting", game, board, buildError: null, connected: false, deviceMessage: "Disconnected", sleeping: false };
  const emit = (type, value) => {
    const message = `event: ${type}\ndata: ${JSON.stringify(value)}\n\n`;
    for (const client of clients) client.write(message);
  };
  const connection = (connected, message) => {
    state.connected = connected; state.deviceMessage = message;
    emit("connection", { connected, message });
  };
  const addLog = line => { logs.push({ index: nextLog++, text: String(line).slice(0, 4096) }); if (logs.length > 1000) logs.shift(); };
  const request = (action, input = {}) => new Promise((resolveRequest, reject) => {
    if (clients.size > 1) { reject(new Error("Multiple simulator pages are connected; close extra tabs before using agent actions")); return; }
    if (!clients.size || Date.now() - pageSeen > 3000) { reject(new Error("Simulator page is not ready; open it and wait for firmware boot")); return; }
    if (action !== "reload" && !firmwareReady) { reject(new Error("Simulator firmware is not ready")); return; }
    const id = randomUUID();
    const duration = action === "send" ? Math.min(300000, 10000 + input.commands.reduce((sum, text) => sum + (text.startsWith("wait ") ? Number(text.slice(5)) * 1000 : 5060), 0)) : 15000;
    const timeout = setTimeout(() => { pending.delete(id); reject(new Error(`Simulator action timed out: ${action}`)); }, duration);
    pending.set(id, {
      resolve: result => { clearTimeout(timeout); resolveRequest(result); },
      reject: e => { clearTimeout(timeout); reject(e); },
    });
    emit("request", { id, action, input });
  });
  const saveScreenshot = async (png, label = "screen") => {
    const bytes = screenshotBytes(png, board);
    const name = String(label).replace(/[^a-zA-Z0-9_-]/g, "-").slice(0, 64) || "screen";
    await mkdir(artifacts, { recursive: true });
    const path = join(artifacts, `${name}-${Date.now()}-${randomBytes(3).toString("hex")}.png`);
    await writeFile(path, bytes, { flag: "wx" });
    return { path, width: board === "169" ? 280 : 320, height: board === "169" ? 240 : 172 };
  };
  const disconnect = async () => {
    if (!bridge) return { connected: false };
    const child = bridge;
    child.kill("SIGINT");
    await bridgeStop;
    return { connected: false };
  };
  const connect = async explicit => {
    if (bridge) return { connected: state.connected, message: state.deviceMessage };
    if (explicit !== undefined && (typeof explicit !== "string" || !explicit || /[\r\n\0]/.test(explicit))) throw new Error("Invalid serial port");
    const child = spawn(process.env.PYTHON || "python3", ["tools/tg.py", "bridge", ...(explicit ? ["--port", explicit] : [])], { cwd: root });
    bridge = child;
    connection(false, "Connecting...");
    let buffer = "", stderr = "", settled = false;
    let resolveStop;
    bridgeStop = new Promise(r => { resolveStop = r; });
    return await new Promise((resolveConnect, reject) => {
      const timeout = setTimeout(() => {
        if (!settled) { settled = true; child.kill("SIGINT"); reject(new Error("No controller frames received; flash firmware with stream support first")); }
      }, 8000);
      const fail = e => {
        clearTimeout(timeout);
        if (!settled) { settled = true; reject(e); }
        connection(false, e.message);
      };
      child.stdout.on("data", data => {
        buffer += data;
        if (buffer.length > 16384) { child.kill("SIGINT"); fail(new Error("Controller response exceeded line limit")); return; }
        let end;
        while ((end = buffer.indexOf("\n")) !== -1) {
          const line = buffer.slice(0, end); buffer = buffer.slice(end + 1);
          try {
            const frame = JSON.parse(line);
            if (!validateDevice(frame)) throw new Error("Invalid controller frame");
            emit("device", frame);
            if (!settled) {
              settled = true; clearTimeout(timeout);
              connection(true, "Connected console");
              resolveConnect({ connected: true });
            }
          } catch (e) { child.kill("SIGINT"); fail(new Error(`Controller protocol error: ${e.message}`)); }
        }
      });
      child.stderr.on("data", data => { stderr = (stderr + data).slice(-8000); });
      child.on("error", fail);
      child.on("close", code => {
        bridge = undefined; resolveStop(); clearTimeout(timeout);
        const message = code === 0 ? "Disconnected" : (stderr.trim() || `Controller bridge stopped (${code})`);
        if (!settled) fail(new Error(message)); else connection(false, message);
      });
    });
  };
  const action = async (name, input = {}) => {
    if (name === "get_output") {
      const since = input.since ?? 0;
      if (!Number.isSafeInteger(since) || since < 0) throw new Error("since must be a nonnegative integer");
      return { ...state, pageReady: firmwareReady && Date.now() - pageSeen < 3000, lines: logs.filter(line => line.index >= since), next: nextLog };
    }
    if (name === "connect_device") return connect(input.port);
    if (name === "disconnect_device") return disconnect();
    if (name === "launch" && !validGame(input.game)) throw new Error("Invalid game id");
    if (name === "send" && (!Array.isArray(input.commands) || input.commands.length > 100
      || !input.commands.every(text => typeof text === "string" && text.length <= 240 && !/[\r\n\0]/.test(text)
        && (!text.startsWith("wait ") || (Number.isFinite(Number(text.slice(5))) && Number(text.slice(5)) >= 0 && Number(text.slice(5)) <= 60))))) throw new Error("Invalid commands; maximum 100 single-line commands, waits 0..60 seconds");
    if (!["reload", "launch", "send", "screenshot"].includes(name)) throw new Error("Unknown simulator action");
    const result = await request(name, input);
    return name === "screenshot" ? saveScreenshot(result.png, input.label) : result;
  };
  const mime = { ".html": "text/html; charset=utf-8", ".css": "text/css", ".js": "text/javascript", ".mjs": "text/javascript", ".wasm": "application/wasm" };
  const server = createServer(async (req, res) => {
    const json = (status, value) => { res.writeHead(status, { "Content-Type": "application/json" }); res.end(JSON.stringify(value)); };
    res.setHeader("Cache-Control", "no-store");
    res.setHeader("X-Content-Type-Options", "nosniff");
    try {
      if (req.headers.host !== `127.0.0.1:${server.address().port}`) { json(403, { error: "Host rejected" }); return; }
      const url = new URL(req.url, "http://127.0.0.1");
      if (!url.pathname.startsWith(prefix)) { json(404, { error: "Not found" }); return; }
      const route = decodeURIComponent(url.pathname.slice(prefix.length));
      if (req.method === "GET" && route === "events") {
        res.writeHead(200, { "Content-Type": "text/event-stream", "Connection": "keep-alive" });
        res.write(": connected\n\n");
        res.write(`event: connection\ndata: ${JSON.stringify({ connected: state.connected, message: state.deviceMessage })}\n\n`);
        clients.add(res);
        req.on("close", () => {
          clients.delete(res);
          if (!clients.size) firmwareReady = false;
        });
        return;
      }
      if (req.method === "GET" && route === "state") { json(200, state); return; }
      if (req.method === "POST") {
        if (req.headers.origin && req.headers.origin !== `http://127.0.0.1:${server.address().port}`) { json(403, { error: "Origin rejected" }); return; }
        let body = "";
        for await (const chunk of req) {
          body += chunk;
          if (body.length > 1500000) { json(413, { error: "Request too large" }); return; }
        }
        const input = JSON.parse(body || "{}");
        if (route === "reply") {
          const item = pending.get(input.id);
          if (!item) { json(409, { error: "Action expired" }); return; }
          pending.delete(input.id);
          if (input.error) item.reject(new Error(String(input.error))); else item.resolve(input.result);
          json(200, {}); return;
        }
        if (route === "report") {
          if (!Array.isArray(input.lines) || input.lines.length > 1000 || !input.lines.every(line => typeof line === "string")) throw new Error("Invalid serial report");
          if (typeof input.ready !== "boolean") throw new Error("Invalid firmware readiness");
          input.lines.forEach(addLog); pageSeen = Date.now(); firmwareReady = input.ready;
          if (!state.buildError && !rebuilding) state.status = input.ready ? "ready" : "starting";
          state.sleeping = Boolean(input.sleeping);
          json(200, {}); return;
        }
        if (route === "screenshot") { json(200, await saveScreenshot(input.png, input.label)); return; }
        if (route === "connect") { json(200, await connect(input.port)); return; }
        if (route === "disconnect") { json(200, await disconnect()); return; }
        if (route === "action") { json(200, await action(input.name, input.input)); return; }
        json(404, { error: "Not found" }); return;
      }
      if (req.method !== "GET") { json(405, { error: "Method not allowed" }); return; }
      const file = resolve(web, route || "index.html");
      const rel = relative(web, file);
      if (rel.startsWith("..") || isAbsolute(rel) || !mime[extname(file)] || rel.endsWith(".test.mjs")) { json(404, { error: "Not found" }); return; }
      const bytes = await readFile(file);
      res.writeHead(200, { "Content-Type": mime[extname(file)] }); res.end(bytes);
    } catch (e) { if (!res.headersSent) json(e.code === "ENOENT" ? 404 : 400, { error: e.message }); else res.end(); }
  });
  await new Promise((r, reject) => { server.once("error", reject); server.listen(port, "127.0.0.1", r); });
  const url = `http://127.0.0.1:${server.address().port}${prefix}`;
  const rebuild = async () => {
    if (closed) return;
    if (rebuilding) { dirty = true; return; }
    rebuilding = true; state.status = "building";
    emit("build", { message: "Rebuilding firmware..." });
    try {
      await build(root, () => {}, board);
      state.buildError = null; state.status = "starting";
      emit("reload", {});
    } catch (e) {
      state.status = "build-error"; state.buildError = e.message;
      emit("build-error", { message: e.message });
    } finally {
      rebuilding = false;
      if (dirty) { dirty = false; rebuild(); }
    }
  };
  const watchers = autoBuild ? ["src", "include", "sim", "games", "engine"].map(folder => {
    const watcher = watch(join(root, folder), { recursive: true }, (_, filename) => {
      if (!filename || String(filename).split(/[\\/]/).some(part => part === "build" || part === "build-169") ||
          !/\.(cpp|h|c|js|mjs|css|html|lua|txt|toml|md)$/.test(String(filename))) return;
      clearTimeout(timer); timer = setTimeout(rebuild, 350);
    });
    watcher.on("error", e => { state.buildError = `File watcher failed: ${e.message}`; emit("build-error", { message: state.buildError }); });
    return watcher;
  }) : [];
  const keepalive = setInterval(() => {
    for (const client of clients) client.write(": heartbeat\n\n");
    if (bridge && Date.now() - pageSeen > 5000) disconnect().catch(e => connection(false, e.message));
  }, 2000);
  return {
    url, action, rebuild,
    close: async () => {
      closed = true; clearTimeout(timer); clearInterval(keepalive);
      watchers.forEach(w => w.close());
      await disconnect();
      for (const item of pending.values()) item.reject(new Error("Simulator closed"));
      pending.clear();
      clients.forEach(client => client.end());
      await new Promise(r => server.close(r));
    },
  };
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const root = resolve(fileURLToPath(new URL("..", import.meta.url)));
  const args = process.argv.slice(2);
  const portIndex = args.indexOf("--port");
  const port = portIndex === -1 ? 0 : Number(args[portIndex + 1]);
  const boardIndex = args.indexOf("--board");
  const board = boardIndex === -1 ? "147b" : args[boardIndex + 1];
  const artifacts = process.env.TINYGAME_SIM_ARTIFACTS || join(root, "sim", "screenshots");
  const instance = await startSimulator({ root, artifacts, port, board });
  process.stdout.write(instance.url + "\n");
  for (const signal of ["SIGINT", "SIGTERM"]) process.once(signal, async () => { await instance.close(); process.exit(0); });
}
