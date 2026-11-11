import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, writeFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { startServer } from "../server.mjs";
import { FixedStep } from "../web/engine.mjs";

test("FixedStep gives exactly 60 ticks per second whatever the frame time", () => {
  for (const frameMicros of [16667, 16666, 8333, 33333, 7000]) {
    const clock = new FixedStep();
    let ticks = 0, elapsed = 0;
    while (elapsed + frameMicros <= 10_000_000) { ticks += clock.advanceMicros(frameMicros); elapsed += frameMicros; }
    assert.ok(Math.abs(ticks - elapsed * 60 / 1e6) < 1, `${frameMicros}: ${ticks} ticks in ${elapsed} us`);
  }
});

test("FixedStep catches up at most 5 ticks after a stall", () => {
  const clock = new FixedStep();
  assert.equal(clock.advanceMicros(5_000_000), 5);
  assert.equal(clock.advanceMicros(0), 0);
});

test("the server is loopback only, serves the page, and reports bundle changes", async () => {
  const dir = mkdtempSync(join(tmpdir(), "tg-sim-"));
  const bundle = join(dir, "game.tgb");           // does not exist yet
  const server = await startServer({ bundle, pollMs: 50 });
  try {
    assert.match(server.url, /^http:\/\/127\.0\.0\.1:\d+\/$/);
    const page = await fetch(server.url);
    assert.equal(page.status, 200);
    assert.match(await page.text(), /TinyGame/);
    assert.equal((await fetch(server.url + "simulator.js")).headers.get("content-type").split(";")[0], "text/javascript");
    assert.equal((await fetch(server.url + "bundle")).status, 404);
    assert.equal((await fetch(server.url + "../server.mjs")).status, 404);
    assert.equal((await fetch(server.url + "%2e%2e/server.mjs")).status, 404);
    assert.equal((await fetch(server.url + "nothing.txt")).status, 404);

    const events = await fetch(server.url + "events");
    const reader = events.body.getReader();
    const next = async () => new TextDecoder().decode((await reader.read()).value);
    assert.match(await next(), /connected/);
    writeFileSync(bundle, "one");
    assert.match(await next(), /event: bundle/);
    const body = await fetch(server.url + "bundle");
    assert.equal(await body.text(), "one");
    writeFileSync(bundle, "twotwo");
    assert.match(await next(), /event: bundle/);
    await reader.cancel();
  } finally {
    await server.close();
    rmSync(dir, { recursive: true });
  }
});
