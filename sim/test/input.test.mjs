import test from "node:test";
import assert from "node:assert/strict";
import {
  LEFT, RIGHT, REPEAT_DELAY, REPEAT_EVERY, Rotation, SimInput, angleAt, angleDelta, arrowTilt,
} from "../web/input.mjs";

test("A and D turn the left knob, J and L the right knob", () => {
  const input = new SimInput();
  input.keyDown("d"); input.keyUp("d");
  input.keyDown("A"); input.keyUp("A");     // capital letters count too
  input.keyDown("l"); input.keyUp("l");
  input.keyDown("j"); input.keyUp("j"); input.keyDown("j"); input.keyUp("j");
  const tick = input.takeTick();
  assert.equal(tick.left, 0);               // +4 and -4
  assert.equal(tick.right, -4);             // +4 and -8
});

test("a turn is given to exactly one tick", () => {
  const input = new SimInput();
  input.keyDown("d"); input.keyUp("d");
  assert.equal(input.takeTick().left, 4);
  assert.equal(input.takeTick().left, 0);
});

test("a held turn key repeats after a delay, and the keyboard's own repeat is ignored", () => {
  const input = new SimInput();
  input.keyDown("d");
  input.keyDown("d"); input.keyDown("d");
  let sum = input.takeTick().left;          // tick 1
  assert.equal(sum, 4);
  for (let t = 2; t < REPEAT_DELAY; t++) sum += input.takeTick().left;
  assert.equal(sum, 4, "no repeat yet");
  assert.equal(input.takeTick().left, 4, "first repeat at the delay");
  for (let t = 1; t < REPEAT_EVERY; t++) assert.equal(input.takeTick().left, 0);
  assert.equal(input.takeTick().left, 4);
  input.keyUp("d");
  assert.equal(input.takeTick().left, 0);
});

test("S and K click the knobs while held", () => {
  const input = new SimInput();
  assert.equal(input.takeTick().buttons, 0);
  input.keyDown("s");
  assert.equal(input.takeTick().buttons, 1);
  input.keyDown("K");
  assert.equal(input.takeTick().buttons, 3);
  input.keyUp("s");
  assert.equal(input.takeTick().buttons, 2);
  input.keyUp("k");
  assert.equal(input.takeTick().buttons, 0);
});

test("a knob stays down while any one thing holds it", () => {
  const input = new SimInput();
  input.press(LEFT, "key", true);
  input.press(LEFT, "mouse", true);
  input.press(LEFT, "key", false);
  assert.ok(input.isDown(LEFT));
  input.press(LEFT, "mouse", false);
  assert.ok(!input.isDown(LEFT));
  assert.ok(!input.isDown(RIGHT));
});

test("arrow keys tilt, opposite keys cancel, letting go is level", () => {
  assert.deepEqual(arrowTilt(new Set()), [0, 0]);
  assert.deepEqual(arrowTilt(new Set(["ArrowRight"])), [1, 0]);
  assert.deepEqual(arrowTilt(new Set(["ArrowLeft", "ArrowUp"])), [-1, -1]);
  assert.deepEqual(arrowTilt(new Set(["ArrowLeft", "ArrowRight", "ArrowDown"])), [0, 1]);
  const input = new SimInput();
  assert.ok(input.keyDown("ArrowDown"));
  assert.equal(input.takeTick().tiltY, 1);
  input.keyUp("ArrowDown");
  const level = input.takeTick();
  assert.equal(level.tiltX, 0);
  assert.equal(level.tiltY, 0);
});

test("keys that are not the console's are left alone", () => {
  const input = new SimInput();
  assert.equal(input.keyDown("x"), false);
  assert.equal(input.keyDown("Tab"), false);
  assert.equal(input.keyDown("a"), true);
});

test("releaseAll lets go of everything", () => {
  const input = new SimInput();
  input.keyDown("d"); input.keyDown("s"); input.keyDown("ArrowLeft");
  input.releaseAll();
  assert.deepEqual(input.takeTick(), { left: 0, right: 0, buttons: 0, tiltX: 0, tiltY: 0 });
});

test("dragging a knob a full turn is 80 transitions, and leftovers are kept", () => {
  const rotation = new Rotation();
  assert.equal(rotation.move(2 * Math.PI), 80);
  let total = 0;
  for (let i = 0; i < 100; i++) total += rotation.move(0.01);   // each is 0.127 transitions
  assert.equal(total, Math.trunc(100 * 0.01 * 80 / (2 * Math.PI)));
  assert.equal(new Rotation().move(-Math.PI), -40);
});

test("angles wrap the short way round", () => {
  assert.ok(Math.abs(angleDelta(Math.PI - 0.1, -Math.PI + 0.1) - 0.2) < 1e-9);
  const rect = { left: 0, top: 0, width: 100, height: 100 };
  assert.equal(angleAt(100, 50, rect), 0);
  assert.ok(Math.abs(angleAt(50, 100, rect) - Math.PI / 2) < 1e-9);
});
