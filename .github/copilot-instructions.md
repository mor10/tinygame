# TinyGame

For every request to create, fix or change a TinyGame game, automatically invoke
`make-a-tinygame` (`.agents/skills/make-a-tinygame/SKILL.md`) before writing cart code.
If skill invocation is unavailable, read that file and follow it directly.
It routes to `AGENTS.md`, the shared source of game-making guidance.
For engine, simulator, tooling or firmware work unrelated to a cart, read the relevant
sections of `docs/development.md` and `README.md`.
Do not use the old implementation on `legacy` as the current architecture.

Use the documented `./tg` commands from the repository root. `./tg setup` builds the native
simulator and checks the template. Follow `AGENTS.md` for cart checks and hardware boundaries;
local checks and previews do not need a connected console.

For interactive preview in Copilot, use the repository's `tinygame-preview` Canvas with
`{"cart":"games/<name>.lua"}`. It runs the actual browser simulator and reloads saved cart
edits; it does not replace the strict native cart check. See the README for support checks.
