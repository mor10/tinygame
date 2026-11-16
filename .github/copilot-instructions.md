# TinyGame

Start with `AGENTS.md` for cart work; it is the shared source of game-making guidance.
Read `docs/api-card.md` and `games/_template.lua` before writing Lua. For engine, simulator,
tooling or firmware work, read the relevant sections of `CLAUDE.md` and `README.md`.
Do not use the old implementation on `legacy` as the current architecture.

Use the documented `./tg` commands from the repository root. `./tg setup` builds the native
simulator and checks the template. Check changed carts with `./tg check games/<name>.lua`
and inspect their screenshots. Hardware access (`send`, `flash`, serial commands) requires
an explicit user request; local checks and previews do not need a connected console.

For interactive preview in Copilot, use the repository's `tinygame-preview` Canvas with
`{"cart":"games/<name>.lua"}`. It runs the actual browser simulator and reloads saved cart
edits; it does not replace the strict native cart check. See the README for support checks.
