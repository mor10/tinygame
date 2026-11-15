# TinyGame

Tiny screen. Twisty knobs. Real games, built by you.

TinyGame is a pocket game console for kids and parents to build, play and share games with AI
help. It's a Waveshare ESP32-S3-LCD-1.47B board with a 320x172 screen, two endless clicky knobs
and a tilt sensor. Games ("carts") are single Lua files.

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

## The console

Hold the board with the screen facing you. By default the right knob scrolls and either knob
selects; Settings > Hands swaps sides for left-handed players. In a game, clicking both knobs
pauses, and holding both for 3 seconds leaves the game. Holding the right knob while powering on
starts safe mode.

To build and flash the firmware, install ESP-IDF v5.5 in `~/esp/esp-idf` (see the Espressif
getting-started guide), plug in the board, and run `./tg flash`.
