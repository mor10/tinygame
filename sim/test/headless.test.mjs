// The WebAssembly engine must draw exactly what the native build draws: each
// case in engine/demo/scroll_cases.h has a golden fingerprint in
// engine/test/golden_scroll_demo.h.

import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { hashPixels } from "../web/engine.mjs";
import { loadEngine, runTicks } from "../headless.mjs";

const engineDir = fileURLToPath(new URL("../../engine/", import.meta.url));
const read = path => readFileSync(engineDir + path, "utf8");

export function parseGolden(text) {
  const golden = new Map();
  for (const m of text.matchAll(/\{\s*"([a-z0-9_]+)"\s*,\s*0x([0-9a-fA-F]+)ULL\s*\}/g)) {
    golden.set(m[1], m[2].toLowerCase().padStart(16, "0"));
  }
  return golden;
}

export function parseCases(text) {
  const cases = [];
  for (const m of text.matchAll(/\{\s*"([a-z0-9_]+)"\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*View::(Standard|Fill)\s*,\s*(\d+)\s*\}/g)) {
    cases.push({ name: m[1], width: +m[2], height: +m[3], view: m[4].toLowerCase(), tick: +m[5] });
  }
  return cases;
}

const golden = parseGolden(read("test/golden_scroll_demo.h"));
const cases = parseCases(read("demo/scroll_cases.h"));
const engine = await loadEngine();

test("the case and golden headers parse", () => {
  assert.ok(cases.length >= 7, `found ${cases.length} cases`);
  assert.equal(golden.size, cases.length);
});

for (const c of cases) {
  test(`${c.name}: WebAssembly frame equals the golden fingerprint`, () => {
    const expected = golden.get(c.name);
    assert.ok(expected, `no golden value for ${c.name}`);
    const frame = runTicks(engine, c.width, c.height, c.view, c.tick);
    assert.equal(frame.length, c.width * c.height);
    assert.equal(hashPixels(frame), expected);
  });
}

test("drawing twice, and restarting, gives the same frame", () => {
  const c = cases.find(x => x.tick > 0);
  const first = runTicks(engine, c.width, c.height, c.view, c.tick);
  const one = hashPixels(first);
  assert.equal(hashPixels(engine.draw()), one);
  runTicks(engine, 320, 172, "standard", 5);   // a different board in between
  assert.equal(hashPixels(runTicks(engine, c.width, c.height, c.view, c.tick)), one);
});

test("tick input is accepted and ignored by the demo", () => {
  const c = cases.find(x => x.tick > 0);
  engine.init(c.width, c.height, c.view);
  for (let i = 0; i < c.tick; i++) engine.tick({ left: 3, right: -2, buttons: 3, tiltX: 1, tiltY: -1 });
  assert.equal(hashPixels(engine.draw()), golden.get(c.name));
});

test("a bundle that is not a game is refused and the demo keeps running", () => {
  engine.init(320, 172);
  assert.ok(engine.loadBundle(new Uint8Array([1, 2, 3])) < 0);
  assert.notEqual(engine.error, "");
  assert.equal(engine.hasGame, false);
  engine.tick();
  assert.equal(engine.draw().length, 320 * 172);
});

test("a bad board size is refused", () => {
  assert.throws(() => engine.init(0, 0));
  engine.init(320, 172);
});
