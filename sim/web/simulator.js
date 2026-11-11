// The simulator page: runs the WebAssembly engine at 60 ticks per second and
// shows its frame on the canvas.

import { BOARDS, BUNDLE_OK, BUNDLE_UNSUPPORTED, Engine, FixedStep } from "./engine.mjs";
import { LEFT, RIGHT, TRANSITIONS_PER_TURN, Rotation, SimInput, angleAt, angleDelta } from "./input.mjs";

const $ = id => document.getElementById(id);
const screen = $("screen"), context = screen.getContext("2d");
const knobs = [...document.querySelectorAll(".knob")];
const input = new SimInput();

let engine = null;            // set when the WebAssembly has loaded
let board = BOARDS.a, view = "standard";
let image = null;             // ImageData the size of the current board
let clock = new FixedStep();
let lastMicros = null;        // time of the previous animation frame
let ticks = 0, framesThisSecond = 0, fps = 0, fpsSince = 0;

// RGB565 -> the 32-bit pixel canvas wants, worked out once. (Rounds the same
// way as tools/tg.py so screenshots match the PNGs it writes.)
const RGBA = new Uint32Array(65536);
{
  const bytes = new Uint8Array(RGBA.buffer);
  for (let value = 0; value < 65536; value++) {
    bytes[value * 4] = Math.floor(((value >> 11) * 255 + 15) / 31);
    bytes[value * 4 + 1] = Math.floor((((value >> 5) & 63) * 255 + 31) / 63);
    bytes[value * 4 + 2] = Math.floor(((value & 31) * 255 + 15) / 31);
    bytes[value * 4 + 3] = 255;
  }
}

function showError(message) {
  $("error").hidden = false;
  $("error").textContent = String(message);
}

function showStatus() {
  $("status").textContent = engine
    ? `${board.name} ${board.width}×${board.height} · ${view} view · tick ${ticks} · ${fps} frames per second`
    : "Starting…";
  // For tests and tools: the same facts without parsing the sentence.
  Object.assign(document.body.dataset, { board: board.id, view, ticks, fps, ready: String(Boolean(engine)) });
}

// The biggest whole-number zoom that fits the window (at least 1, at most 4).
function fitZoom() {
  const room = Math.min((window.innerWidth - 90) / board.width, (window.innerHeight * 0.6) / board.height);
  const zoom = Math.max(1, Math.min(4, Math.floor(room)));
  screen.style.width = `${board.width * zoom}px`;
  screen.style.height = `${board.height * zoom}px`;
  document.body.dataset.zoom = zoom;
}

function drawFrame() {
  const pixels = engine.draw();
  const out = new Uint32Array(image.data.buffer);
  for (let i = 0; i < pixels.length; i++) out[i] = RGBA[pixels[i]];
  context.putImageData(image, 0, 0);
}

// Starts the engine again for the chosen board and view.
function restart() {
  for (const button of document.querySelectorAll("[data-board]")) {
    button.setAttribute("aria-pressed", String(button.dataset.board === board.id));
  }
  for (const button of document.querySelectorAll("[data-view]")) {
    button.setAttribute("aria-pressed", String(button.dataset.view === view));
  }
  screen.width = board.width;
  screen.height = board.height;
  image = context.createImageData(board.width, board.height);
  fitZoom();
  ticks = 0;
  clock = new FixedStep();
  lastMicros = null;
  if (!engine) return;
  try {
    engine.init(board.width, board.height, view);
    $("error").hidden = true;
    drawFrame();
  } catch (error) {
    showError(error.message);
  }
  showStatus();
}

function frame(timeMs) {
  requestAnimationFrame(frame);
  if (!engine) return;
  // Whole microseconds, measured from absolute times so nothing drifts.
  const nowMicros = Math.round(timeMs * 1000);
  const elapsed = lastMicros === null ? 0 : Math.max(0, nowMicros - lastMicros);
  lastMicros = nowMicros;
  const steps = clock.advanceMicros(elapsed);
  for (let i = 0; i < steps; i++) engine.tick(input.takeTick());
  ticks += steps;
  drawFrame();
  showControls();

  framesThisSecond++;
  if (timeMs - fpsSince >= 1000) {
    fps = Math.round(framesThisSecond * 1000 / (timeMs - fpsSince));
    framesThisSecond = 0;
    fpsSince = timeMs;
  }
  showStatus();
}

// Makes the on-screen knobs and tilt dot follow the input.
function showControls() {
  knobs.forEach((knob, side) => {
    knob.querySelector(".marker").style.transform =
      `rotate(${input.total[side] * 360 / TRANSITIONS_PER_TURN}deg)`;
    knob.classList.toggle("pressed", input.isDown(side));
    knob.setAttribute("aria-pressed", String(input.isDown(side)));
  });
  const [x, y] = input.tilt();
  $("tilt-dot").style.transform = `translate(${x * 12}px, ${y * 12}px)`;
}

// ---- keyboard ----
document.addEventListener("keydown", event => {
  if (event.ctrlKey || event.metaKey || event.altKey) return;
  if (input.keyDown(event.key)) event.preventDefault();
});
document.addEventListener("keyup", event => {
  if (input.keyUp(event.key)) event.preventDefault();
});
window.addEventListener("blur", () => input.releaseAll());

// ---- on-screen knobs ----
knobs.forEach((knob, side) => {
  let drag = null;   // { angle, rotation } while the rim is being dragged
  knob.addEventListener("pointerdown", event => {
    event.preventDefault();
    knob.focus();
    knob.setPointerCapture(event.pointerId);
    if (event.target.classList.contains("centre")) {
      input.press(side, "mouse", true);
    } else {
      drag = { angle: angleAt(event.clientX, event.clientY, knob.getBoundingClientRect()), rotation: new Rotation() };
    }
  });
  knob.addEventListener("pointermove", event => {
    if (!drag) return;
    const angle = angleAt(event.clientX, event.clientY, knob.getBoundingClientRect());
    input.turn(side, drag.rotation.move(angleDelta(drag.angle, angle)));
    drag.angle = angle;
  });
  const release = () => { drag = null; input.press(side, "mouse", false); };
  knob.addEventListener("pointerup", release);
  knob.addEventListener("pointercancel", release);
  knob.addEventListener("wheel", event => {
    event.preventDefault();
    input.turn(side, event.deltaY > 0 ? 4 : -4);
  }, { passive: false });
  // With the knob focused, Space or Enter clicks it.
  knob.addEventListener("keydown", event => {
    if (event.key === " " || event.key === "Enter") { event.preventDefault(); input.press(side, "focus", true); }
  });
  knob.addEventListener("keyup", event => {
    if (event.key === " " || event.key === "Enter") input.press(side, "focus", false);
  });
  knob.addEventListener("blur", () => input.press(side, "focus", false));
});

// ---- board and view switches ----
for (const button of document.querySelectorAll("[data-board]")) {
  button.addEventListener("click", () => { board = BOARDS[button.dataset.board]; restart(); });
}
for (const button of document.querySelectorAll("[data-view]")) {
  button.addEventListener("click", () => { view = button.dataset.view; restart(); });
}
window.addEventListener("resize", fitZoom);

// ---- game bundle from the server (sim/server.mjs) ----
// The server says when the bundle file changed; fetch it and hand it to the
// engine. The WebAssembly itself is not rebuilt or reloaded.
async function loadBundle() {
  if (!engine) return;
  const notice = $("notice");
  try {
    const response = await fetch("bundle", { cache: "no-store" });
    if (!response.ok) throw new Error(`The server has no bundle (${response.status})`);
    const bytes = new Uint8Array(await response.arrayBuffer());
    const answer = engine.loadBundle(bytes);
    document.body.dataset.bundleBytes = bytes.length;
    document.body.dataset.bundleLoads = Number(document.body.dataset.bundleLoads || 0) + 1;
    notice.hidden = false;
    if (answer === BUNDLE_OK) {
      notice.textContent = `Game loaded (${bytes.length} bytes).`;
      restart();
    } else if (answer === BUNDLE_UNSUPPORTED) {
      notice.textContent = `Got a game bundle (${bytes.length} bytes), but games cannot run here yet. Showing the demo.`;
    } else {
      notice.textContent = "The engine could not read that game bundle.";
    }
  } catch (error) {
    notice.hidden = false;
    notice.textContent = `Could not load the game bundle: ${error.message}`;
  }
}

function listenToServer() {
  if (!window.EventSource || location.protocol === "file:") return;
  const events = new EventSource("events");
  events.addEventListener("bundle", () => loadBundle());
}

// ---- start ----
async function start() {
  restart();
  showStatus();
  try {
    const { default: createTinyGame } = await import("./build/tinygame.js");
    const module = await createTinyGame({
      locateFile: path => new URL(`build/${path}`, document.baseURI).href,
      printErr: text => showError(text),
      onAbort: reason => showError(`The engine stopped: ${reason}`),
    });
    engine = new Engine(module);
  } catch (error) {
    showError(`The simulator engine did not load: ${error.message}\nBuild it with: python3 tools/sim_build.py`);
    return;
  }
  restart();
  listenToServer();
  requestAnimationFrame(frame);
}
start();
