import test from "node:test";
import assert from "node:assert/strict";
import { mkdtemp, rm, mkdir, writeFile } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { startSimulator, screenshotBytes, validGame } from "./server.mjs";

test("validates game identifiers and rejects invalid screenshots", () => {
  assert.equal(validGame("brickbreaker"), true);
  assert.equal(validGame("../secret"), false);
  assert.equal(validGame("x\ninfo"), false);
  assert.throws(() => screenshotBytes("data:image/png;base64,AAAA"));
});

test("controller bridge connects, validates frames and releases its child process", async t => {
  const root = await mkdtemp(join(tmpdir(), "tinygame-bridge-test-"));
  await mkdir(join(root, "tools"));
  await writeFile(join(root, "tools", "tg.py"), `
import json, signal, sys, time
signal.signal(signal.SIGINT, lambda *_: sys.exit(0))
print(json.dumps(dict(leftTotal=0, rightTotal=0, leftDown=False, rightDown=False, tiltX=0.0, tiltY=0.0)), flush=True)
while True: time.sleep(0.1)
`);
  const instance = await startSimulator({ root, artifacts: root, autoBuild: false });
  t.after(async () => { await instance.close(); await rm(root, { recursive: true }); });
  assert.deepEqual(await instance.action("connect_device"), { connected: true });
  assert.equal((await instance.action("get_output")).connected, true);
  assert.deepEqual(await instance.action("disconnect_device"), { connected: false });
  assert.equal((await instance.action("get_output")).connected, false);
  await writeFile(join(root, "tools", "tg.py"), `print('{"leftTotal": "bad"}', flush=True)\n`);
  await assert.rejects(instance.action("connect_device"), /protocol error/);
});

test("loopback server scopes routes, validates posts and handles action replies", async t => {
  const artifacts = await mkdtemp(join(tmpdir(), "tinygame-sim-test-"));
  const instance = await startSimulator({ root: resolve("."), artifacts, autoBuild: false });
  t.after(async () => { await instance.close(); await rm(artifacts, { recursive: true }); });
  assert.match(instance.url, /^http:\/\/127\.0\.0\.1:\d+\/[a-f0-9]{48}\/$/);
  const get = path => fetch(new URL(path, instance.url));
  const post = (path, body, origin) => fetch(new URL(path, instance.url), {
    method: "POST", headers: { "Content-Type": "application/json", ...(origin ? { Origin: origin } : {}) }, body: JSON.stringify(body),
  });
  assert.equal((await get("")).status, 200);
  assert.equal((await get("build/missing.wasm")).status, 404);
  assert.equal((await fetch(new URL("/index.html", instance.url))).status, 404);
  assert.equal((await get("%2e%2e%2fserver.mjs")).status, 404);
  assert.equal((await post("report", { lines: [] }, "https://example.org")).status, 403);
  assert.equal((await post("report", { lines: "invalid" })).status, 400);
  await post("report", { lines: ["[BOOT] ready"], ready: true });
  const output = await instance.action("get_output");
  assert.equal(output.status, "ready");
  assert.equal(output.lines[0].text, "[BOOT] ready");
  assert.deepEqual((await instance.action("get_output", { since: output.next })).lines, []);
  await assert.rejects(instance.action("launch", { game: "../x" }));
  await assert.rejects(instance.action("send", { commands: ["wait Infinity"] }));
  await assert.rejects(instance.action("reload"), /page is not ready/);

  const abort = new AbortController();
  const events = await fetch(new URL("events", instance.url), { signal: abort.signal });
  const reader = events.body.getReader();
  await reader.read();
  const action = instance.action("launch", { game: "maze" });
  let text = "";
  while (!text.includes("event: request")) text += new TextDecoder().decode((await reader.read()).value);
  const request = JSON.parse(text.split("data: ").at(-1).trim());
  assert.equal(request.input.game, "maze");
  assert.equal((await post("reply", { id: request.id, result: { game: "maze" } })).status, 200);
  assert.deepEqual(await action, { game: "maze" });
  assert.equal((await post("reply", { id: request.id, result: {} })).status, 409);
  const secondAbort = new AbortController();
  const second = await fetch(new URL("events", instance.url), { signal: secondAbort.signal });
  await second.body.getReader().read();
  await assert.rejects(instance.action("reload"), /Multiple simulator pages/);
  secondAbort.abort();
  abort.abort();
});
