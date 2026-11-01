import test from "node:test";
import assert from "node:assert/strict";
import { KEY_TURNS, KEY_BUTTONS, angleDelta, Rotation, arrowTilt, DeviceInput } from "./input.mjs";

test("requested shortcuts use clockwise positive and four transitions", () => {
  assert.deepEqual(KEY_TURNS, { a: [0, -4], d: [0, 4], j: [1, -4], l: [1, 4] });
  assert.deepEqual(KEY_BUTTONS, { s: 0, k: 1 });
});
test("drag crosses the angle seam without jumping and retains fractional movement", () => {
  assert.ok(Math.abs(angleDelta(Math.PI - .1, -Math.PI + .1) - .2) < 1e-10);
  const rotation = new Rotation();
  let total = 0;
  for (let i = 0; i < 100; i++) total += rotation.move(2 * Math.PI / 100);
  assert.equal(total, 80);
  assert.equal(rotation.move(-2 * Math.PI), -80);
});
test("arrow tilt springs back and opposing keys cancel", () => {
  assert.deepEqual(arrowTilt(new Set(["ArrowLeft", "ArrowDown"])), [-1, 1]);
  assert.deepEqual(arrowTilt(new Set(["ArrowLeft", "ArrowRight"])), [0, 0]);
});
test("controller establishes a baseline, preserves button edges and rejects malformed input", () => {
  const device = new DeviceInput();
  const frame = { leftTotal: 123, rightTotal: -9, leftDown: false, rightDown: true, tiltX: .4, tiltY: -.5 };
  assert.deepEqual(device.apply(frame), { turns: [0, 0], buttons: [false, true], tilt: [.4, -.5] });
  assert.deepEqual(device.apply({ ...frame, leftTotal: 127, rightTotal: -13 }).turns, [4, -4]);
  assert.throws(() => device.apply({ ...frame, tiltX: Infinity }));
  device.reset();
  assert.deepEqual(device.apply(frame).turns, [0, 0]);
  device.apply({ ...frame, leftTotal: 2147483647 });
  assert.equal(device.apply({ ...frame, leftTotal: -2147483648 }).turns[0], 1);
});
