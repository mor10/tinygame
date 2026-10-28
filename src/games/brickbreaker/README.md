# BRICK BREAKER

**Made by:** the TinyGame team (the first game on the console)

## How to play

- **Main knob** (right knob, or left knob in left-handed mode): move the paddle. Turn slowly to aim precisely, spin fast to cross the screen
- **Click** (either knob): launch the ball; with the Laser power-up, fire
- **Hold:** pause menu (Resume / Restart / Quit)
- Clear every brick to reach the next level. Grey bricks take two hits.

## Power-ups

A broken brick has an 18% chance to drop a capsule. Catch it with the paddle.

| Capsule | Colour | Effect |
|---|---|---|
| Wide | blue | Wider paddle for 12 s |
| Laser | pink | Click fires two bolts for 12 s |
| Multi | orange | Splits the ball into 3 |
| Big | yellow | Triple-radius ball for 10 s |
| Life | green | +1 life (max 9) |

## How it's built

- `game.cpp`, `physics.cpp`, `level.cpp`: the simulation. Pure C++ with no
  LVGL, tested by `test/` (`python3 tools/tg.py test`).
  - Fixed 120 Hz steps, with time clamped after stalls.
  - The paddle bounce angle depends on where the ball hits.
  - The ball always keeps some vertical speed.
  - Six level layouts repeat with more speed.
  - Power-up drops use a seeded random generator, so tests are repeatable.
- `view.cpp`: draws the simulation with long-lived LVGL blocks. They're only
  updated when something changes: square ball, palette-coloured bricks,
  particle bursts.
- `brickbreaker.cpp`: connects the simulation to TinyGame. It handles input,
  scores, LED flashes, and `TINYGAME(...)`.
- `config.h`: every tuning number (speeds, sizes, drop rates).
