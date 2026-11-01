import { KEY_TURNS, KEY_BUTTONS, angleDelta, angleAt, Rotation, arrowTilt, DeviceInput } from "./input.mjs";

const $ = id => document.getElementById(id);
const screen = $("screen"), context = screen.getContext("2d");
const image = context.createImageData(320, 172);
const knobs = [...document.querySelectorAll(".knob")];
const buttons = [new Set(), new Set()], angles = [0, 0], keys = new Set();
const deviceInput = new DeviceInput();
let firmware, booting = false, pendingAck, serialBuffer = "", serialBatch = [];
let commandChain = Promise.resolve(), deviceConnected = false, tiltDrag = null;
const repeats = new Map();

function error(message) {
  $("error").hidden = false;
  $("error").textContent = String(message);
}
function serial(text) {
  serialBuffer += text;
  let end;
  while ((end = serialBuffer.indexOf("\n")) !== -1) {
    const line = serialBuffer.slice(0, end).replace(/\r$/, "");
    serialBuffer = serialBuffer.slice(end + 1);
    serialBatch.push(line);
    const lines = ($("log").textContent + line + "\n").split("\n");
    $("log").textContent = lines.slice(-300).join("\n");
    if (line === "[DEV] ok") pendingAck?.resolve();
    else if (line.startsWith("[DEV] error")) pendingAck?.reject(new Error(line));
  }
}
async function api(path, data) {
  const response = await fetch(path, data === undefined ? {} : {
    method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(data),
  });
  const result = await response.json();
  if (!response.ok) throw new Error(result.error || response.statusText);
  return result;
}
function turn(side, delta) {
  if (!firmware || booting || !delta || firmware._tg_sim_sleeping()) return;
  firmware._tg_sim_turn(side, delta);
  angles[side] += delta * 360 / 80;
  knobs[side].querySelector(".marker").style.transform = `rotate(${angles[side]}deg)`;
}
function button(side, down, source) {
  const before = buttons[side].size > 0;
  if (down) buttons[side].add(source); else buttons[side].delete(source);
  const after = buttons[side].size > 0;
  knobs[side].classList.toggle("pressed", after);
  knobs[side].setAttribute("aria-pressed", String(after));
  if (before === after || !firmware || booting) return;
  if (after && firmware._tg_sim_sleeping()) { boot().catch(error); return; }
  firmware._tg_sim_button(side, Number(after));
}
function tilt(x, y) {
  firmware?._tg_sim_tilt(x, y);
  $("tilt-dot").style.transform = `translate(${x * 12}px, ${y * 12}px)`;
}
function command(text) {
  const run = async () => {
    if (!firmware || booting) throw new Error("Firmware is not ready");
    const module = firmware;
    if (text.startsWith("wait ")) {
      const seconds = Number(text.slice(5));
      if (!Number.isFinite(seconds) || seconds < 0 || seconds > 60) throw new Error("Wait must be 0..60 seconds");
      await new Promise(resolve => setTimeout(resolve, seconds * 1000));
      return;
    }
    if (/[\r\n\0]/.test(text) || text.length > 240) throw new Error("Send one dev command per item (max 240 characters)");
    const encoded = new TextEncoder().encode(text + "\0");
    const ptr = module._malloc(encoded.length);
    if (!ptr) throw new Error("Firmware command allocation failed");
    try {
      await new Promise((resolve, reject) => {
        const timer = setTimeout(() => { pendingAck = null; reject(new Error(`No firmware acknowledgement: ${text}`)); }, 5000);
        pendingAck = {
          resolve: () => { clearTimeout(timer); pendingAck = null; resolve(); },
          reject: e => { clearTimeout(timer); pendingAck = null; reject(e); },
        };
        module.HEAPU8.set(encoded, ptr);
        module._tg_sim_command(ptr);
      });
      await new Promise(resolve => setTimeout(resolve, 60));
    } finally { module._free(ptr); }
  };
  const result = commandChain.then(run);
  commandChain = result.catch(() => {});
  return result;
}
async function boot() {
  if (booting) return;
  booting = true;
  $("status").textContent = "Starting firmware...";
  try {
    firmware?.pauseMainLoop();
    pendingAck?.reject(new Error("Firmware restarted"));
    for (const side of [0, 1]) buttons[side].clear();
    knobs.forEach(knob => knob.classList.remove("pressed"));
    const { default: createTinyGame } = await import("./build/tinygame.js");
    firmware = await createTinyGame({
      locateFile: path => new URL(`build/${path}`, document.baseURI).href,
      onSerial: serial,
      print: text => serial(text + "\n"),
      printErr: text => { serial(text + "\n"); error(text); },
      onAbort: reason => error(`Firmware stopped: ${reason}`),
    });
    deviceInput.reset();
    $("error").hidden = true;
    $("status").textContent = "Ready";
  } finally { booting = false; }
}
function renderFrame() {
  if (firmware && !booting) {
    const address = firmware._tg_sim_framebuffer();
    const pixels = new Uint16Array(firmware.HEAPU8.buffer, address, 320 * 172);
    for (let i = 0; i < pixels.length; i++) {
      const v = pixels[i], p = i * 4;
      image.data[p] = Math.round(((v >> 11) & 31) * 255 / 31);
      image.data[p + 1] = Math.round(((v >> 5) & 63) * 255 / 63);
      image.data[p + 2] = Math.round((v & 31) * 255 / 31);
      image.data[p + 3] = 255;
    }
    context.putImageData(image, 0, 0);
    $("sleep").hidden = !firmware._tg_sim_sleeping();
    screen.style.opacity = String(firmware._tg_sim_brightness() / 100);
    const rgb = firmware._tg_sim_led();
    $("led").style.backgroundColor = `#${(rgb >>> 0).toString(16).padStart(6, "0")}`;
  }
}
function render() { renderFrame(); requestAnimationFrame(render); }
function releaseLocal() {
  for (const timer of repeats.values()) clearInterval(timer);
  repeats.clear(); keys.clear(); tiltDrag = null;
  for (const side of [0, 1]) {
    button(side, false, "keyboard"); button(side, false, "mouse"); button(side, false, "focus");
  }
  if (!deviceConnected) tilt(0, 0);
}
document.addEventListener("keydown", event => {
  if (event.target instanceof HTMLInputElement || event.ctrlKey || event.metaKey || event.altKey) return;
  const key = event.key.toLowerCase();
  if (KEY_TURNS[key]) {
    event.preventDefault();
    if (repeats.has(key)) return;
    const [side, delta] = KEY_TURNS[key];
    turn(side, delta);
    let ticks = 0;
    repeats.set(key, setInterval(() => { if (++ticks >= 4) turn(side, delta); }, 80));
  } else if (key in KEY_BUTTONS) {
    event.preventDefault(); button(KEY_BUTTONS[key], true, "keyboard");
  } else if (event.key.startsWith("Arrow")) {
    event.preventDefault(); keys.add(event.key); tilt(...arrowTilt(keys));
  }
});
document.addEventListener("keyup", event => {
  const key = event.key.toLowerCase();
  clearInterval(repeats.get(key)); repeats.delete(key);
  if (key in KEY_BUTTONS) button(KEY_BUTTONS[key], false, "keyboard");
  if (event.key.startsWith("Arrow")) { keys.delete(event.key); tilt(...arrowTilt(keys)); }
});
window.addEventListener("blur", releaseLocal);
document.addEventListener("visibilitychange", () => { if (document.hidden) releaseLocal(); });
knobs.forEach((knob, side) => {
  let drag = null;
  knob.addEventListener("pointerdown", event => {
    if (event.button !== 0 || drag) return;
    event.preventDefault(); knob.focus(); knob.setPointerCapture(event.pointerId);
    const rect = knob.getBoundingClientRect();
    const centre = Math.hypot(event.clientX - rect.left - rect.width / 2, event.clientY - rect.top - rect.height / 2) < rect.width * .28;
    drag = { id: event.pointerId, centre, angle: angleAt(event.clientX, event.clientY, rect), rotation: new Rotation() };
    if (centre) button(side, true, "mouse");
  });
  knob.addEventListener("pointermove", event => {
    if (!drag || drag.id !== event.pointerId || drag.centre) return;
    const angle = angleAt(event.clientX, event.clientY, knob.getBoundingClientRect());
    turn(side, drag.rotation.move(angleDelta(drag.angle, angle)));
    drag.angle = angle;
  });
  const release = event => {
    if (!drag || drag.id !== event.pointerId) return;
    button(side, false, "mouse"); drag = null;
  };
  knob.addEventListener("pointerup", release);
  knob.addEventListener("pointercancel", release);
  knob.addEventListener("lostpointercapture", release);
  knob.addEventListener("wheel", event => {
    event.preventDefault(); turn(side, event.deltaY === 0 ? 0 : Math.sign(event.deltaY) * 4);
  }, { passive: false });
  knob.addEventListener("keydown", event => {
    if (event.key === " " || event.key === "Enter") { event.preventDefault(); button(side, true, "focus"); }
  });
  knob.addEventListener("keyup", event => {
    if (event.key === " " || event.key === "Enter") { event.preventDefault(); button(side, false, "focus"); }
  });
  knob.addEventListener("blur", () => button(side, false, "focus"));
  window.addEventListener("blur", () => { drag = null; });
});
screen.addEventListener("pointerdown", event => {
  if (event.button !== 0) return;
  screen.setPointerCapture(event.pointerId); tiltDrag = { id: event.pointerId, x: event.clientX, y: event.clientY };
});
screen.addEventListener("pointermove", event => {
  if (!tiltDrag || tiltDrag.id !== event.pointerId) return;
  const scale = screen.getBoundingClientRect().width / 4;
  tilt(Math.max(-1, Math.min(1, (event.clientX - tiltDrag.x) / scale)), Math.max(-1, Math.min(1, (event.clientY - tiltDrag.y) / scale)));
});
for (const name of ["pointerup", "pointercancel", "lostpointercapture"]) screen.addEventListener(name, () => { tiltDrag = null; tilt(...arrowTilt(keys)); });
$("command-form").addEventListener("submit", event => {
  event.preventDefault(); command($("command").value).catch(error); $("command").value = "";
});
$("reload").addEventListener("click", () => boot().catch(error));
$("screenshot").addEventListener("click", () => api("screenshot", { png: screen.toDataURL("image/png") }).then(result => {
  $("status").textContent = "Screenshot saved"; serial(`Saved ${result.path}\n`);
}).catch(error));
$("device").addEventListener("change", () => api($("device").checked ? "connect" : "disconnect", {}).catch(e => { $("device").checked = false; error(e); }));

const events = new EventSource("events");
events.addEventListener("device", event => {
  try {
    const input = deviceInput.apply(JSON.parse(event.data));
    input.turns.forEach((delta, side) => turn(side, delta));
    input.buttons.forEach((down, side) => button(side, down, "device"));
    tilt(...input.tilt);
  } catch (e) { error(e); }
});
events.addEventListener("connection", event => {
  const state = JSON.parse(event.data);
  deviceConnected = state.connected;
  $("device").checked = state.connected;
  $("device-status").textContent = state.message;
  if (!state.connected) {
    deviceInput.reset();
    for (const side of [0, 1]) button(side, false, "device");
    tilt(...arrowTilt(keys));
  }
});
events.addEventListener("reload", () => location.reload());
events.addEventListener("build", event => { $("status").textContent = JSON.parse(event.data).message; });
events.addEventListener("build-error", event => error(JSON.parse(event.data).message));
events.addEventListener("request", async event => {
  const { id, action, input } = JSON.parse(event.data);
  try {
    let result = {};
    if (action === "send") { for (const text of input.commands) await command(text); result = { sent: input.commands.length }; }
    else if (action === "launch") { await command(`launch ${input.game}`); result = { game: input.game }; }
    else if (action === "reload") { await boot(); result = { status: "ready" }; }
    else if (action === "screenshot") {
      if (!firmware || booting) throw new Error("Firmware is not ready");
      renderFrame(); result = { png: screen.toDataURL("image/png") };
    }
    else throw new Error(`Unknown simulator action: ${action}`);
    await api("reply", { id, result });
  } catch (e) { await api("reply", { id, error: String(e) }).catch(error); }
});
setInterval(() => {
  if (!firmware || booting) return;
  const lines = serialBatch; serialBatch = [];
  api("report", { lines, ready: true, sleeping: Boolean(firmware._tg_sim_sleeping()) }).catch(error);
}, 250);
events.onerror = () => { $("status").textContent = "Server disconnected; reconnecting..."; };
render();
boot().then(async () => {
  const state = await api("state");
  if (state.game) await command(`launch ${state.game}`);
}).catch(e => { $("status").textContent = "Firmware unavailable"; error(e); });
