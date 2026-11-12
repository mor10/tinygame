import assert from "node:assert/strict";
import fs from "node:fs";
import createTinyGame from "../web/build-169/tinygame.js";
import { assertClippedText, assertTransparentArea, replaceMember } from "./bundle_test_helpers.mjs";

let now = 1000;
let scheduled = [];
globalThis.performance = { now: () => now };
globalThis.requestAnimationFrame = callback => { scheduled.push(callback); return scheduled.length; };
globalThis.cancelAnimationFrame = () => {};
const saved = new Map();
globalThis.localStorage = {
  getItem: key => saved.get(key) ?? null,
  setItem: (key, value) => saved.set(key, value),
  key: index => [...saved.keys()][index] ?? null,
  get length() { return saved.size; },
};
const serial = [];
const wasmBinary = fs.readFileSync(new URL("../web/build-169/tinygame.wasm", import.meta.url));
let engine = await createTinyGame({ wasmBinary, onSerial: text => serial.push(text) });
function advance(ms = 96) {
  for (let elapsed = 0; elapsed < ms; elapsed += 16) {
    now += 16;
    const callbacks = scheduled;
    scheduled = [];
    callbacks.forEach(callback => callback(now));
  }
}
function command(text) {
  assert.equal(engine.ccall("tg_sim_command", "number", ["string"], [text]), 1);
  advance();
}
advance();
assert.equal(engine._tg_sim_width(), 280);
assert.equal(engine._tg_sim_height(), 240);
assert.match(serial.join(""), /\[LCD\] Simulator RGB565 280x240 initialized/);
assert.match(serial.join(""), /\[BOOT\] ready/);
command("install 1 capabilities");
assert.match(serial.join(""), /\[DEV\] install-capabilities 1 profile=169 game-api=1\n\[DEV\] ok\n/);
assert.equal(engine._tg_sim_led(), 0);
const address = engine._tg_sim_framebuffer();
assert(address > 0);
assert(engine.HEAPU8.subarray(address, address + 280 * 240 * 2).some(byte => byte !== 0));
command("launch knob_dodge");
assert.match(serial.join(""), /\[APP\] start knob_dodge/);
engine._tg_sim_turn(1, 20);
advance(200);
assert.doesNotMatch(serial.join(""), /Lua game stopped/);
engine._tg_sim_button(0, 1); engine._tg_sim_button(1, 1);
advance(80);
engine._tg_sim_button(0, 0); engine._tg_sim_button(1, 0);
advance(80);
assert.match(serial.join(""), /\[APP\] screen=PAUSE/);
const pixels = new Uint16Array(engine.HEAPU8.buffer, address, 280 * 240);
const at = (x, y) => pixels[y * 280 + x];
assert.equal(at(26, 56), at(253, 56));
assert.notEqual(at(26, 56), at(20, 56));
engine._tg_sim_button(0, 1); advance(80);
engine._tg_sim_button(0, 0); advance(80);
assert.match(serial.join(""), /\[APP\] screen=GAME/);
engine._tg_sim_stop();

const bundleBytes = Buffer.from(fs.readFileSync(
  new URL("../../tests/fixtures/tgb_v1_lua_tick.hex", import.meta.url), "utf8")
  .replace(/\s/g, ""), "hex");
async function renderRules(rules) {
  engine = await createTinyGame({ wasmBinary, onSerial: text => serial.push(text) });
  advance();
  const bundle = replaceMember(bundleBytes, "rules.lua", rules);
  const pointer = engine._malloc(bundle.length);
  assert(pointer > 0);
  engine.HEAPU8.set(bundle, pointer);
  assert.equal(engine._tg_sim_install_bundle(pointer, bundle.length), 0);
  engine._free(pointer);
  engine._tg_sim_stop();
  engine = await createTinyGame({ wasmBinary, onSerial: text => serial.push(text) });
  advance();
  command("launch lua_tick");
  advance(200);
  const result = Uint16Array.from(
    new Uint16Array(engine.HEAPU8.buffer, engine._tg_sim_framebuffer(), 280 * 240));
  engine._tg_sim_stop();
  return result;
}
const background = "tg.rect(0,0,input.screen.width,input.screen.height,7)";
const transparent = await renderRules(
  `function update(dt,input) ${background} tg.rect(0,0,20,20,0) tg.text(1,1,'TEST',0) end`);
assertTransparentArea(transparent, 280, 20, 12);
const origin = await renderRules(`function update(dt,input) ${background} tg.text(0,0,'ABCD',8) end`);
const clipped = await renderRules(`function update(dt,input) ${background} tg.text(-4,-2,'ABCD',8) end`);
assertClippedText(origin, clipped, 280, 20, 12);
console.log("PASS: 1.69 WASM profile, native dimensions, Lua game and Pause/resume.");
