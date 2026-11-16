# TinyGame

Tiny screen. Twisty knobs. Real games, built by you.

TinyGame is a pocket game console for kids and parents to build, play and share games with AI
help. It's a Waveshare ESP32-S3-LCD-1.47B board with a 320x172 screen, two endless clicky knobs
and a tilt sensor. Games ("carts") are single Lua files.

> This is a full rebuild. The earlier version lives on the [`legacy`](https://github.com/mor10/tinygame/tree/legacy) branch and the `legacy-v0` tag.

## Make a game with an AI coding agent

Open this folder in your coding agent (Claude Code, Codex, Cursor, or any other) and ask for a
game, for example: *"Make a game where a cat catches falling fish."*

The agent finds its instructions in `AGENTS.md` (Claude Code also gets the `make-a-tinygame`
skill). It writes the game into `games/`, checks it in the simulator, looks at a screenshot,
fixes problems, and can send it to the console. In testing, 20 out of 20 kid-style requests
produced a working game on the first try.

## Do it yourself

You need `cmake` and a C compiler (on a Mac: `xcode-select --install` and `brew install cmake`).

```bash
./tg setup                          # build the simulator, once
./tg new space-cats                 # start games/space-cats.lua from a template
./tg check games/space-cats.lua     # check it; screenshot in out/space-cats.png
./tg play games/space-cats.lua --turn right@10:5 --press right@30
./tg send games/space-cats.lua      # send it to a TinyGame plugged in over USB
./tg serve                          # simulator and uploader in the browser
```

With `./tg serve` running:

- **Simulator:** <http://localhost:8766/sim/web/>. Play any game with the keyboard (A/D and S for
  the left knob, J/L and K for the right, arrow keys to tilt, space to shake), or drop a `.lua`
  file on the page.
- **Uploader:** <http://localhost:8766/uploader/> in Chrome or Edge. Drop a game, see it checked
  and previewed, and send it to the console over USB with no other tools.

## What's where

| Path | What |
|---|---|
| `games/` | your games (`_template.lua` is the starting point) |
| `docs/api-card.md` | everything a game can use, on one page |
| `tg` | the helper command above |
| `console/` | the console firmware (ESP-IDF) |
| `components/tinygame/` | the engine, shared by the console, the simulator and the browser |
| `sim/` | the command-line simulator and the browser simulator page |
| `uploader/` | the browser uploader page |
| `web-engine/` | the engine compiled for browsers (rebuilt with `./tg web-build`) |
| `firmware/` | the screen lab used to tune the display |
| `docs/scope-v0.1.md` | the original plan |
| `CLAUDE.md` | the developer notes: hardware, firmware, measurements |

## Copilot and local checks

Copilot reads `.github/copilot-instructions.md`, which points to the shared cart and developer
guidance rather than maintaining another copy. The Copilot app configuration in
`.github/github-app.yml` provides manual **Set up simulator** and **Browser simulator** commands.
Review and trust that configuration when prompted; neither command accesses hardware.
`.github/workflows/copilot-setup-steps.yml` prepares Node.js and the native simulator for
Copilot cloud agent sessions once the workflow is on the default branch.

The repository extension in `.github/extensions/tinygame-preview/` adds a **TinyGame live preview**
Canvas in compatible Copilot app/CLI versions. Ask Copilot to open it for
`{"cart":"games/brickbreaker.lua"}` (the default cart). The Canvas serves the existing browser
simulator and committed WASM engine on an automatically selected loopback port, so there is no
extra package install, build, or `./tg serve` process to manage. Saved edits to the selected Lua
cart reload within about a second and restart the game; selecting or dropping another cart stops
that live reload. Use a new Canvas instance to preview a different cart. The `status` action
reports the original selected cart and URL. Closing the Canvas closes its server.

If extension support is unavailable, run `./tg serve` and open
<http://localhost:8766/sim/web/?cart=/games/brickbreaker.lua&live=1>. Without the query parameters,
the existing browser simulator behaves as before. Live reload watches Lua source only; engine
changes still require `./tg web-build` and a page reload.

GitHub Actions runs the hardware-free checks below on pushes and pull requests (CMake, a C
compiler, Python 3 and Node.js 22+ are needed). Cart screenshots are kept as workflow artifacts.

```bash
./tg setup
for cart in games/*.lua; do ./tg check "$cart" || exit; done
node --check .github/extensions/tinygame-preview/extension.mjs
node --check sim/web/app.js
node --test .github/extensions/tinygame-preview/server.test.mjs sim/web/preview.test.mjs sim/web/parity.test.mjs
```

The Node tests cover preview routing/live reload and compare 300 frames of each cart between
the native simulator and the committed browser engine. A parity failure after an engine change
means the browser assets need rebuilding with `./tg web-build`. These checks never flash,
upload to, or open a serial connection to the console.

## The console

Hold the board with the screen facing you. By default the right knob scrolls and either knob
selects; Settings > Hands swaps sides for left-handed players. In a game, clicking both knobs
pauses (the menu has Continue, Settings, Sleep and Exit game), and holding both for 3 seconds
leaves the game; outside a game the same hold puts the console to sleep. Any click wakes it, back
into the paused game if one was running. On battery it sleeps by itself after 3, 5 (default) or
10 minutes of no use (Settings > Sleep after); it never does that while plugged into USB. A
battery icon shows on the main screens when a battery is connected. Holding the right knob while
powering on starts safe mode.

To build and flash the firmware, install ESP-IDF v5.5 in `~/esp/esp-idf` (see the Espressif
getting-started guide), plug in the board, and run `./tg flash`.
