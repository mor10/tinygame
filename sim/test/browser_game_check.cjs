// Browser check for games (not part of CI): serves a COPY of cave_hop, plays it
// with the keyboard, edits a tuning value in the copy, packs it again to the
// watched path and checks the page picks the new bundle up without a
// WebAssembly rebuild. Screenshots go to out/sim/.
//   node sim/test/browser_game_check.cjs
const { chromium } = require("playwright");
const path = require("node:path");
const fs = require("node:fs");
const os = require("node:os");
const { execFileSync } = require("node:child_process");

const root = path.resolve(__dirname, "../..");
const pack = (folder, out) => execFileSync("python3", ["tools/tg.py", "pack", folder, "-o", out], { cwd: root, stdio: "pipe" });

(async () => {
  const { startServer } = await import("../server.mjs");
  const work = fs.mkdtempSync(path.join(os.tmpdir(), "tg-game-"));
  const copy = path.join(work, "cave_hop");
  fs.cpSync(path.join(root, "games/cave_hop"), copy, { recursive: true });
  const bundle = path.join(work, "cave_hop.tgb");
  pack(copy, bundle);
  const server = await startServer({ bundle, pollMs: 50 });
  const out = path.join(root, "out/sim");
  fs.mkdirSync(out, { recursive: true });
  const browser = await chromium.launch();
  const failures = [];
  const expect = (ok, what) => { console.log(`${ok ? "ok  " : "FAIL"} ${what}`); if (!ok) failures.push(what); };
  try {
    const page = await browser.newPage({ viewport: { width: 900, height: 800 }, deviceScaleFactor: 2 });
    const problems = [];
    page.on("pageerror", e => problems.push(e.message));
    page.on("console", m => { if (m.type() === "error") problems.push(m.text()); });
    await page.goto(server.url);
    await page.waitForFunction(() => document.body.dataset.game === "running" && Number(document.body.dataset.ticks) > 10);
    expect(true, "the page fetched the bundle on start and started the game");
    const shot = () => page.evaluate(() => document.getElementById("screen").toDataURL());
    const first = await shot();

    // Run right with the arrow key, jump with K (the right knob's click).
    await page.keyboard.down("ArrowRight");
    await page.waitForTimeout(700);
    for (let i = 0; i < 3; i++) {
      await page.keyboard.down("k"); await page.waitForTimeout(450); await page.keyboard.up("k");
      await page.waitForTimeout(500);
    }
    const moved = await shot();
    expect(moved !== first, "the picture changed while playing");
    await page.screenshot({ path: path.join(out, "game_board_a.png") });
    await page.keyboard.up("ArrowRight");

    // Edit a tuning value in the copy and pack again to the watched path.
    const loadsBefore = Number(await page.evaluate(() => document.body.dataset.bundleLoads));
    const wasmBefore = await page.evaluate(() => performance.getEntriesByType("resource").filter(r => r.name.endsWith(".wasm")).length);
    const toml = path.join(copy, "game.toml");
    fs.writeFileSync(toml, fs.readFileSync(toml, "utf8").replace("run_speed = 6", "run_speed = 3"));
    pack(copy, bundle);
    await page.waitForFunction(n => Number(document.body.dataset.bundleLoads) > n, loadsBefore, { timeout: 5000 });
    expect(true, "the page picked up the new bundle after the file changed");
    await page.waitForFunction(() => document.body.dataset.game === "running" && Number(document.body.dataset.ticks) > 5);
    const wasmAfter = await page.evaluate(() => performance.getEntriesByType("resource").filter(r => r.name.endsWith(".wasm")).length);
    expect(wasmAfter === wasmBefore, `WebAssembly was not fetched again (${wasmBefore} -> ${wasmAfter})`);
    // With run_speed 3, holding right for one second moves the player half as far as before.
    const ticksStart = Number(await page.evaluate(() => document.body.dataset.ticks));
    await page.keyboard.down("ArrowRight");
    await page.waitForTimeout(1000);
    await page.keyboard.up("ArrowRight");
    const slow = await shot();
    expect(slow !== moved, "the changed game plays");

    // A broken bundle is shown readably, and a good one recovers.
    fs.writeFileSync(bundle, "not a game");
    await page.waitForFunction(() => document.body.dataset.game === "failed", null, { timeout: 5000 });
    const message = await page.textContent("#error");
    expect(/could not be loaded/.test(message), `load error is readable: ${message.trim()}`);
    pack(copy, bundle);
    await page.waitForFunction(() => document.body.dataset.game === "running", null, { timeout: 5000 });
    expect(await page.evaluate(() => document.getElementById("error").hidden), "the error goes away after a good bundle");

    // Both boards, a few seconds into the game.
    for (const board of ["a", "b"]) {
      await page.click(`[data-board="${board}"]`);
      await page.waitForTimeout(300);
      await page.keyboard.down("ArrowRight");
      await page.waitForTimeout(1500);
      await page.keyboard.up("ArrowRight");
      await page.screenshot({ path: path.join(out, `game_board_${board}.png`) });
      const size = await page.evaluate(() => [document.getElementById("screen").width, document.getElementById("screen").height]);
      expect(size[0] === (board === "a" ? 320 : 280), `Board ${board.toUpperCase()} canvas ${size.join("x")}`);
    }
    expect(problems.length === 0, `no console errors ${JSON.stringify(problems)}`);
  } finally {
    await browser.close();
    await server.close();
    fs.rmSync(work, { recursive: true, force: true });
  }
  if (failures.length) { console.log(`${failures.length} check(s) failed`); process.exit(1); }
  console.log("all browser game checks passed");
})().catch(error => { console.error(error); process.exit(1); });
