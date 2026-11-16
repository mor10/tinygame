import { createServer } from "node:http";
import { readFile, realpath } from "node:fs/promises";
import { resolve } from "node:path";

const cartPattern = /^games\/[a-z0-9_-]{1,31}\.lua$/;
const assets = new Map([
    ["sim/web/", ["sim/web/index.html", "text/html; charset=utf-8"]],
    ["sim/web/app.js", ["sim/web/app.js", "text/javascript; charset=utf-8"]],
    ["sim/web/preview.js", ["sim/web/preview.js", "text/javascript; charset=utf-8"]],
    ["web-engine/tinygame.js", ["web-engine/tinygame.js", "text/javascript; charset=utf-8"]],
    ["web-engine/tinygame.wasm", ["web-engine/tinygame.wasm", "application/wasm"]],
]);

async function localFile(root, path) {
    const expected = resolve(root, path);
    if (await realpath(expected) !== expected) throw new Error("Preview files must not be symlinks.");
    return expected;
}

export async function checkCart(root, cart) {
    if (!cartPattern.test(cart)) throw new Error("Use a cart path such as games/brickbreaker.lua.");
    try {
        await localFile(await realpath(root), cart);
    } catch (error) {
        throw new Error(`Cannot preview ${cart}: ${error.message}`, { cause: error });
    }
}

export async function startPreview(root) {
    root = await realpath(root);
    const server = createServer(async (req, res) => {
        res.setHeader("Cache-Control", "no-store");
        res.setHeader("X-Content-Type-Options", "nosniff");
        if (req.headers.host !== `127.0.0.1:${server.address().port}`) {
            res.writeHead(403).end("Loopback host required");
            return;
        }
        if (req.method !== "GET" && req.method !== "HEAD") {
            res.writeHead(405, { Allow: "GET, HEAD" }).end("Method not allowed");
            return;
        }
        let path;
        try {
            path = decodeURIComponent(new URL(req.url, "http://localhost").pathname).slice(1);
        } catch {
            res.writeHead(400).end("Invalid URL");
            return;
        }
        const asset = assets.get(path) ?? (cartPattern.test(path) ? [path, "text/plain; charset=utf-8"] : null);
        if (!asset) {
            res.writeHead(404).end("Not found");
            return;
        }
        try {
            const body = await readFile(await localFile(root, asset[0]));
            res.writeHead(200, { "Content-Type": asset[1] });
            res.end(req.method === "HEAD" ? undefined : body);
        } catch (error) {
            const unavailable = ["ENOENT", "ENOTDIR"].includes(error.code) || error.message.includes("symlinks");
            if (!unavailable) console.error("TinyGame preview:", error);
            res.writeHead(unavailable ? 404 : 500).end(unavailable ? "Preview file not found" : "Cannot read preview file");
        }
    });
    await new Promise((resolve, reject) => {
        server.once("error", reject);
        server.listen(0, "127.0.0.1", resolve);
    });
    return {
        origin: `http://127.0.0.1:${server.address().port}`,
        close: () => new Promise((resolve, reject) => {
            server.close((error) => error ? reject(error) : resolve());
            server.closeIdleConnections();
        }),
    };
}
