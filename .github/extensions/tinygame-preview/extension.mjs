import { fileURLToPath } from "node:url";
import { joinSession, createCanvas, CanvasError } from "@github/copilot-sdk/extension";
import { startPreview, checkCart } from "./server.mjs";

const root = fileURLToPath(new URL("../../../", import.meta.url));
const servers = new Map();

await joinSession({
    canvases: [
        createCanvas({
            id: "tinygame-preview",
            displayName: "TinyGame live preview",
            description: "Play a local Lua cart in the real browser simulator; saved cart edits reload automatically.",
            inputSchema: {
                type: "object",
                properties: {
                    cart: { type: "string", pattern: "^games/[a-z0-9_-]{1,31}\\.lua$" },
                },
                additionalProperties: false,
            },
            actions: [
                {
                    name: "status",
                    description: "Get this preview's cart and loopback URL.",
                    inputSchema: { type: "object", properties: {}, additionalProperties: false },
                    handler: (ctx) => {
                        const entry = servers.get(ctx.instanceId);
                        if (!entry) throw new CanvasError("preview_not_open", "Open the preview first.");
                        return { cart: entry.cart, url: entry.url, liveReload: true };
                    },
                },
            ],
            open: async (ctx) => {
                const cart = ctx.input?.cart ?? "games/brickbreaker.lua";
                try {
                    await checkCart(root, cart);
                } catch (error) {
                    throw new CanvasError("preview_cart_invalid", error.message);
                }
                let entry = servers.get(ctx.instanceId);
                if (!entry) {
                    entry = await startPreview(root);
                    servers.set(ctx.instanceId, entry);
                }
                entry.cart = cart;
                entry.url = `${entry.origin}/sim/web/?cart=${encodeURIComponent(`/${cart}`)}&live=1`;
                return {
                    title: `TinyGame: ${cart.slice(6, -4)}`,
                    url: entry.url,
                };
            },
            onClose: async (ctx) => {
                const entry = servers.get(ctx.instanceId);
                if (entry) {
                    servers.delete(ctx.instanceId);
                    await entry.close();
                }
            },
        }),
    ],
});
