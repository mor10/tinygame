// Browser check (not part of CI): serves the page, runs it, switches boards and
// views, drives the keyboard, and saves screenshots to out/sim/.
//   node sim/test/browser_check.cjs
const { chromium } = require("playwright");
const path = require("node:path");
const fs = require("node:fs");

(async () => {
  const { startServer } = await import("../server.mjs");
  const bundle = path.join(require("node:os").tmpdir(), `tg-check-${process.pid}.tgb`);
  const server = await startServer({ bundle, pollMs: 50 });
  const out = path.resolve(__dirname, "../../out/sim");
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
    await page.waitForFunction(() => document.body.dataset.ready === "true" && Number(document.body.dataset.ticks) > 60);
    const data = () => page.evaluate(() => ({ ...document.body.dataset, status: document.getElementById("status").textContent }));

    let d = await data();
    expect(d.board === "a" && d.view === "standard", `starts on Board A standard (${d.status})`);
    const canvas = await page.evaluate(() => { const c = document.getElementById("screen"); return [c.width, c.height, c.clientWidth]; });
    expect(canvas[0] === 320 && canvas[1] === 172 && canvas[2] % 320 === 0, `canvas 320x172 at an integer zoom (${canvas[2]} px wide)`);
    const rate = await page.evaluate(async () => {
      const a = Number(document.body.dataset.ticks); const t = performance.now();
      await new Promise(r => setTimeout(r, 2000));
      return (Number(document.body.dataset.ticks) - a) * 1000 / (performance.now() - t);
    });
    expect(Math.abs(rate - 60) < 3, `about 60 ticks per second (${rate.toFixed(1)})`);
    const before = await page.evaluate(() => document.getElementById("screen").toDataURL());
    await page.waitForTimeout(700);
    const after = await page.evaluate(() => document.getElementById("screen").toDataURL());
    expect(before !== after, "the picture changes as the demo scrolls");
    await page.screenshot({ path: path.join(out, "board_a_standard.png") });

    await page.click('[data-board="b"]');
    await page.waitForFunction(() => document.body.dataset.board === "b");
    await page.waitForTimeout(1200);
    d = await data();
    const cb = await page.evaluate(() => { const c = document.getElementById("screen"); return [c.width, c.height]; });
    expect(cb[0] === 280 && cb[1] === 240, `Board B canvas is 280x240 (${d.status})`);
    await page.screenshot({ path: path.join(out, "board_b_standard.png") });

    await page.click('[data-view="fill"]');
    await page.waitForFunction(() => document.body.dataset.view === "fill");
    await page.waitForTimeout(1200);
    await page.screenshot({ path: path.join(out, "board_b_fill.png") });

    await page.click('[data-board="a"]');
    await page.waitForTimeout(800);
    await page.screenshot({ path: path.join(out, "board_a_fill.png") });

    // Keyboard: the knob marker turns, the tilt dot moves.
    const marker = () => page.evaluate(() => document.querySelector(".knob .marker").style.transform);
    const m0 = await marker();
    await page.keyboard.press("d");
    await page.waitForTimeout(100);
    expect((await marker()) !== m0, "pressing D turns the left knob on screen");
    await page.keyboard.down("s");
    await page.waitForTimeout(100);
    expect(await page.evaluate(() => document.querySelector(".knob").classList.contains("pressed")), "holding S presses the left knob");
    await page.keyboard.up("s");
    await page.keyboard.down("ArrowRight");
    await page.waitForTimeout(100);
    expect(await page.evaluate(() => document.getElementById("tilt-dot").style.transform.includes("12px")), "arrow key tilts");
    await page.keyboard.up("ArrowRight");
    await page.waitForTimeout(100);
    expect(await page.evaluate(() => /translate\(0px, 0px\)/.test(document.getElementById("tilt-dot").style.transform)), "letting go is level");
    // Mouse: click the middle of the right knob.
    const centre = await page.locator('.knob[data-side="1"] .centre').boundingBox();
    await page.mouse.move(centre.x + centre.width / 2, centre.y + centre.height / 2);
    await page.mouse.down();
    await page.waitForTimeout(100);
    expect(await page.evaluate(() => document.querySelector('.knob[data-side="1"]').classList.contains("pressed")), "mouse press on the right knob");
    await page.mouse.up();
    await page.screenshot({ path: path.join(out, "controls.png") });

    // The bundle path: a change on disk reaches the page (a file that is not a game is refused, readably).
    fs.writeFileSync(bundle, "TGB-test");
    await page.waitForFunction(() => Number(document.body.dataset.bundleLoads || 0) >= 1);
    const notice = await page.textContent("#notice");
    expect(/could not be loaded/.test(notice) && /could not be loaded/.test(await page.textContent("#error")), `bundle change reaches the page (${notice})`);
    expect(problems.length === 0, `no console errors ${JSON.stringify(problems)}`);
  } finally {
    await browser.close();
    await server.close();
    fs.rmSync(bundle, { force: true });
  }
  if (failures.length) { console.error(`${failures.length} check(s) failed`); process.exit(1); }
  console.log(`Screenshots are in ${out}`);
})().catch(e => { console.error(e); process.exit(1); });
