# TinyGame v2 (in progress)

A rebuild of TinyGame as a game platform: a C++ engine that runs games written
as text files (art, levels, settings and small Lua rules) on two boards.

- Plan of record: [docs/spec.md](docs/spec.md)
- Decisions made so far: [docs/decisions/](docs/decisions/)
- v1 (the working console firmware) is the `v1.0` tag.

## What exists today

The first slices, running on a computer only: a portable renderer, the test
card used to choose the block size and palette, and the engine core (fixed
60 Hz clock, stage layout for both screens, tile maps, camera) shown by a
scrolling demo with golden-image tests.

```bash
python3 tools/tg.py test
```

```bash
python3 tools/tg.py testcard
```

```bash
python3 tools/tg.py demo
```

`testcard` and `demo` write PNGs for both screens under `out/`. They need Python 3
and a C++17 compiler (`c++`, or set `CXX`). Nothing here runs on the console
yet.

## Packing and checking

- `python3 tools/tg.py check [game]` looks for mistakes in a game (or in every game and starter) and says how to fix each one.
- `python3 tools/tg.py pack <game> [-o file]` packs a game into one `.tgb` bundle (default `out/<id>.tgb`). It refuses a game that has problems.
- `python3 tools/tg.py new <id> --kind platformer [--name "NAME"]` starts a new game in `games/<id>/` from a starter in `starters/`.

All three take `--json`. The bundle format is [docs/formats/bundle.md](docs/formats/bundle.md).
