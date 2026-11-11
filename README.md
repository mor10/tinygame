# TinyGame v2 (in progress)

A rebuild of TinyGame as a game platform: a C++ engine that runs games written
as text files (art, levels, settings and small Lua rules) on two boards.

- Plan of record: [docs/spec.md](docs/spec.md)
- Decisions made so far: [docs/decisions/](docs/decisions/)
- v1 (the working console firmware) is the `v1.0` tag.

## What exists today

On a computer: a portable renderer, the engine (60 Hz clock, stage layout,
tile maps, camera, platformer physics, a sandboxed Lua rules host), a packer
for game folders, one sample game (`games/cave_hop/`) and a browser simulator
that plays packed games. The firmware for Board A compiles (it runs the
scrolling demo) but has not been tried on hardware.

```bash
python3 tools/tg.py verify          # the gate: checks, tests, golden frames, game steps
python3 tools/tg.py sim cave_hop    # play cave_hop in the browser simulator
python3 tools/tg.py run cave_hop --steps games/cave_hop/test/play.steps --probe
```

`tg run` plays a small script of inputs and checks, see
[docs/formats/steps.md](docs/formats/steps.md). `testcard` and `demo` write PNGs
for both screens under `out/`. You need Python 3 and a C++17 compiler (`c++`,
or set `CXX`); the simulator also needs Node.js 22 and its Emscripten SDK
(installed once by `python3 tools/sim_build.py`).

## Packing and checking

- `python3 tools/tg.py check [game]` looks for mistakes in a game (or in every game and starter) and says how to fix each one.
- `python3 tools/tg.py pack <game> [-o file]` packs a game into one `.tgb` bundle (default `out/<id>.tgb`). It refuses a game that has problems.
- `python3 tools/tg.py new <id> --kind platformer [--name "NAME"]` starts a new game in `games/<id>/` from a starter in `starters/`.

All three take `--json`. The bundle format is [docs/formats/bundle.md](docs/formats/bundle.md).
