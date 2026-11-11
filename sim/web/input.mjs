// Keyboard and mouse to console input. No browser objects in here, so it can
// be unit-tested in Node (sim/test/input.test.mjs).
//
//   Left knob:  A turn left, D turn right, S click
//   Right knob: J turn left, L turn right, K click
//   Tilt:       arrow keys (let go = level)

export const TRANSITIONS_PER_TURN = 80;   // same as the real knobs
export const LEFT = 0, RIGHT = 1;

// key -> [knob, transitions per step]. Clockwise is positive.
export const KEY_TURNS = Object.freeze({
  a: [LEFT, -4], d: [LEFT, 4], j: [RIGHT, -4], l: [RIGHT, 4],
});
export const KEY_BUTTONS = Object.freeze({ s: LEFT, k: RIGHT });

// A held turn key steps once at once, then repeats: after REPEAT_DELAY ticks,
// one more step every REPEAT_EVERY ticks (60 ticks = 1 second).
export const REPEAT_DELAY = 20, REPEAT_EVERY = 5;

export function angleDelta(previous, current) {
  return Math.atan2(Math.sin(current - previous), Math.cos(current - previous));
}

export function angleAt(x, y, rect) {
  return Math.atan2(y - rect.top - rect.height / 2, x - rect.left - rect.width / 2);
}

// Turns a dragged angle into whole knob transitions, keeping the leftover.
export class Rotation {
  remainder = 0;
  move(radians) {
    this.remainder += radians * TRANSITIONS_PER_TURN / (2 * Math.PI);
    const transitions = Math.trunc(this.remainder + Math.sign(this.remainder) * 1e-10);
    this.remainder -= transitions;
    return transitions;
  }
}

export function arrowTilt(keys) {
  return [
    Number(keys.has("ArrowRight")) - Number(keys.has("ArrowLeft")),
    Number(keys.has("ArrowDown")) - Number(keys.has("ArrowUp")),
  ];
}

// Collects everything the player does between two game ticks. The page calls
// keyDown/keyUp/turn/press as events arrive, and takeTick() once per tick.
export class SimInput {
  pending = [0, 0];                    // knob transitions not yet given to a tick
  total = [0, 0];                      // all transitions so far (for the on-screen knobs)
  holders = [new Set(), new Set()];    // what is holding each knob down ("key", "mouse")
  keys = new Set();                    // arrow keys held
  heldTurns = new Map();               // turn key -> ticks held

  // Returns true if the key belongs to the console (so the page can stop the
  // browser from scrolling). `key` is KeyboardEvent.key.
  keyDown(key) {
    const lower = key.length === 1 ? key.toLowerCase() : key;
    if (lower in KEY_TURNS) {
      if (!this.heldTurns.has(lower)) {       // ignore the keyboard's own repeat
        this.heldTurns.set(lower, 0);
        this.turn(...KEY_TURNS[lower]);
      }
      return true;
    }
    if (lower in KEY_BUTTONS) { this.press(KEY_BUTTONS[lower], "key", true); return true; }
    if (key.startsWith("Arrow")) { this.keys.add(key); return true; }
    return false;
  }

  keyUp(key) {
    const lower = key.length === 1 ? key.toLowerCase() : key;
    if (lower in KEY_TURNS) { this.heldTurns.delete(lower); return true; }
    if (lower in KEY_BUTTONS) { this.press(KEY_BUTTONS[lower], "key", false); return true; }
    if (key.startsWith("Arrow")) { this.keys.delete(key); return true; }
    return false;
  }

  turn(side, transitions) {
    this.pending[side] += transitions;
    this.total[side] += transitions;
  }

  press(side, holder, down) {
    if (down) this.holders[side].add(holder); else this.holders[side].delete(holder);
  }

  isDown(side) { return this.holders[side].size > 0; }
  tilt() { return arrowTilt(this.keys); }

  // Lets go of everything (the page lost focus, or the engine restarted).
  releaseAll() {
    this.pending = [0, 0];
    this.holders.forEach(set => set.clear());
    this.keys.clear();
    this.heldTurns.clear();
  }

  // The input for one 60 Hz tick, in the shape tg_sim_tick wants.
  takeTick() {
    for (const [key, ticks] of this.heldTurns) {
      const held = ticks + 1;
      this.heldTurns.set(key, held);
      if (held >= REPEAT_DELAY && (held - REPEAT_DELAY) % REPEAT_EVERY === 0) {
        this.turn(...KEY_TURNS[key]);
      }
    }
    const [left, right] = this.pending;
    this.pending = [0, 0];
    const [tiltX, tiltY] = this.tilt();
    return {
      left, right,
      buttons: Number(this.isDown(LEFT)) | (Number(this.isDown(RIGHT)) << 1),
      tiltX, tiltY,
    };
  }
}
