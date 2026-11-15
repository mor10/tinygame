---
name: make-a-tinygame
description: Write, fix or change a TinyGame game (a "cart": one Lua file for the TinyGame console with two knobs, tilt and a 320x172 screen). Use for any request to make a game, change how a game plays or looks, or fix a cart that shows an error.
---

# Making a TinyGame cart

TinyGame carts are single Lua 5.4 files written against a small API. Everything a cart can use
is in `docs/api-card.md`: read it first, every time, and use only what it lists. TinyGame is not
PICO-8, LOVE2D or pygame, and the checker rejects their function names.

## Examples to copy from

| Cart | Shows |
|---|---|
| `console/carts/brick.lua` | sprites, a tile map as a brick wall, `map_hit`/`map_set`, lives |
| `console/carts/examples/rocks.lua` | `group()` of falling things, circles, tilt plus knob steering, a saved best score |
| `console/carts/examples/tilt.lua` | tilt with a knob fallback, a maze map, coins, a "which way is down" marker |
| `console/carts/examples/walkwalk.lua` | a scrolling platformer: gravity, jumping, `gfx.map` with a camera |
| `console/carts/examples/snake.lua` | a grid game steered with knob notches, game over and restart |
| `console/carts/examples/paint.lua` | `knob.pos` for direct control, a picture that stays between frames, shake |

## The loop

1. Write the whole cart to a `.lua` file, for example `carts/<name>.lua`.
2. Check it: `sim/build/tinygame-sim --check carts/<name>.lua --png /tmp/<name>.png`
   - It lints the code, checks the `cart` table, then plays 300 frames twice: once with no
     input and once with busy knob and tilt input. Unknown names are errors.
   - Fix every `"level":"error"` line and rerun until it prints `"ok":1`. Fix warnings too
     (a missing tagline or icon makes the gallery look broken).
   - If `sim/build/tinygame-sim` is missing, build it: `cmake -S sim -B sim/build && cmake --build sim/build`.
3. Look at the screenshot (the PNG). It is the 300th frame with no input: check that the game
   is visible, readable and centred, not a blank or black screen.
4. To see gameplay, script input and take another screenshot:
   `sim/build/tinygame-sim carts/<name>.lua --frames 120 --turn right@10:5 --press right@30 --tilt 0.6,0@40 --png /tmp/play.png`
   (`--turn side@frame:notches`, `--press side@frame`, `--tilt x,y@frame` holds from that frame).
   Add `sys.log(...)` lines temporarily to print positions or scores if something seems off.
5. Send it to the console: the parent opens `uploader/` in Chrome, or with the board on USB:
   `~/.espressif/python_env/idf5.5_py3.12_env/bin/python console/tools/send_cart.py /dev/cu.usbmodem* carts/<name>.lua`.

## What makes a good cart here

- A `cart` table with `title` (23 characters at most), `tagline` (47 at most), `api = 1`,
  `fps = 30` and a 16x16 `icon` that shows what the game is about.
- One clear control scheme, said on screen in a few words ("Turn right knob to steer").
  Clicking both knobs together is the console's pause, so don't use it in a game.
- Readable text: light colours (4, 5, 8, 12) on dark backgrounds (0, 1, 2), or navy on cream.
- A way to lose and start again, and a reason to replay (score, time or a saved best).
- Short: under about 150 lines plus art. Kids read these.
- Tilt games always have a knob way to play and show which way is down.
- When fixing an error report, change only what the error needs, then run the check again.
