// Shared by the browser page and the Node headless runner: the board sizes,
// the fixed 60 Hz clock, and a thin wrapper over the WebAssembly C functions
// (see sim/wasm/sim_main.cpp).

export const BOARDS = Object.freeze({
  a: Object.freeze({ id: "a", name: "Board A", width: 320, height: 172 }),
  b: Object.freeze({ id: "b", name: "Board B", width: 280, height: 240 }),
});
export const VIEWS = Object.freeze({ standard: 0, fill: 1 });

// Answers from Engine.loadBundle, the same numbers as in sim_main.cpp.
export const BUNDLE_OK = 0, BUNDLE_FAILED = -1;

// The same clock as engine/core/fixed_step.h. Feed it elapsed real time in
// whole microseconds; it says how many game ticks to run. Time is counted in
// units of 1/60 microsecond so 60 ticks per second is exact, with no drift.
export class FixedStep {
  static HZ = 60;
  static MAX_TICKS = 5;      // after a long stall, catch up this much and drop the rest
  static ONE_SECOND = 1_000_000;
  accumulated = 0;
  advanceMicros(elapsedMicros) {
    this.accumulated += elapsedMicros * FixedStep.HZ;
    let ticks = 0;
    while (this.accumulated >= FixedStep.ONE_SECOND && ticks < FixedStep.MAX_TICKS) {
      this.accumulated -= FixedStep.ONE_SECOND;
      ticks++;
    }
    if (this.accumulated >= FixedStep.ONE_SECOND) this.accumulated = 0;
    return ticks;
  }
}

// FNV-1a over 16-bit pixels, low byte first: the same as engine/core/hash.h.
// Returns 16 hex digits, the way the native demo host prints it.
export function hashPixels(pixels) {
  const mask = 0xffffffffffffffffn, prime = 0x100000001b3n;
  let hash = 0xcbf29ce484222325n;
  for (let i = 0; i < pixels.length; i++) {
    const pixel = pixels[i];
    hash = ((hash ^ BigInt(pixel & 0xff)) * prime) & mask;
    hash = ((hash ^ BigInt(pixel >> 8)) * prime) & mask;
  }
  return hash.toString(16).padStart(16, "0");
}

export class Engine {
  constructor(module) { this.module = module; }

  // Starts or restarts for a screen size. Throws if the engine refuses.
  init(width, height, view = "standard") {
    if (!(view in VIEWS)) throw new Error(`Unknown view "${view}" (use standard or fill)`);
    if (!this.module._tg_sim_init(width, height, VIEWS[view])) {
      throw new Error(`The engine could not start at ${width}x${height} (${view})`);
    }
  }

  // One 60 Hz tick. input is what SimInput.takeTick() returns.
  tick(input = {}) {
    this.module._tg_sim_tick(input.left ?? 0, input.right ?? 0, input.buttons ?? 0,
      input.tiltX ?? 0, input.tiltY ?? 0);
  }

  get width() { return this.module._tg_sim_width(); }
  get height() { return this.module._tg_sim_height(); }

  // Draws and returns the frame as RGB565 pixels. This is a view into the
  // engine's memory: use it (or copy it) before the next init.
  draw() {
    const address = this.module._tg_sim_draw();
    return new Uint16Array(this.module.HEAPU8.buffer, address, this.width * this.height);
  }

  // Why the last bundle was refused, or what stopped the running game ("" if nothing).
  get error() { return this.module.UTF8ToString(this.module._tg_sim_error()); }
  get hasGame() { return this.module._tg_sim_has_game() === 1; }
  // The engine's probe text for a running game: "tick 120\nscore 10\n...".
  get probe() { return this.module.UTF8ToString(this.module._tg_sim_probe()); }

  // Gives the engine a game bundle (a Uint8Array) and starts it. Returns
  // BUNDLE_OK (0), or a negative number with `error` telling why.
  loadBundle(bytes) {
    const address = this.module._malloc(Math.max(1, bytes.length));
    if (!address) throw new Error("Out of memory while loading the bundle");
    try {
      this.module.HEAPU8.set(bytes, address);
      return this.module._tg_sim_load_bundle(address, bytes.length);
    } finally {
      this.module._free(address);
    }
  }
}
