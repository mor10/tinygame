# ROCK RAIN

**Made by:** Morten Rand-Hendriksen (with Claude)

Bricks rain down from the sky. Dodge them with your little robot, grab
power-ups, and blast the bricks to pieces.

## How to play

- **Main knob** (right knob, or left knob in left-handed mode): move the robot left and right. Turn slowly to aim precisely, spin fast to cross the screen
- **Press** either knob: shoot. You start with 5 shots; get more from power-ups.
- **Click:** play again after game over
- **Hold:** pause menu (Resume / Restart / Quit)
- You start with 3 hearts and can collect up to 5. A brick that hits you
  takes one, then you blink for a moment and can't be hurt. Bricks that hit the floor do no harm.
- Every game starts with a **wave**: 6 rows of bricks, each with one or two
  openings. Slip through the gaps! You can also shoot a brick out of a row
  to make your own hole.
- The longer you survive, the faster and more often the bricks fall.

## Bricks and points

| Thing | Points |
|---|---|
| Every second you survive | 10 |
| Small orange brick (1 shot) | 50 |
| Big red brick (2 shots, turns peach when cracked) | 150 |
| BOSS brick (10 shots) | 500 |

## The boss

Every now and then a big purple **BOSS** block with an angry face appears. It sinks to the
middle of the screen and slides back and forth, dropping small bricks on you,
until you break it. Its green bar shows how many shots it has left. While the
boss is out, AMMO and RAPID capsules fall more often to help you. Blast it for
500 points and a big explosion! Then a new, faster wave of rows comes down.

## Power-ups

| Capsule | Colour | Effect |
|---|---|---|
| A (Ammo) | blue | +5 shots (max 20) |
| R (Rapid) | pink | 5 seconds of free, fast shooting |
| + (Life) | green | +1 heart (max 5). At least every 7th non-boss capsule |

The LED is green normally, blue when you have ammo, pink in rapid fire, and
flashes red when you get hit.

## How it's built

- `logic.cpp`: the rules (moving, falling, shooting, scoring). Plain C++,
  tested by `test/test_logic.cpp` (`python3 tools/tg.py test`).
- `view.cpp`: draws everything with LVGL objects made once at the start.
- `rockrain.cpp`: connects input, rules, drawing, LED and scores.
- `config.h`: every tuning number. Change these to make the game easier or
  harder.

At the brick pool limit, a shot that would need an extra row section leaves
that row intact rather than erasing unhit bricks. Edge pieces still break.
