import { createCanvas, joinSession, CanvasError } from "@github/copilot-sdk/extension";
import { fileURLToPath } from "node:url";
import { resolve, join } from "node:path";
import { build, startSimulator } from "../../../sim/server.mjs";

const root = resolve(fileURLToPath(new URL("../../..", import.meta.url)));
const instances = new Map();
let session;
const object = properties => ({ type: "object", properties, additionalProperties: false });
const game = { type: "string", pattern: "^[a-z][a-z0-9_]{0,14}$" };
const definitions = [
  ["reload", "Restart the simulator firmware.", object({})],
  ["launch", "Launch a registered game.", { ...object({ game }), required: ["game"] }],
  ["send", "Send dev commands; supports turn, click, hold, down/up, tilt, launch, info and wait.", {
    ...object({ commands: { type: "array", maxItems: 100, items: { type: "string", maxLength: 240 } } }), required: ["commands"],
  }],
  ["screenshot", "Save the native screen as a PNG in session files.", object({ label: { type: "string", maxLength: 64 } })],
  ["get_output", "Read firmware status, build errors and serial output since a cursor.", object({ since: { type: "integer", minimum: 0 } })],
  ["connect_device", "Use a USB TinyGame console as controller; disconnect before flashing or serial tools.", object({ port: { type: "string", maxLength: 256 } })],
  ["disconnect_device", "Release the USB serial port and return the console to normal.", object({})],
];
const canvas = createCanvas({
  id: "tinygame-simulator",
  displayName: "TinyGame simulator",
  description: "Play the real TinyGame firmware using browser knobs, tilt or a connected USB console.",
  inputSchema: object({ game }),
  actions: definitions.map(([name, description, inputSchema]) => ({
    name, description, inputSchema,
    handler: async ctx => {
      const entry = instances.get(ctx.instanceId);
      if (!entry) throw new CanvasError("simulator_not_open", "Open the TinyGame simulator first.");
      try { return await entry.action(name, ctx.input ?? {}); }
      catch (e) { throw new CanvasError("simulator_error", e.message); }
    },
  })),
  open: async ctx => {
    let entry = instances.get(ctx.instanceId);
    if (!entry) {
      if (!session?.workspacePath) throw new CanvasError("workspace_missing", "Session artifact workspace is unavailable.");
      try {
        await build(root);
        entry = await startSimulator({
          root, game: ctx.input?.game,
          artifacts: join(session.workspacePath, "files", "tinygame-simulator"),
        });
        instances.set(ctx.instanceId, entry);
      } catch (e) { throw new CanvasError("simulator_build_failed", e.message); }
    }
    return { title: "TinyGame simulator", url: entry.url, status: "Ready" };
  },
  onClose: async ctx => {
    const entry = instances.get(ctx.instanceId);
    instances.delete(ctx.instanceId);
    await entry?.close();
  },
});
session = await joinSession({ canvases: [canvas] });
