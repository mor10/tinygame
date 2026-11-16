import assert from "node:assert/strict";
import { setImmediate } from "node:timers/promises";
import { test } from "node:test";
import "./preview.js";

const { cartUrl, watch } = globalThis.TinyGamePreview;

test("preview cart selection accepts only same-origin games", () => {
  const base = "http://127.0.0.1:1234/sim/web/";
  assert.equal(cartUrl("", base), null);
  assert.equal(cartUrl("?cart=../../games/new-game.lua", base), "http://127.0.0.1:1234/games/new-game.lua");
  assert.equal(cartUrl("?cart=/games/_template.lua", base), "http://127.0.0.1:1234/games/_template.lua");
  for (const cart of ["https://example.com/games/a.lua", "/private.lua", "/games/a.lua?x=1",
    "/games/a.lua#x", "/games/../../private.lua", "/games/a.txt"]) {
    assert.throws(() => cartUrl(`?cart=${encodeURIComponent(cart)}`, base));
  }
});

test("live preview loads once, reloads saved changes, reports failures and recovers", async () => {
  const loaded = [], errors = [], pending = [];
  let text = "first", error, reads = 0;
  const stop = watch("/games/test.lua", {
    read: async () => { reads++; if (error) throw error; return text; },
    load: (text, name) => loaded.push([text, name]),
    report: (message) => errors.push(message),
    schedule: (callback, delay) => { assert.equal(delay, 1000); pending.push(callback); },
  });
  await setImmediate();
  assert.deepEqual(loaded, [["first", "test.lua"]]);
  await pending.shift()();
  assert.equal(loaded.length, 1);
  text = "edit";
  await pending.shift()();
  assert.deepEqual(loaded[1], ["edit", "test.lua"]);
  error = new Error("HTTP 404");
  await pending.shift()();
  await pending.shift()();
  assert.deepEqual(errors, ["Live preview: HTTP 404"]);
  error = undefined;
  text = "recovered";
  await pending.shift()();
  assert.equal(loaded.at(-1)[0], "recovered");
  stop();
  const readsAtStop = reads;
  text = "must not load";
  await pending.shift()();
  assert.equal(loaded.length, 3);
  assert.equal(reads, readsAtStop);
  assert.equal(pending.length, 0);
});

test("switching away from a live cart cancels an in-flight load", async () => {
  let finish;
  const loaded = [];
  const stop = watch("/games/test.lua", {
    read: () => new Promise((resolve) => { finish = resolve; }),
    load: (text) => loaded.push(text),
    report: assert.fail,
    schedule: assert.fail,
  });
  stop();
  finish("old cart");
  await setImmediate();
  assert.deepEqual(loaded, []);
});
