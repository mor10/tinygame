// Browser simulator page: loads the WASM engine, runs a cart at its frame rate, and maps the
// keyboard, mouse and touch onto the two knobs. Serve the repository root over HTTP and open
// /sim/web/ (WASM can't load from file://).
(async () => {
  const logEl = document.getElementById("log");
  const statusEl = document.getElementById("status");
  const canvas = document.getElementById("screen");
  const ctx = canvas.getContext("2d");
  const image = ctx.createImageData(320, 172);

  function log(line) {
    let cls = "";
    try { const j = JSON.parse(line); if (j.t === "stats" || j.t === "hash") return; cls = j.t; } catch (e) {}
    const div = document.createElement("div");
    div.className = cls;
    div.textContent = line;
    logEl.appendChild(div);
    while (logEl.childNodes.length > 200) logEl.removeChild(logEl.firstChild);
    logEl.scrollTop = logEl.scrollHeight;
  }

  const M = await TinyGame({ print: log, printErr: log });
  const tgLoad = M.cwrap("tg_load", "number", ["number", "number", "number", "number"]);
  const tgFrame = M.cwrap("tg_frame", "number", ["number", "number", "number", "number"]);
  const tgRgba = M.cwrap("tg_rgba", "number", []);
  const tgFps = M.cwrap("tg_fps", "number", []);

  // ---------- input ----------
  // Each knob collects transitions (4 per notch) and click events until the next frame.
  const knobs = [0, 1].map(() => ({ delta: 0, down: false, pressed: false, released: false }));
  function press(i, down) {
    const k = knobs[i];
    if (down && !k.down) k.pressed = true;
    if (down && i === 1 && !running) load(source, "cart");   // right click restarts, as on the board
    if (!down && k.down) k.released = true;
    k.down = down;
    document.getElementById(i ? "dial-right" : "dial-left").classList.toggle("down", down);
  }
  function turn(i, notches) { knobs[i].delta += notches * 4; spin(i, notches); }
  const angle = [0, 0];
  function spin(i, notches) {
    angle[i] += notches * 18;   // 20 notches per turn
    document.getElementById(i ? "dial-right" : "dial-left").style.transform = `rotate(${angle[i]}deg)`;
  }

  const keyMap = { a: [0, -1], d: [0, 1], j: [1, -1], l: [1, 1] };
  const clickKey = { s: 0, k: 1 };
  addEventListener("keydown", (e) => {
    const k = e.key.toLowerCase();
    if (keyMap[k]) { turn(keyMap[k][0], keyMap[k][1]); e.preventDefault(); }
    if (k in clickKey && !e.repeat) { press(clickKey[k], true); e.preventDefault(); }
  });
  addEventListener("keyup", (e) => {
    const k = e.key.toLowerCase();
    if (k in clickKey) press(clickKey[k], false);
  });

  [0, 1].forEach((i) => {
    const dial = document.getElementById(i ? "dial-right" : "dial-left");
    let lastX = null, moved = 0;
    dial.addEventListener("pointerdown", (e) => { dial.setPointerCapture(e.pointerId); lastX = e.clientX; moved = 0; press(i, true); });
    dial.addEventListener("pointermove", (e) => {
      if (lastX === null) return;
      const steps = Math.trunc((e.clientX - lastX) / 6);
      if (steps) { turn(i, steps); lastX += steps * 6; moved += Math.abs(steps); }
      if (moved > 1 && knobs[i].down) press(i, false);   // a drag is a turn, not a click
    });
    const up = () => { lastX = null; if (knobs[i].down) press(i, false); };
    dial.addEventListener("pointerup", up);
    dial.addEventListener("pointercancel", up);
    dial.addEventListener("wheel", (e) => { e.preventDefault(); turn(i, e.deltaY > 0 ? 1 : -1); }, { passive: false });
  });

  // ---------- carts ----------
  let source = "", running = false;
  function load(text, name) {
    source = text;
    logEl.textContent = "";
    log(`loading ${name}`);
    const bytes = M.lengthBytesUTF8(text);
    const ptr = M.stringToNewUTF8(text);
    running = tgLoad(ptr, bytes, 0, (Math.random() * 1e9) >>> 0) === 0;
    M._free(ptr);
    draw();
    nextFrame = performance.now();
  }

  async function loadUrl(url) {
    const r = await fetch(url);
    if (!r.ok) { log(`{"t":"error","msg":"couldn't fetch ${url}"}`); return; }
    load(await r.text(), url.split("/").pop());
  }

  function loadFile(file) {
    if (!file) return;
    file.text().then((t) => load(t, file.name));
  }

  document.getElementById("builtin").addEventListener("change", (e) => loadUrl(e.target.value));
  document.getElementById("restart").addEventListener("click", () => load(source, "cart"));
  document.getElementById("file").addEventListener("change", (e) => loadFile(e.target.files[0]));
  addEventListener("dragover", (e) => { e.preventDefault(); document.body.classList.add("drop"); });
  addEventListener("dragleave", () => document.body.classList.remove("drop"));
  addEventListener("drop", (e) => { e.preventDefault(); document.body.classList.remove("drop"); loadFile(e.dataTransfer.files[0]); });
  canvas.addEventListener("click", () => { if (!running) load(source, "cart"); });   // like the restart click

  // ---------- loop ----------
  function draw() {
    image.data.set(M.HEAPU8.subarray(tgRgba(), tgRgba() + 320 * 172 * 4));
    ctx.putImageData(image, 0, 0);
  }

  let nextFrame = performance.now(), frames = 0, fpsWindow = performance.now();
  function tick(now) {
    requestAnimationFrame(tick);
    if (!running) { statusEl.textContent = "stopped: click the screen or Restart"; return; }
    const period = 1000 / tgFps();
    let steps = 0;
    while (now >= nextFrame && steps < 2 && running) {
      const bits = knobs.map((k) => (k.pressed ? 1 : 0) | (k.down ? 2 : 0) | (k.released ? 4 : 0));
      running = tgFrame(knobs[0].delta, knobs[1].delta, bits[0], bits[1]) === 0;
      knobs.forEach((k) => { k.delta = 0; k.pressed = false; k.released = false; });
      nextFrame += period;
      steps++;
      frames++;
    }
    if (now - nextFrame > 250) nextFrame = now;   // tab was hidden: don't race to catch up
    if (steps) draw();
    if (now - fpsWindow >= 1000) {
      statusEl.textContent = `${Math.round(frames * 1000 / (now - fpsWindow))} fps`;
      frames = 0;
      fpsWindow = now;
    }
  }

  await loadUrl(document.getElementById("builtin").value);
  requestAnimationFrame(tick);
})();
