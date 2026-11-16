import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { readdirSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { test } from "node:test";

const root = fileURLToPath(new URL("../../", import.meta.url));
const frames = 300;
const expectedFrames = [1, ...Array.from({ length: frames / 30 }, (_, i) => (i + 1) * 30)];

function hashes(command, args) {
  const output = execFileSync(command, args, { cwd: root, encoding: "utf8" });
  const events = output.trim().split("\n").filter((line) => line.startsWith("{")).map((line) => JSON.parse(line));
  assert.equal(events.some((event) => event.t === "error"), false, output);
  const result = events.filter((event) => event.t === "hash");
  assert.deepEqual(result.map((event) => event.frame), expectedFrames, output);
  return result;
}

for (const name of readdirSync(new URL("../../games/", import.meta.url)).filter((name) => name.endsWith(".lua"))) {
  test(`committed browser engine matches native simulator: ${name}`, () => {
    const cart = `games/${name}`;
    const native = hashes("./sim/build/tinygame-sim", [cart, "--frames", `${frames}`, "--seed", "0"]);
    const browser = hashes(process.execPath, ["sim/web/hash_node.cjs", cart, `${frames}`, "0"]);
    assert.deepEqual(browser, native, "Rebuild web-engine with ./tg web-build after engine changes.");
  });
}
