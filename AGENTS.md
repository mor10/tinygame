# Making TinyGame games: instructions for coding agents

For every request to create, fix or change a TinyGame game, automatically invoke
`make-a-tinygame` from `.agents/skills/make-a-tinygame/SKILL.md` if it is not already active.
If your agent has no skill invocation mechanism, read that file and follow it directly.
This file remains the shared source of game-making guidance; the skill routes here.

TinyGame is a pocket game console: a 320x172 screen, two endless clicky knobs, and a tilt
sensor. A game ("cart") is one Lua 5.4 file. Your job here is usually to write or change a cart
in `games/`, check it in the simulator, look at it, and send it to the console.

## Read before writing

1. `docs/api-card.md`: everything a cart can use, the limits, and a "Don't" list. Use only what
   it lists. TinyGame is not PICO-8, LOVE2D or pygame; their function names are rejected.
2. `games/_template.lua`, the smallest working cart. There are no golden example carts right now:
   the repository was reset to a clean slate, and games are rebuilt one at a time.

## The loop

1. New game: `./tg new <name>` creates `games/<name>.lua` from a template (names are 1-31 of
   a-z 0-9 _ -). Write the whole cart into it.
2. Check: `./tg check games/<name>.lua`
   - It lints the code, checks the `cart` table, then plays 300 frames twice (no input, then busy
     knob and tilt input). Names from other consoles (`btn`, `spr`, ...) are errors, and so is
     anything that crashes or gets stuck.
   - Fix every ERROR and run it again until it says PASS. Fix warnings too: a missing tagline or
     a broken icon makes the console's game gallery look wrong.
   - The first run builds the simulator (needs cmake and a C compiler; `./tg setup` does it on purpose).
3. Look at `out/<name>.png`, the last checked frame with no input. Is the game visible, readable
   and laid out sensibly? A blank or black screen means something isn't drawn.
4. See gameplay: `./tg play games/<name>.lua --turn right@10:5 --press right@30 --tilt 0.6,0@40`
   and look at `out/<name>-play.png`. `--turn side@frame:notches`, `--press side@frame`,
   `--tilt x,y@frame` (holds from that frame), `--frames N` (default 150). Add `sys.log(...)`
   calls to print values while debugging; they show up in the output. Remove them when done.
5. Only when the user explicitly requests it, send it to a TinyGame on USB:
   `./tg send games/<name>.lua`. It is stored on the console and starts playing.
   If sending is requested but the agent has no USB access, tell the person to run `./tg serve`
   and use the uploader page in Chrome. Otherwise, stop after local checks and previews.

Hardware access (`send`, `flash` and serial commands) always requires an explicit user request.

Run commands from the repository root. Don't edit the engine (`components/`, `console/`, `sim/`)
to make a game work: if the API can't do something, say so.

## What makes a good cart

- A `cart` table with `title` (at most 23 characters), `tagline` (at most 47), `api = 1`,
  `fps = 30`, and a 16x16 `icon` that shows what the game is about.
- One clear control scheme, shown on screen in a few words ("Turn right knob to steer").
  Clicking both knobs together is the console's pause, so don't use it in a game.
- Tell the player the controls from the start, with the control icons: `ui.controls(...)` (a ring-arrow icon means turn, a press-arrow icon means click). Use `ui.sides()` so left-handed players get the knobs swapped.
- Look like the console: no big top bar; overlay only the vital score and lives on the board and put the rest in `stats()` (shown in the pause sidebar). Use `gfx.label` for text and `ui.card` /
  `ui.pill` for level intros and game over (see "Console look" in `docs/api-card.md`). Keep 16 px from
  the sides and below `ui.BAR` (28 px): the rounded bezel hides the corners. Make things chunky: the
  screen is small and hard to read, so favour few big shapes over many small ones.
- Readable colours: light text (4, 5, 8, 12, 18) on dark backgrounds (0, 1, 2, 24, 31), or navy on cream. Colours 16-31 are brighter and purer: use them for sprites and highlights, and 27-29 for pink animals.
- A way to lose and start again, and a reason to replay (score, time or a saved best).
- Short: under about 150 lines plus art. Kids read these and change them.
- Tilt games always have a knob way to play and show which way is down.
- When fixing a reported error, change only what the error needs, then check again.

## Other parts of the repository

The console firmware, simulator and tools have their own notes in
[docs/development.md](docs/development.md). You don't need them to make games.
