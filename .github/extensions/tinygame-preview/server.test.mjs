import assert from "node:assert/strict";
import { mkdtemp, mkdir, writeFile, symlink, rm } from "node:fs/promises";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { get } from "node:http";
import { test } from "node:test";
import { checkCart, startPreview } from "./server.mjs";

test("preview serves only simulator assets and local carts, without caching edits", async (t) => {
    const root = await mkdtemp(join(tmpdir(), "tinygame-preview-"));
    t.after(() => rm(root, { recursive: true, force: true }));
    await Promise.all(["sim/web", "web-engine", "games"].map((path) => mkdir(join(root, path), { recursive: true })));
    await Promise.all([
        writeFile(join(root, "sim/web/index.html"), "<h1>Simulator</h1>"),
        writeFile(join(root, "web-engine/tinygame.wasm"), "wasm"),
        writeFile(join(root, "games/test.lua"), "first cart"),
        writeFile(join(root, "private.txt"), "not public"),
        symlink(join(root, "private.txt"), join(root, "games/link.lua")),
    ]);
    const preview = await startPreview(root);
    t.after(() => preview.close());
    assert.match(preview.origin, /^http:\/\/127\.0\.0\.1:\d+$/);
    await checkCart(root, "games/test.lua");
    await assert.rejects(checkCart(root, "../private.txt"));
    await assert.rejects(checkCart(root, "games/missing.lua"));
    await assert.rejects(checkCart(root, "games/link.lua"));

    const page = await fetch(`${preview.origin}/sim/web/`);
    assert.equal(page.status, 200);
    assert.match(page.headers.get("content-type"), /text\/html/);
    assert.match(await page.text(), /Simulator/);
    const wasm = await fetch(`${preview.origin}/web-engine/tinygame.wasm`);
    assert.equal(wasm.headers.get("content-type"), "application/wasm");
    assert.equal(await wasm.text(), "wasm");

    const cartUrl = `${preview.origin}/games/test.lua`;
    const first = await fetch(cartUrl);
    assert.equal(first.headers.get("cache-control"), "no-store");
    assert.equal(await first.text(), "first cart");
    await writeFile(join(root, "games/test.lua"), "saved edit");
    assert.equal(await (await fetch(cartUrl)).text(), "saved edit");
    const head = await fetch(cartUrl, { method: "HEAD" });
    assert.equal(head.status, 200);
    assert.equal(await head.text(), "");
    assert.equal((await fetch(cartUrl, { method: "POST" })).status, 405);
    const reboundHostStatus = await new Promise((resolve, reject) => {
        get(cartUrl, { headers: { Host: "untrusted.example" } }, (res) => {
            res.resume();
            resolve(res.statusCode);
        }).on("error", reject);
    });
    assert.equal(reboundHostStatus, 403);

    for (const path of ["/private.txt", "/.git/config", "/games/link.lua", "/games/missing.lua",
        "/games/%2e%2e%2fprivate.txt", "/sim/web/../../../private.txt"]) {
        assert.equal((await fetch(`${preview.origin}${path}`)).status, 404, path);
    }
    assert.equal((await fetch(`${preview.origin}/%zz`)).status, 400);
});
