// Run after tools/sim_build.py: node sim/arduino/test_engine.mjs
import assert from "node:assert/strict";
import fs from "node:fs";
import createTinyGame from "../web/build/tinygame.js";
import { assertClippedText, assertTransparentArea, crc32, replaceMember } from "./bundle_test_helpers.mjs";

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
  key: index => [...saved.keys()][index] ?? null,
  get length() { return saved.size; },
};
const wasmBinary = fs.readFileSync(new URL("../web/build/tinygame.wasm", import.meta.url));
const versionHeader = fs.readFileSync(new URL("../../include/tg_os_version.h", import.meta.url), "utf8");
const firmwareVersion = versionHeader.match(/^#define TG_FIRMWARE_VERSION "((?:0|[1-9]\d*)\.(?:0|[1-9]\d*)\.(?:0|[1-9]\d*))"$/m)?.[1];
assert(firmwareVersion, "Firmware uses a maintained MAJOR.MINOR.PATCH value");
const builtInGameIds = ["brickbreaker", "knob_dodge", "rockrain", "tiltblock"];

function advance(ms = 96) {
  for (let elapsed = 0; elapsed < ms; elapsed += 16) {
    now += 16;
    const callbacks = scheduled;
    scheduled = [];
    callbacks.forEach(callback => callback(now));
  }
}

function jump(ms) {
  now += ms;
  const callbacks = scheduled;
  scheduled = [];
  callbacks.forEach(callback => callback(now));
}

async function boot(gameIds = builtInGameIds) {
  const serial = [];
  const wake = [];
  const engine = await createTinyGame({
    wasmBinary,
    onSerial: text => serial.push(text),
    onWake: side => wake.push(side),
  });
  advance();
  assert.match(serial.join(""), new RegExp(`ready, ${gameIds.length} games`));
  assert.deepEqual(serial.filter(line => line.startsWith("[BOOT] game ")),
    gameIds.map(id => `[BOOT] game ${id}\n`));
  assert(serial.join("").includes(`[BOOT] Firmware ${firmwareVersion}\n`));
  assert.doesNotMatch(serial.join(""), /\[BOOT\] OS [0-9a-f]{12}/);
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

function installBundle(engine, bytes) {
  const pointer = engine._malloc(bytes.length);
  assert(pointer > 0);
  try {
    engine.HEAPU8.set(bytes, pointer);
    return engine._tg_sim_install_bundle(pointer, bytes.length);
  } finally {
    engine._free(pointer);
  }
}

function addOversizedRules(bundle) {
  const tableOffset = bundle.readUInt32LE(12);
  const tableLength = bundle.readUInt32LE(16);
  let position = tableOffset;
  let rulesOffset;
  let rulesLength;
  let rulesEntry;
  for (let i = 0; i < bundle.readUInt16LE(8); i++) {
    const nameLength = bundle.readUInt16LE(position);
    if (bundle.toString("ascii", position + 16, position + 16 + nameLength) === "rules.lua") {
      rulesOffset = bundle.readUInt32LE(position + 4);
      rulesLength = bundle.readUInt32LE(position + 8);
      rulesEntry = position;
      break;
    }
    position += 16 + ((nameLength + 3) & ~3);
  }
  assert.notEqual(rulesEntry, undefined);
  const extra = 64 * 1024;
  const end = rulesOffset + rulesLength;
  const result = Buffer.concat([bundle.subarray(0, end), Buffer.alloc(extra, 0x20),
    bundle.subarray(end)]);
  position = tableOffset;
  for (let i = 0; i < result.readUInt16LE(8); i++) {
    const nameLength = result.readUInt16LE(position);
    const offset = result.readUInt32LE(position + 4);
    if (offset > rulesOffset) result.writeUInt32LE(offset + extra, position + 4);
    position += 16 + ((nameLength + 3) & ~3);
  }
  result.writeUInt32LE(rulesLength + extra, rulesEntry + 8);
  result.writeUInt32LE(crc32(result.subarray(rulesOffset, end + extra)), rulesEntry + 12);
  result.writeUInt32LE(result.readUInt32LE(24) + extra, 24);
  result.writeUInt32LE(result.length, 28);
  result.writeUInt32LE(crc32(result.subarray(tableOffset, tableOffset + tableLength)), 32);
  return result;
}

function installViaProtocol(engine, bytes) {
  const checksum = value => crc32(value).toString(16).padStart(8, "0").toUpperCase();
  command(engine, `install 1 begin lua_tick ${bytes.length} ${checksum(bytes)}`);
  for (let offset = 0; offset < bytes.length; offset += 768) {
    const chunk = bytes.subarray(offset, Math.min(offset + 768, bytes.length));
    command(engine, `install 1 chunk ${offset} ${checksum(chunk)} ${chunk.toString("base64")}`);
  }
  command(engine, "install 1 finish");
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

function pauseChord(engine) {
  engine._tg_sim_button(0, 1);
  advance(80);
  engine._tg_sim_button(1, 1);
  advance(80);
  engine._tg_sim_button(0, 0);
  advance(80);
  engine._tg_sim_button(1, 0);
  advance(80);
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
command(engine, "install 1 capabilities");
assert.match(serial.join(""), /\[DEV\] install-capabilities 1 profile=147b game-api=1\n\[DEV\] ok\n/);
const installOwnershipStart = serial.length;
command(engine, "install 1 begin test 1 00000000");
command(engine, "stream on");
assert.match(serial.slice(installOwnershipStart).join(""), /install active; finish or cancel first/);
command(engine, "install 1 cancel");
engine._tg_sim_serial_connected(1);
const controllerOwnershipStart = serial.length;
command(engine, "stream on");
command(engine, "install 1 capabilities");
assert.match(serial.slice(controllerOwnershipStart).join(""), /controller mode; use stream off first/);
command(engine, "stream off");
engine._tg_sim_serial_connected(0);
assert.equal(engine._tg_sim_brightness(), 90);
assert.equal(engine._tg_sim_sleeping(), 0);
click(engine);
assert.match(serial.join(""), /screen=LAUNCHER/);
const pixel = (x, y) => engine.HEAPU16[engine._tg_sim_framebuffer() / 2 + y * 320 + x];
const yellow565 = 0xfe8c;
const surface565 = 0x10c5;
const cartridgeLed = engine._tg_sim_led();
for (let selected = 0; selected < 3; ++selected) {
  for (let row = 0; row < 3; ++row) {
    assert.equal(pixel(280, 42 + row * 20), row === selected ? yellow565 : surface565,
      "All selected cartridge rows use yellow, with a distinct filled focus state");
    if (row !== selected) assert.equal(pixel(10, 36 + row * 20), yellow565,
      "Unselected cartridge badges also use yellow");
  }
  assert.equal(engine._tg_sim_led(), cartridgeLed, "Launcher LED no longer changes with game identity");
  engine._tg_sim_turn(1, 4);
  advance();
}
command(engine, "launch maze");
assert.match(serial.join(""), /\[DEV\] error: no game with id maze/);
assert.doesNotMatch(serial.join(""), /\[APP\] start maze/);
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
saved.set("tinygame.sim.best.rockrain", JSON.stringify([0x78, 0x56, 0x34, 0x12]));
engine._tg_sim_stop();
({ engine, serial, wake } = await boot());
assert.equal(engine._tg_sim_brightness(), 80);
for (const id of ["brickbreaker", "knob_dodge", "rockrain", "tiltblock"]) {
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
assert.match(serial.join(""), /physical total=-32/);
assert.match(serial.join(""), /physical total=32/);
const beforeSoloHold = serial.length;
hold(engine); // An individual hold remains game input.
assert.doesNotMatch(serial.slice(beforeSoloHold).join(""), /screen=PAUSE/);
const beforePause = serial.length;
pauseChord(engine);
assert.match(serial.slice(beforePause).join(""), /screen=PAUSE/);
pauseChord(engine);
assert.match(serial.slice(beforePause).join(""), /screen=GAME/);
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
pauseChord(engine);
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
assert.doesNotMatch(serial.join(""), /start (brickbreaker|knob_dodge|rockrain|tiltblock)/);
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

({ engine, serial } = await boot());
const bundleHex = fs.readFileSync(
  new URL("../../tests/fixtures/tgb_v1_lua_tick.hex", import.meta.url), "utf8").replace(/\s/g, "");
const bundleBytes = Buffer.from(bundleHex, "hex");
assert(bundleBytes.length > 0);
assert.equal(installBundle(engine, bundleBytes), 0);
assert.match(serial.join(""), /\[STORE\] installed lua_tick/);
assert(saved.has("tinygame.sim.game.v1.lua_tick"));
engine._tg_sim_stop();

({ engine, serial } = await boot(["brickbreaker", "knob_dodge", "lua_tick", "rockrain", "tiltblock"]));
assert.match(serial.join(""), /\[BOOT\] ready, 5 games/);
command(engine, "launch lua_tick");
assert.match(serial.join(""), /\[APP\] start lua_tick/);
installViaProtocol(engine, bundleBytes);
assert.match(serial.join(""), /\[DEV\] ok/);
const storedBundle = saved.get("tinygame.sim.game.v1.lua_tick");
const alternateToml = replaceMember(bundleBytes, "game.toml",
  "\"api\"=+1\n'id'='lua_tick'\n'name'='LUA TICK'\n'starter'='platformer'\n'view'='fixed'");
assert.equal(installBundle(engine, alternateToml), 0);
const whitespaceToml = replaceMember(bundleBytes, "game.toml",
  "api=1\nid='lua_tick'\nname='   '\nstarter='\t'\nview='fixed'");
assert.equal(installBundle(engine, whitespaceToml), 0);
assert.equal(installBundle(engine, bundleBytes), 0);
assert.notEqual(installBundle(engine, Buffer.from("not a TGB bundle")), 0);
assert.match(serial.join(""), /\[STORE\] bundle install failed:/);
assert.equal(saved.get("tinygame.sim.game.v1.lua_tick"), storedBundle);
const moduleBundle = Buffer.from(fs.readFileSync(
  new URL("../../tests/fixtures/tgb_v1_lua_modules.hex", import.meta.url), "utf8")
  .replace(/\s/g, ""), "hex");
assert.equal(installBundle(engine, moduleBundle), 0);
saved.delete("tinygame.sim.game.v1.lua_modules");
const invalidModule = Buffer.from(moduleBundle);
const modulePath = invalidModule.indexOf("modules/palette.lua");
assert(modulePath >= 40);
invalidModule[modulePath + 8] = "1".charCodeAt(0);
const tableOffset = invalidModule.readUInt32LE(12);
const tableLength = invalidModule.readUInt32LE(16);
invalidModule.writeUInt32LE(
  crc32(invalidModule.subarray(tableOffset, tableOffset + tableLength)), 32);
assert.notEqual(installBundle(engine, invalidModule), 0);
assert(!saved.has("tinygame.sim.game.v1.lua_modules"));
assert.notEqual(installBundle(engine, addOversizedRules(bundleBytes)), 0);
assert.equal(saved.get("tinygame.sim.game.v1.lua_tick"), storedBundle);
const availableStorage = globalThis.localStorage;
globalThis.localStorage = {
  get length() { return availableStorage.length; },
  key: index => availableStorage.key(index),
  getItem: key => availableStorage.getItem(key),
  setItem: () => { throw new Error("storage quota"); },
};
assert.notEqual(installBundle(engine, bundleBytes), 0);
assert.match(serial.join(""), /\[STORAGE\] error saving .*Error/);
assert.equal(saved.get("tinygame.sim.game.v1.lua_tick"), storedBundle);
globalThis.localStorage = availableStorage;
engine._tg_sim_stop();

for (const expression of [
  "string.rep('', 10000)",
  "local s=string.rep('x', 1024); for i=1,100 do string.rep(s, 1) end",
  "local s=string.rep('x', 1024); for i=1,100 do table.concat({s}) end",
  "local s=string.rep('x', 1024); for i=1,100 do string.upper(s) end",
  "local s=string.rep('9', 1024); for i=1,100 do tonumber(s) end",
  "table.unpack({}, 1, 10000)",
  "table.move({}, 1, 10000, 1, {})",
  "local t={} for i=1,4100 do t[i]='' end table.concat(t)",
]) {
  ({ engine, serial } = await boot(["brickbreaker", "knob_dodge", "lua_tick", "rockrain", "tiltblock"]));
  const adversarial = replaceMember(bundleBytes, "rules.lua", `function update() ${expression} end\n`);
  assert.equal(installBundle(engine, adversarial), 0);
  engine._tg_sim_stop();
  ({ engine, serial } = await boot(["brickbreaker", "knob_dodge", "lua_tick", "rockrain", "tiltblock"]));
  command(engine, "launch lua_tick");
  assert.match(serial.join(""), /Lua game stopped: .*native (?:work|byte) limit exceeded/);
  advance(500);
  assert.equal(serial.join("").match(/Lua game stopped:/g)?.length, 1);
  pauseChord(engine);
  assert.match(serial.join(""), /\[APP\] screen=PAUSE/);
  const beforeQuit = serial.length;
  engine._tg_sim_turn(1, 8);
  advance();
  click(engine);
  assert.match(serial.slice(beforeQuit).join(""), /\[APP\] screen=LAUNCHER/);
  engine._tg_sim_stop();
}

({ engine, serial } = await boot(["brickbreaker", "knob_dodge", "lua_tick", "rockrain", "tiltblock"]));
const transparencyRules = `
function update(dt, input)
  tg.rect(0, 0, input.screen.width, input.screen.height, 7)
  tg.rect(0, 0, 20, 20, 0)
  tg.text(1, 1, 'TEST', 0)
end
`;
assert.equal(installBundle(engine, replaceMember(bundleBytes, "rules.lua", transparencyRules)), 0);
engine._tg_sim_stop();
({ engine, serial } = await boot(["brickbreaker", "knob_dodge", "lua_tick", "rockrain", "tiltblock"]));
command(engine, "launch lua_tick");
advance(200);
const transparentPixels = new Uint16Array(engine.HEAPU8.buffer, engine._tg_sim_framebuffer(), 320 * 172);
assertTransparentArea(transparentPixels, 320, 8, 0);
assert.doesNotMatch(serial.join(""), /Lua game stopped/);
engine._tg_sim_stop();

async function textFrame(x, y) {
  ({ engine, serial } = await boot(["brickbreaker", "knob_dodge", "lua_tick", "rockrain", "tiltblock"]));
  const rules = `function update(dt, input)
    tg.rect(0, 0, input.screen.width, input.screen.height, 7)
    tg.text(${x}, ${y}, 'ABCD', 8)
  end`;
  assert.equal(installBundle(engine, replaceMember(bundleBytes, "rules.lua", rules)), 0);
  engine._tg_sim_stop();
  ({ engine, serial } = await boot(["brickbreaker", "knob_dodge", "lua_tick", "rockrain", "tiltblock"]));
  command(engine, "launch lua_tick");
  advance(200);
  assert.doesNotMatch(serial.join(""), /Lua game stopped/);
  const pixels = Uint16Array.from(
    new Uint16Array(engine.HEAPU8.buffer, engine._tg_sim_framebuffer(), 320 * 172));
  engine._tg_sim_stop();
  return pixels;
}
const textOrigin = await textFrame(0, 0);
const textClipped = await textFrame(-4, -2);
assertClippedText(textOrigin, textClipped, 320, 8, 0);

({ engine, serial } = await boot(["brickbreaker", "knob_dodge", "lua_tick", "rockrain", "tiltblock"]));
const timingRules = `
local n, total = 0, 0
function update(dt, input)
  assert(dt > 0.01666 and dt < 0.01667)
  n = n + 1
  total = total + input.main.delta
  if n > 30 then assert(total == 4) end
  if input.click then
    assert(n > 30 and n < 140 and total == 4, "bad timing")
    error("timing complete")
  end
  tg.rect(total, 0, 1, 1, 2)
end
`;
assert.equal(installBundle(engine, replaceMember(bundleBytes, "rules.lua", timingRules)), 0);
engine._tg_sim_stop();
({ engine, serial } = await boot(["brickbreaker", "knob_dodge", "lua_tick", "rockrain", "tiltblock"]));
command(engine, "launch lua_tick");
engine._tg_sim_turn(1, 4);
jump(200);
advance(600);
assert.match(serial.join(""), /Simulation catching up/);
assert.doesNotMatch(serial.join(""), /Lua game stopped/);
engine._tg_sim_visible(0);
engine._tg_sim_visible(0);
jump(300_000);
engine._tg_sim_visible(1);
engine._tg_sim_visible(1);
advance(96);
assert.doesNotMatch(serial.join(""), /Lua game stopped|screen=SLEEP/);
pauseChord(engine);
engine._tg_sim_turn(1, 4);
advance();
engine._tg_sim_turn(1, -4);
advance();
advance(2000);
click(engine);
advance(600);
assert.doesNotMatch(serial.join(""), /Lua game stopped/);
click(engine);
assert.match(serial.join(""), /Lua game stopped: .*timing complete/);
engine._tg_sim_stop();

console.log("PASS: firmware boot, all games, RGB565 framebuffer, physical input/holds, " +
  "serial, settings and TGB persistence/errors, Lua timing and native-loop faults, idle sleep, wake reset, and loop controls.");
