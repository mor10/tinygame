// Run after tools/sim_build.py: node sim/arduino/test_engine.mjs
import assert from "node:assert/strict";
import fs from "node:fs";
import createTinyGame from "../web/build/tinygame.js";

let now = 1000;
let scheduled = [];
globalThis.performance = { now: () => now };
globalThis.requestAnimationFrame = callback => {
  scheduled.push(callback);
  return scheduled.length;
};
globalThis.cancelAnimationFrame = () => {};
const saved = new Map();
globalThis.localStorage = {
  getItem: key => saved.get(key) ?? null,
  setItem: (key, value) => saved.set(key, value),
};
const wasmBinary = fs.readFileSync(new URL("../web/build/tinygame.wasm", import.meta.url));

function advance(ms = 96) {
  for (let elapsed = 0; elapsed < ms; elapsed += 16) {
    now += 16;
    const callbacks = scheduled;
    scheduled = [];
    callbacks.forEach(callback => callback(now));
  }
}

async function boot() {
  const serial = [];
  const wake = [];
  const engine = await createTinyGame({
    wasmBinary,
    onSerial: text => serial.push(text),
    onWake: side => wake.push(side),
  });
  advance();
  assert.match(serial.join(""), /ready, 4 games/);
  assert.match(serial.join(""), /\[BOOT\] OS [0-9a-f]{12}/);
  const framebuffer = engine._tg_sim_framebuffer();
  assert(framebuffer > 0);
  assert(engine.HEAPU8.subarray(framebuffer, framebuffer + 320 * 172 * 2)
    .some(byte => byte !== 0));
  return { engine, serial, wake };
}

function command(engine, text) {
  assert.equal(engine.ccall("tg_sim_command", "number", ["string"], [text]), 1);
  advance();
}

function click(engine, side = 0) {
  engine._tg_sim_button(side, 1);
  advance(64);
  engine._tg_sim_button(side, 0);
  advance(64);
}

function hold(engine, side = 1) {
  engine._tg_sim_button(side, 1);
  advance(704);
  engine._tg_sim_button(side, 0);
  advance(64);
}

function sleepChord(engine) {
  engine._tg_sim_button(0, 1);
  advance(320);
  engine._tg_sim_button(1, 1);
  advance(3104);
}

function releaseChord(engine) {
  engine._tg_sim_button(0, 0);
  advance(80);
  engine._tg_sim_button(1, 0);
  advance(80);
}

let { engine, serial, wake } = await boot();
assert.doesNotMatch(serial.join(""), /\[STORAGE\] error/);
assert.equal(engine._tg_sim_brightness(), 90);
assert.equal(engine._tg_sim_sleeping(), 0);
click(engine);
assert.match(serial.join(""), /screen=LAUNCHER/);
engine._tg_sim_turn(1, 1000); // Clamp to System regardless of installed game count.
advance();
click(engine);
assert.match(serial.join(""), /screen=SYSTEM/);
click(engine);
assert.match(serial.join(""), /screen=SETTINGS/);
click(engine); // Edit brightness.
engine._tg_sim_turn(1, -4);
advance();
assert.equal(engine._tg_sim_brightness(), 80);

// The global chord must beat the 650 ms pause/back events, including in an edit.
sleepChord(engine);
assert.match(serial.join(""), /sleep confirmation=open/);
assert.equal(engine._tg_sim_sleeping(), 0);
advance(4000); // Holding longer cannot confirm or retrigger.
assert.equal(engine._tg_sim_sleeping(), 0);
releaseChord(engine);
assert.equal(engine._tg_sim_sleeping(), 0); // Release clicks cannot confirm Yes.
engine._tg_sim_turn(1, 4); // No.
advance();
click(engine);
assert.match(serial.join(""), /sleep confirmation=closed/);
assert.equal(engine._tg_sim_brightness(), 80);
engine._tg_sim_turn(1, 4); // Still editing brightness, not navigating settings.
advance();
assert.equal(engine._tg_sim_brightness(), 90);
engine._tg_sim_turn(1, -4);
advance();
click(engine); // Confirm the edit.
assert.deepEqual(JSON.parse(saved.get("tinygame.sim.settings.v3")), [80, 50, 0, 1, 1]);

engine._tg_sim_turn(1, 12);
advance();
click(engine); // Edit auto-off.
engine._tg_sim_turn(1, -4);
advance();
click(engine);
assert.deepEqual(JSON.parse(saved.get("tinygame.sim.settings.v3")), [80, 50, 0, 0, 1]);
advance(300_500);
assert.equal(engine._tg_sim_sleeping(), 0);
click(engine);
engine._tg_sim_turn(1, 4);
advance();
click(engine); // Re-enable.
engine._tg_sim_turn(1, 4);
advance();
click(engine); // Edit timeout.
for (const [transitions, index] of [[-4, 0], [4, 1], [4, 2], [4, 3], [4, 4]]) {
  engine._tg_sim_turn(1, transitions);
  advance();
  click(engine); // Confirm each choice through the shared OS.
  assert.equal(JSON.parse(saved.get("tinygame.sim.settings.v3"))[4], index);
  click(engine);
}
engine._tg_sim_turn(1, -12);
advance();
click(engine); // Restore five-minute default.
assert.deepEqual(JSON.parse(saved.get("tinygame.sim.settings.v3")), [80, 50, 0, 1, 1]);

// Store scores through the real HAL data layout, then confirm a reboot keeps them.
saved.set("tinygame.sim.best.maze", JSON.stringify([0x78, 0x56, 0x34, 0x12]));
engine._tg_sim_stop();
({ engine, serial, wake } = await boot());
assert.equal(engine._tg_sim_brightness(), 80);
for (const id of ["brickbreaker", "maze", "rockrain"]) {
  command(engine, `launch ${id}`);
  assert.match(serial.join(""), new RegExp(`start ${id}`));
  engine._tg_sim_turn(0, -8);
  engine._tg_sim_turn(1, 8);
  engine._tg_sim_tilt(0.4, -0.2);
  advance();
}
command(engine, "log on");
advance(1100);
assert.match(serial.join(""), /tilt=\+0\.40,-0\.20/);
assert.match(serial.join(""), /physical total=-24/);
assert.match(serial.join(""), /physical total=24/);
hold(engine); // Either physical knob pauses via the shared input logic.
assert.match(serial.join(""), /screen=PAUSE/);
hold(engine, 0);
assert.match(serial.join(""), /screen=GAME/);
assert(engine._tg_sim_led() >= 0 && engine._tg_sim_led() <= 0xffffff);
const startsBeforeSleep = serial.filter(line => line.includes("[APP] start ")).length;
const beforeChord = serial.length;
sleepChord(engine);
assert.match(serial.slice(beforeChord).join(""), /sleep confirmation=open/);
assert.doesNotMatch(serial.slice(beforeChord).join(""), /screen=PAUSE/);
releaseChord(engine);
engine._tg_sim_turn(1, 4);
advance();
click(engine); // Cancel returns to the same running game, without restarting it.
assert.equal(engine._tg_sim_sleeping(), 0);
assert.equal(serial.filter(line => line.includes("[APP] start ")).length, startsBeforeSleep);
hold(engine);
assert.match(serial.slice(beforeChord).join(""), /screen=PAUSE/);
click(engine); // Resume still works after cancel.
command(engine, "info");
assert.match(serial.join(""), /\[DEV\] ok\n/);

// Browser dev commands aren't a USB host; tilt doesn't reset the idle timer.
engine._tg_sim_tilt(-0.8, 0.3);
advance(300_500);
assert.equal(engine._tg_sim_sleeping(), 1);
assert.equal(engine._tg_sim_brightness(), 0);
assert.equal(engine._tg_sim_led(), 0);
assert.equal(engine.ccall("tg_sim_command", "number", ["string"], ["info"]), 0);
engine._tg_sim_turn(0, 4);
assert.deepEqual(wake, []);
engine._tg_sim_button(1, 1);
engine._tg_sim_button(1, 1);
assert.deepEqual(wake, [1]);
engine._tg_sim_stop();

({ engine, serial } = await boot());
assert.equal(engine._tg_sim_sleeping(), 0);
assert.equal(engine._tg_sim_brightness(), 80);
assert.doesNotMatch(serial.join(""), /start (maze|brickbreaker|rockrain)/);
sleepChord(engine); // Welcome supports the same confirmation, with Yes default.
releaseChord(engine);
assert.equal(engine._tg_sim_sleeping(), 0);
click(engine);
advance(256);
assert.equal(engine._tg_sim_sleeping(), 1);
engine._tg_sim_stop();
({ engine, serial } = await boot());
assert.equal(typeof engine.pauseMainLoop, "function");
engine.pauseMainLoop();
advance();
engine.resumeMainLoop();
advance();
engine._tg_sim_stop();

saved.delete("tinygame.sim.settings.v3");
saved.set("tinygame.sim.settings.v2", "[70, 25, 1]");
({ engine, serial } = await boot());
assert.equal(engine._tg_sim_brightness(), 70); // Version 2 settings migrate.
assert.doesNotMatch(serial.join(""), /\[STORAGE\] error/);
engine._tg_sim_stop();
saved.delete("tinygame.sim.settings.v2");

saved.set("tinygame.sim.settings.v3", "[90, 50, 0, 1, -1]");
({ engine, serial } = await boot());
assert.equal(engine._tg_sim_brightness(), 90);
assert.match(serial.join(""), /\[STORAGE\] error loading .*invalid byte array/);
engine._tg_sim_stop();

saved.set("tinygame.sim.settings.v3", "{broken JSON");
({ engine, serial } = await boot());
assert.match(serial.join(""), /\[STORAGE\] error loading .*SyntaxError/);
engine._tg_sim_stop();

saved.set("tinygame.sim.settings.v3", "[255, 50, 0, 255, 255]");
({ engine, serial } = await boot());
assert.equal(engine._tg_sim_brightness(), 90);
assert.match(serial.join(""), /\[STORAGE\] error loading .*values out of range/);
engine._tg_sim_stop();

const originalStorage = globalThis.localStorage;
globalThis.localStorage = {
  getItem: () => { throw new Error("storage blocked"); },
  setItem: () => { throw new Error("storage quota"); },
};
({ engine, serial } = await boot());
assert.match(serial.join(""), /\[STORAGE\] error loading /);
click(engine);
engine._tg_sim_turn(1, 1000);
advance();
click(engine);
click(engine);
click(engine);
engine._tg_sim_turn(1, -4);
advance();
click(engine);
assert.match(serial.join(""), /\[STORAGE\] error saving /);
assert.match(serial.join(""), /settings save failed/);
engine._tg_sim_stop();
globalThis.localStorage = originalStorage;
console.log("PASS: firmware boot, all games, RGB565 framebuffer, physical input/hold, " +
  "serial, settings persistence/errors, idle sleep, wake reset, and loop controls.");
