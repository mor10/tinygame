# TinyGame agent index

TinyGame is a pocket game console (ESP32-S3, two knobs with click, tilt sensor,
two tiny screens: 320x172 and 280x240). Kids around 10 and adults make games
for it by talking to a coding agent. Explain things simply.

This is the v2 rebuild. It is at an early stage: the drawing core, stage
layout, tile maps, camera and a scrolling demo exist, on the host only. There
are no games, no Lua and no firmware yet. [docs/spec.md](docs/spec.md) is the
plan of record.

**The gate:** `python3 tools/tg.py test` must pass before you say a change is
done. (`tg verify` replaces it when games exist; spec section 12.)

Open the page that owns the task:

- What to build, and in what order: [docs/spec.md](docs/spec.md), section 17
- Why something was decided: [docs/decisions/](docs/decisions/). Read the
  record before proposing to change a decision.
- Drawing code: `engine/gfx/`; stage, camera and tile maps: `engine/world/`;
  clock, random and input: `engine/core/`; tests: `engine/test/`
- A renderer change altered a golden frame: look at `python3 tools/tg.py demo`,
  then `python3 tools/tg.py golden --update` only if the change is intended
- Test card (block size and palette choice): `engine/testcard/`,
  `python3 tools/tg.py testcard`
- Risk spikes and their results: [docs/spikes/](docs/spikes/)
- v1 code to carry over: `git show v1.0:<path>`; the list is in spec section 4

Rules for every task:

- `engine/` is portable C++17: no Arduino, LVGL or browser includes.
- Never invent a pin number. Pins come from v1 `src/hal/board_config.h` or the
  vendor schematic.
- Games use original names, characters, art and levels, even when the request
  is "like" an existing game.
- Items marked GATE in the spec need a person. Stop and ask, then record the
  answer in `docs/decisions/`.
- Never report a failed build, test or flash as success. Nothing about real
  hardware is "working" on the strength of a host or simulator run.
