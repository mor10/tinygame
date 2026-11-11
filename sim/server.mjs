// Loopback server for the TinyGame simulator. No dependencies.
//
//   node sim/server.mjs [--port N] [bundle-file]
//
// Serves sim/web on 127.0.0.1 only. If a bundle file is given (it may not
// exist yet), the page can fetch it from /bundle, and every time the file
// changes the server sends a "bundle" event on /events (server-sent events) so
// the page reloads the bundle. The WebAssembly is not rebuilt.
//
// The first line printed on stdout is the page address.

import { createServer } from "node:http";
import { readFile, stat } from "node:fs/promises";
import { watchFile, unwatchFile } from "node:fs";
import { dirname, join, normalize, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";

const WEB = join(dirname(fileURLToPath(import.meta.url)), "web");
const TYPES = {
  ".html": "text/html; charset=utf-8", ".js": "text/javascript; charset=utf-8",
  ".mjs": "text/javascript; charset=utf-8", ".css": "text/css; charset=utf-8",
  ".wasm": "application/wasm", ".json": "application/json", ".png": "image/png",
};

export async function startServer({ port = 0, bundle = null, pollMs = 200 } = {}) {
  const bundlePath = bundle ? resolve(bundle) : null;
  const clients = new Set();
  let version = 0;   // counts bundle changes (also sent with each event)

  const send = (name, data) => {
    for (const client of clients) client.write(`event: ${name}\ndata: ${JSON.stringify(data)}\n\n`);
  };

  if (bundlePath) {
    // Polling notices a file that is created, replaced or rewritten, however
    // the editor or build tool saves it.
    watchFile(bundlePath, { interval: pollMs }, (now, before) => {
      if (now.mtimeMs === before.mtimeMs && now.size === before.size) return;
      version++;
      send("bundle", { version, exists: now.size > 0 || now.mtimeMs > 0 });
    });
  }

  const server = createServer(async (request, response) => {
    try {
      if (request.method !== "GET" && request.method !== "HEAD") {
        response.writeHead(405, { Allow: "GET, HEAD" }).end();
        return;
      }
      let path;
      try { path = decodeURIComponent(new URL(request.url, "http://x").pathname); }
      catch { response.writeHead(400).end("Bad address"); return; }
      const headers = { "Cache-Control": "no-store", "X-Content-Type-Options": "nosniff" };

      if (path === "/events") {
        response.writeHead(200, { ...headers, "Content-Type": "text/event-stream", Connection: "keep-alive" });
        response.write(`retry: 1000\n: connected\n\n`);
        clients.add(response);
        const beat = setInterval(() => response.write(": keep-alive\n\n"), 15000);
        response.on("close", () => { clearInterval(beat); clients.delete(response); });
        return;
      }
      if (path === "/bundle") {
        if (!bundlePath) { response.writeHead(404, headers).end("No bundle file was given to the server"); return; }
        try {
          const bytes = await readFile(bundlePath);
          response.writeHead(200, { ...headers, "Content-Type": "application/octet-stream", "Content-Length": bytes.length });
          response.end(request.method === "HEAD" ? undefined : bytes);
        } catch {
          response.writeHead(404, headers).end("The bundle file does not exist yet");
        }
        return;
      }

      if (path.endsWith("/")) path += "index.html";
      const file = normalize(join(WEB, path));
      if (file !== WEB && !file.startsWith(WEB + sep)) { response.writeHead(403).end("Forbidden"); return; }
      const info = await stat(file).catch(() => null);
      if (!info || !info.isFile()) { response.writeHead(404, headers).end("Not found"); return; }
      const type = TYPES[file.slice(file.lastIndexOf("."))] ?? "application/octet-stream";
      response.writeHead(200, { ...headers, "Content-Type": type, "Content-Length": info.size });
      response.end(request.method === "HEAD" ? undefined : await readFile(file));
    } catch (error) {
      if (!response.headersSent) response.writeHead(500);
      response.end(`Server error: ${error.message}`);
    }
  });

  await new Promise((done, fail) => {
    server.once("error", fail);
    server.listen(port, "127.0.0.1", done);
  });
  const address = server.address();
  return {
    url: `http://127.0.0.1:${address.port}/`,
    port: address.port,
    async close() {
      if (bundlePath) unwatchFile(bundlePath);
      for (const client of clients) client.end();
      server.closeAllConnections();
      await new Promise(done => server.close(done));
    },
  };
}

function usage(message) {
  if (message) console.error(message);
  console.error("usage: node sim/server.mjs [--port N] [bundle-file]");
  process.exit(2);
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  let port = 0, bundle = null;
  const args = process.argv.slice(2);
  for (let i = 0; i < args.length; i++) {
    if (args[i] === "--port") {
      port = Number(args[++i]);
      if (!Number.isInteger(port) || port < 0 || port > 65535) usage("--port needs a number from 0 to 65535");
    } else if (args[i].startsWith("--")) usage(`Unknown option ${args[i]}`);
    else if (bundle === null) bundle = args[i];
    else usage("Only one bundle file can be given");
  }
  let running;
  try { running = await startServer({ port, bundle }); }
  catch (error) { console.error(`Cannot start the server: ${error.message}`); process.exit(1); }
  console.log(running.url);
  if (bundle) console.log(`Watching ${resolve(bundle)}`);
  for (const signal of ["SIGINT", "SIGTERM"]) process.on(signal, () => running.close().then(() => process.exit(0)));
}
