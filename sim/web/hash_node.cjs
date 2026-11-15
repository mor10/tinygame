// Runs a cart through the WASM build in Node and prints frame hashes like tinygame-sim, so the
// browser engine can be checked against the native simulator and the board:
//   node sim/web/hash_node.cjs CART.lua [frames] [seed]
const fs = require("fs");
const path = require("path");
const TinyGame = require(path.join(__dirname, "..", "build-web", "tinygame.js"));

(async () => {
  const [cart, framesArg, seedArg] = process.argv.slice(2);
  const frames = parseInt(framesArg || "300", 10), seed = parseInt(seedArg || "0", 10);
  const M = await TinyGame({ print: (l) => { if (!l.includes('"t":"stats"')) console.log(l); } });
  const load = M.cwrap("tg_load", "number", ["number", "number", "number", "number"]);
  const frame = M.cwrap("tg_frame", "number", ["number", "number", "number", "number", "number", "number", "number"]);
  const hash = M.cwrap("tg_hash", "number", []);
  const src = fs.readFileSync(cart, "utf8");
  const ptr = M.stringToNewUTF8(src);
  let ok = load(ptr, M.lengthBytesUTF8(src), 1, seed) === 0;
  let n = 0;
  while (ok && n < frames) {
    ok = frame(0, 0, 0, 0, 0, 0, 0) === 0;
    if (!ok) break;
    n++;
    if (n % 30 === 0 || n === 1 || n === frames)
      console.log(JSON.stringify({ t: "hash", frame: n, fnv: (hash() >>> 0).toString(16).padStart(8, "0") }));
  }
  console.log(JSON.stringify({ t: "done", frames: n, ok: ok ? 1 : 0 }));
  process.exit(ok ? 0 : 1);
})();
