// Runs the WebAssembly engine in Node, no browser, as fast as possible.
//
//   node sim/headless.mjs [--board a|b] [--view standard|fill] [--ticks N] [--out file.rgb565]
//
// Prints one line: <board> <width> <height> <view> <ticks> <fingerprint>. The
// fingerprint is the same FNV-1a of the RGB565 frame that the native demo host
// prints (engine/core/hash.h). --out writes the raw frame (little-endian).

import { writeFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { resolve } from "node:path";
import { BOARDS, Engine, hashPixels } from "./web/engine.mjs";

export async function loadEngine() {
  const { default: create } = await import("./web/build/tinygame.js");
  return new Engine(await create());
}

// Starts a board and steps `ticks` ticks (no input). Returns the frame.
export function runTicks(engine, width, height, view, ticks) {
  engine.init(width, height, view);
  for (let i = 0; i < ticks; i++) engine.tick();
  return engine.draw();
}

async function main() {
  const options = { board: "a", view: "standard", ticks: "0", out: null };
  const args = process.argv.slice(2);
  for (let i = 0; i < args.length; i++) {
    const name = args[i].replace(/^--/, "");
    if (!args[i].startsWith("--") || !(name in options) || i + 1 >= args.length) {
      console.error("usage: node sim/headless.mjs [--board a|b] [--view standard|fill] [--ticks N] [--out file.rgb565]");
      process.exit(2);
    }
    options[name] = args[++i];
  }
  const board = BOARDS[options.board];
  const ticks = Number(options.ticks);
  if (!board || !Number.isInteger(ticks) || ticks < 0) {
    console.error("--board must be a or b; --ticks must be a whole number, 0 or more");
    process.exit(2);
  }
  let engine;
  try { engine = await loadEngine(); }
  catch (error) {
    console.error(`Cannot load the engine (${error.message}). Build it with: python3 tools/sim_build.py`);
    process.exit(1);
  }
  const frame = runTicks(engine, board.width, board.height, options.view, ticks);
  console.log(`${board.id} ${board.width} ${board.height} ${options.view} ${ticks} ${hashPixels(frame)}`);
  if (options.out) writeFileSync(options.out, new Uint8Array(frame.buffer, frame.byteOffset, frame.byteLength));
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main().catch(error => { console.error(error.message); process.exit(1); });
}
