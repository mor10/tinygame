// TinyGame uploader: checks a cart with the WASM engine, then sends it to the board over Web Serial
// using the console's own commands (cart <bytes> <id>, play, rm, ls). Serve the repository root
// over HTTP (or HTTPS) and open /uploader/ in Chrome or Edge.
(async () => {
  const $ = (id) => document.getElementById(id);
  const PALETTE = [0x000000, 0x1e2340, 0x3a3f4b, 0x8a8f99, 0xffffff, 0xf7f0dc, 0xe8604c, 0xc0392b,
                   0xf2b632, 0xf08a3c, 0x5fb04b, 0x3fa89a, 0x9fd8cb, 0x3b6fd1, 0xf29bb0, 0x8a5a3c];
  const CART_MAX = 128 * 1024;
  const CHECK_FRAMES = 90;   // three seconds at 30 fps

  // ---------- engine (WASM) for checks and the preview ----------
  let sink = [];
  const M = await TinyGame({ print: (l) => sink.push(l), printErr: (l) => sink.push(l) });
  const tgLoad = M.cwrap("tg_load", "number", ["number", "number", "number", "number"]);
  const tgFrame = M.cwrap("tg_frame", "number", ["number", "number", "number", "number"]);
  const tgRgba = M.cwrap("tg_rgba", "number", []);
  const preview = $("preview").getContext("2d");
  const previewImg = preview.createImageData(320, 172);

  function drawPreview() {
    const p = tgRgba();
    previewImg.data.set(M.HEAPU8.subarray(p, p + 320 * 172 * 4));
    preview.putImageData(previewImg, 0, 0);
  }

  // Runs the cart headless for CHECK_FRAMES frames. Returns { ok, error } where error is the
  // engine's JSON error line (kind, line, msg).
  function check(src) {
    sink = [];
    const bytes = M.lengthBytesUTF8(src);
    const ptr = M.stringToNewUTF8(src);
    let ok = tgLoad(ptr, bytes, 1, 0) === 0;
    M._free(ptr);
    for (let f = 0; ok && f < CHECK_FRAMES; f++) ok = tgFrame(0, 0, 0, 0) === 0;
    drawPreview();
    const err = sink.map(tryJson).find((j) => j && j.t === "error");
    return { ok: ok && !err, error: err, logs: sink.map(tryJson).filter((j) => j && j.t === "log") };
  }

  function tryJson(l) { try { return JSON.parse(l); } catch (e) { return null; } }

  // ---------- cart metadata (for display; the board reads the real cart table) ----------
  function field(src, name) {
    const m = src.match(new RegExp(name + "\\s*=\\s*\"([^\"]*)\""));
    return m ? m[1] : "";
  }

  function drawIcon(src) {
    const c = $("icon").getContext("2d");
    c.clearRect(0, 0, 16, 16);
    const m = src.match(/icon\s*=\s*\[\[([\s\S]*?)\]\]/);
    if (!m) return;
    const rows = m[1].split("\n").map((r) => r.trim()).filter(Boolean);
    const img = c.createImageData(16, 16);
    rows.slice(0, 16).forEach((row, y) => [...row.slice(0, 16)].forEach((ch, x) => {
      const v = parseInt(ch, 16);
      if (Number.isNaN(v)) return;
      const o = (y * 16 + x) * 4, rgb = PALETTE[v];
      img.data[o] = rgb >> 16; img.data[o + 1] = (rgb >> 8) & 255; img.data[o + 2] = rgb & 255; img.data[o + 3] = 255;
    }));
    c.putImageData(img, 0, 0);
  }

  function slug(s) {
    return s.toLowerCase().replace(/\.lua$/, "").replace(/[^a-z0-9_-]+/g, "-").replace(/^-+|-+$/g, "").slice(0, 31);
  }

  // ---------- the cart being edited ----------
  let lastCheck = null, lastError = null, idTouched = false, fileName = "";

  function errorText(err, src) {
    const lines = src.split("\n");
    let ctx = "";
    if (err.line > 0) {
      for (let i = Math.max(1, err.line - 2); i <= Math.min(lines.length, err.line + 2); i++)
        ctx += (i === err.line ? "> " : "  ") + String(i).padStart(4) + "  " + lines[i - 1] + "\n";
    }
    return { ctx };
  }

  function showCheck(result, where) {
    const box = $("check");
    lastError = result.error ? { ...result.error, where } : null;
    $("copy").disabled = !lastError;
    if (result.ok) {
      box.className = "check ok";
      box.textContent = where === "board" ? "Running on the TinyGame." :
        "Looks good: it ran for three seconds in the simulator without an error.";
      return;
    }
    const e = result.error || { msg: "The game stopped." };
    box.className = "check bad";
    const head = e.kind === "stuck" ? "It got stuck" : e.kind === "memory" ? "It ran out of memory" : "It hit a bug";
    box.innerHTML = "";
    const strong = document.createElement("strong");
    strong.textContent = (where === "board" ? "On the TinyGame: " : "") + head + (e.line > 0 ? ` at line ${e.line}` : "") + ".";
    box.appendChild(strong);
    box.appendChild(document.createTextNode(" " + (e.kind === "stuck" ? "Maybe a loop that never ends?" : e.msg)));
    const { ctx } = errorText(e, $("src").value);
    if (ctx) { const pre = document.createElement("pre"); pre.textContent = ctx; box.appendChild(pre); }
  }

  function loadText(text, filename) {
    $("src").value = text;
    fileName = filename || "";
    refresh();
  }

  function refresh() {
    const src = $("src").value;
    const title = field(src, "title"), tagline = field(src, "tagline");
    $("title").textContent = title || (src.trim() ? "Untitled game" : "No game loaded");
    $("tagline").textContent = tagline || (src.trim() ? "" : "Drop a .lua file here, open one, or paste the code below.");
    if (!idTouched) $("id").value = slug(fileName || title || "");   // until the name is typed in, it follows the game
    drawIcon(src);
    if (!src.trim()) { lastCheck = null; $("check").className = "check"; $("check").textContent = "Load a game and it will be checked here before it goes to the TinyGame."; updateSend(); return; }
    if (new TextEncoder().encode(src).length > CART_MAX) {
      lastCheck = { ok: false, error: { msg: "This game is bigger than 128 KB, the most a TinyGame cart can be." } };
    } else {
      lastCheck = check(src);
    }
    showCheck(lastCheck, "sim");
    updateSend();
  }

  let refreshTimer = 0;
  $("src").addEventListener("input", () => { fileName = ""; clearTimeout(refreshTimer); refreshTimer = setTimeout(refresh, 400); });
  $("id").addEventListener("input", () => { idTouched = true; $("id").value = slug($("id").value); updateSend(); });
  $("file").addEventListener("change", (e) => { const f = e.target.files[0]; if (f) f.text().then((t) => loadText(t, f.name)); });
  addEventListener("dragover", (e) => { e.preventDefault(); $("cartpane").classList.add("drop"); });
  addEventListener("dragleave", () => $("cartpane").classList.remove("drop"));
  addEventListener("drop", (e) => {
    e.preventDefault();
    $("cartpane").classList.remove("drop");
    const f = e.dataTransfer.files[0];
    if (f) f.text().then((t) => loadText(t, f.name));
  });

  $("copy").addEventListener("click", async () => {
    if (!lastError) return;
    const src = $("src").value;
    const { ctx } = errorText(lastError, src);
    const text = [
      `My TinyGame game "${field(src, "title") || "untitled"}" stopped ${lastError.where === "board" ? "on the TinyGame" : "in the simulator"}.`,
      `Error: ${lastError.msg}${lastError.line > 0 ? ` (line ${lastError.line})` : ""}`,
      ctx ? `Code around that line:\n${ctx}` : "",
      "Please fix it and give me the whole corrected game.",
      "", "Full game:", "```lua", src, "```",
    ].filter((l) => l !== null).join("\n");
    await navigator.clipboard.writeText(text);
    $("sendstatus").textContent = "Copied. Paste it into your AI helper.";
  });

  // ---------- serial ----------
  let port = null, writer = null, games = new Map();
  if (!("serial" in navigator)) { $("nosupport").style.display = "block"; $("connect").disabled = true; }

  function setConnected(on) {
    $("conn").textContent = on ? "Connected" : "Not connected";
    $("conn").classList.toggle("on", on);
    $("connect").textContent = on ? "Disconnect" : "Connect TinyGame";
    updateSend();
  }

  function updateSend() {
    const hasCart = $("src").value.trim().length > 0;
    $("send").disabled = !(port && hasCart && $("id").value && lastCheck && lastCheck.ok);
  }

  async function sendBytes(bytes) {
    for (let i = 0; i < bytes.length; i += 4096) await writer.write(bytes.subarray(i, i + 4096));
  }

  async function command(line) { if (writer) await sendBytes(new TextEncoder().encode(line + "\n")); }

  function log(j, raw) {
    if (!j || j.t === "stats" || j.t === "hash") return;
    const div = document.createElement("div");
    div.className = j.t === "error" ? "error" : j.t === "log" ? "log" : "";
    div.textContent = j.t === "log" ? j.msg : raw;
    $("log").appendChild(div);
    while ($("log").childNodes.length > 200) $("log").removeChild($("log").firstChild);
    $("log").scrollTop = $("log").scrollHeight;
  }

  function renderGames() {
    const ul = $("games");
    ul.innerHTML = "";
    if (!games.size) { ul.innerHTML = '<li><span class="muted">No games listed yet.</span></li>'; return; }
    for (const g of games.values()) {
      const li = document.createElement("li");
      const name = document.createElement("span");
      name.textContent = g.title + " ";
      const small = document.createElement("small");
      small.textContent = g.stored ? g.id : "built in";
      name.appendChild(small);
      const play = document.createElement("button");
      play.className = "small"; play.textContent = "Play";
      play.onclick = () => command(`play ${g.id}`);
      li.append(name, play);
      if (g.stored) {
        const del = document.createElement("button");
        del.className = "small"; del.textContent = "Delete";
        del.onclick = async () => { if (confirm(`Delete "${g.title}" from the TinyGame? This can't be undone.`)) await command(`rm ${g.id}`); };
        li.append(del);
      }
      ul.appendChild(li);
    }
  }

  async function refreshGames() { games.clear(); renderGames(); await command("ls"); }

  let awaitingStore = null;
  function handleLine(raw) {
    const j = tryJson(raw);
    if (!j) return;
    log(j, raw);
    if (j.t === "boot" && !j.safe_mode) { games.clear(); renderGames(); }
    if (j.t === "shell") { games.set(j.id, j); renderGames(); }
    if (j.t === "deleted") refreshGames();
    if (j.t === "stored" && awaitingStore && j.id === awaitingStore) {
      awaitingStore = null;
      $("sendstatus").textContent = "Sent. It's playing on your TinyGame.";
      showCheck({ ok: true }, "board");
    }
    if (j.t === "error" && ["bug", "stuck", "memory"].includes(j.kind)) showCheck({ ok: false, error: j }, "board");
    if (j.t === "error" && j.kind === "upload") $("sendstatus").textContent = "The TinyGame didn't accept it: " + j.msg;
  }

  async function readLoop() {
    const decoder = new TextDecoder();
    let buf = "";
    while (port && port.readable) {
      const reader = port.readable.getReader();
      try {
        for (;;) {
          const { value, done } = await reader.read();
          if (done) break;
          buf += decoder.decode(value, { stream: true });
          let i;
          while ((i = buf.indexOf("\n")) >= 0) { handleLine(buf.slice(0, i).trim()); buf = buf.slice(i + 1); }
        }
      } catch (e) {
        break;   // unplugged
      } finally {
        reader.releaseLock();
      }
    }
    disconnect();
  }

  async function connect() {
    try {
      port = await navigator.serial.requestPort({ filters: [{ usbVendorId: 0x303a }] });   // Espressif
      await port.open({ baudRate: 115200 });
      try { await port.setSignals({ dataTerminalReady: false, requestToSend: false }); } catch (e) {}
      writer = port.writable.getWriter();
      setConnected(true);
      readLoop();
      setTimeout(refreshGames, 1500);   // the board may reset when the port opens; give it a moment
    } catch (e) {
      port = null;
      if (e.name !== "NotFoundError") $("sendstatus").textContent = "Couldn't connect: " + e.message;
    }
  }

  async function disconnect() {
    const p = port;
    port = null;
    try { if (writer) { writer.releaseLock(); writer = null; } } catch (e) {}
    try { if (p) await p.close(); } catch (e) {}
    setConnected(false);
  }

  $("connect").addEventListener("click", () => (port ? disconnect() : connect()));
  if ("serial" in navigator) navigator.serial.addEventListener("disconnect", () => disconnect());

  $("send").addEventListener("click", async () => {
    const bytes = new TextEncoder().encode($("src").value);
    const id = $("id").value;
    awaitingStore = id;
    $("sendstatus").textContent = "Sending…";
    await sendBytes(new TextEncoder().encode(`cart ${bytes.length} ${id}\n`));
    await sendBytes(bytes);
    setTimeout(() => {
      if (awaitingStore === id) { awaitingStore = null; $("sendstatus").textContent = "No answer from the TinyGame. Is it on and connected?"; }
    }, 8000);
  });

  setConnected(false);
  refresh();
})();
