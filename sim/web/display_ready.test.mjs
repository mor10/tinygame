import test from "node:test";
import assert from "node:assert/strict";
import { waitForDisplayFrames } from "./display_ready.mjs";

function fakeHost(hidden = false) {
  let nextId = 0;
  const frames = new Map(), timers = new Map(), listeners = new Set();
  const host = {
    document: {
      hidden,
      addEventListener: (_, listener) => listeners.add(listener),
      removeEventListener: (_, listener) => listeners.delete(listener),
    },
    requestAnimationFrame: callback => { const id = ++nextId; frames.set(id, callback); return id; },
    cancelAnimationFrame: id => frames.delete(id),
    setTimeout: callback => { const id = ++nextId; timers.set(id, callback); return id; },
    clearTimeout: id => timers.delete(id),
  };
  return {
    host, frames, timers, listeners,
    visibility(value) {
      host.document.hidden = value;
      for (const listener of listeners) listener();
    },
    frame() {
      const callbacks = [...frames.values()];
      frames.clear();
      callbacks.forEach(callback => callback());
    },
  };
}

test("hidden boot waits until visible before starting its timeout", async () => {
  const clock = fakeHost(true);
  const ready = waitForDisplayFrames(clock.host);
  assert.equal(clock.timers.size, 0);
  clock.visibility(false);
  assert.equal(clock.timers.size, 1);
  clock.visibility(true);
  assert.equal(clock.timers.size, 0);
  clock.visibility(false);
  for (let i = 0; i < 4; i++) clock.frame();
  await ready;
  assert.equal(clock.timers.size, 0);
  assert.equal(clock.listeners.size, 0);
});

test("visible boot reports a stalled display and cancels its frame", async () => {
  const clock = fakeHost();
  const ready = waitForDisplayFrames(clock.host);
  const timeout = [...clock.timers.values()][0];
  timeout();
  await assert.rejects(ready, /display did not become ready/);
  assert.equal(clock.frames.size, 0);
  assert.equal(clock.listeners.size, 0);
});
