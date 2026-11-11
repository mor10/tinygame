// The WebAssembly engine running a real packed game must draw exactly what the
// native engine draws: the cave_hop frames in engine/test/golden_game.h, with
// the inputs of engine/test/game_cases.h (run right; click at ticks 30 and 90,
// let go at 45 and 105). Needs `python3 tools/tg.py pack cave_hop`, done here.

import test, { before } from "node:test";
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { hashPixels } from "../web/engine.mjs";
import { loadEngine } from "../headless.mjs";

const root = fileURLToPath(new URL("../../", import.meta.url));
const read = path => readFileSync(root + path, "utf8");

function parseGameCases(text) {
  const cases = [];
  for (const m of text.matchAll(/\{\s*"([a-z0-9_]+)"\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}/g)) {
    cases.push({ name: m[1], width: +m[2], height: +m[3], tick: +m[4] });
  }
  return cases;
}
function parseGolden(text) {
  const golden = new Map();
  for (const m of text.matchAll(/\{\s*"([a-z0-9_]+)"\s*,\s*0x([0-9a-fA-F]+)ULL\s*\}/g)) {
    golden.set(m[1], m[2].toLowerCase().padStart(16, "0"));
  }
  return golden;
}

// The same input as goldenInput() in engine/test/game_cases.h; the right knob
// is held from the press to the release.
function goldenInput(tick) {
  const held = (tick >= 30 && tick < 45) || (tick >= 90 && tick < 105);
  return { buttons: held ? 2 : 0, tiltX: 1 };
}

const cases = parseGameCases(read("engine/test/game_cases.h"));
const golden = parseGolden(read("engine/test/golden_game.h"));
let bundle;
const engine = await loadEngine();

before(() => {
  execFileSync("python3", ["tools/tg.py", "pack", "cave_hop"], { cwd: root, stdio: "pipe" });
  bundle = new Uint8Array(readFileSync(root + "out/cave_hop.tgb"));
});

test("the case lists parse", () => {
  assert.equal(cases.length, 4);
  assert.equal(golden.size, 4);
});

for (const name of ["game_a_t0", "game_a_t120", "game_b_t0", "game_b_t120"]) {
  test(`${name}: WebAssembly frame of the packed cave_hop equals the native golden`, () => {
    const c = cases.find(x => x.name === name);
    assert.ok(c, `no case ${name}`);
    engine.init(c.width, c.height);
    assert.equal(engine.loadBundle(bundle), 0, engine.error);
    assert.equal(engine.hasGame, true);
    for (let i = 0; i < c.tick; i++) engine.tick(goldenInput(i));
    const frame = engine.draw();
    assert.equal(frame.length, c.width * c.height);
    assert.equal(hashPixels(frame), golden.get(name));
  });
}

test("the game reports its state through the probe", () => {
  engine.init(320, 172);
  assert.equal(engine.loadBundle(bundle), 0);
  for (let i = 0; i < 10; i++) engine.tick({ tiltX: 1 });
  assert.match(engine.probe, /^tick 10\nscore 0\nhurts 0\n/);
});

test("a corrupted bundle is rejected with an error", () => {
  engine.init(320, 172);
  assert.equal(engine.loadBundle(bundle), 0);
  const bad = bundle.slice();
  bad[bad.length >> 1] ^= 0xff;
  assert.ok(engine.loadBundle(bad) < 0);
  assert.notEqual(engine.error, "");
  assert.equal(engine.hasGame, false);
  assert.ok(engine.loadBundle(bundle.slice(0, 20)) < 0);
  assert.ok(engine.loadBundle(new Uint8Array(0)) < 0);
  // A good bundle works again afterwards.
  assert.equal(engine.loadBundle(bundle), 0);
  assert.equal(engine.error, "");
});
