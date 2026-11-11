# Rules of motion (first slice)

What the engine does each tick, precisely enough to test. Units are blocks and
seconds; one tick is 1/60 second; y grows downward. An actor's box is its
sprite: `width / 8` by `height / 8` blocks, top-left at (x, y).

## Level and tiles
- Outside the level on the left, right and top counts as solid. Below the
  bottom row is open: an actor whose top passes `rows + 2` has fallen out.
- `solid` blocks movement from every side.
- `one_way` blocks only an actor moving down whose bottom edge was at or above
  the tile's top at the start of the tick.
- `hazard` hurts the player when their box overlaps the tile's cell. It does
  not block movement unless it is also solid.

## Platformer movement
- Gravity: `vy += gravity / 60` each tick, capped at 30 blocks/s.
- Move X by `vx / 60` and resolve against tiles, then move Y by `vy / 60` and
  resolve. Hitting a wall zeroes `vx` and sets `hit_wall`; landing zeroes `vy`
  and sets `on_ground`; hitting a ceiling zeroes `vy`.
- Speed is capped so no actor moves more than 0.5 block per tick on an axis.

## The player (provisional controls; play-test gate in spec M4)
- Run direction: if the main knob turned this tick, its sign is held for the
  next 12 ticks; otherwise `tilt_x` with a 0.15 dead zone, full speed from 0.6.
  `vx = direction * run_speed` (no acceleration in this slice).
- Jump: a press of either knob. Jump speed is `sqrt(2 * gravity * jump_height)`
  upward. A press is remembered for 6 ticks (buffered jump) and the player may
  still jump for 6 ticks after leaving the ground (ledge grace). Releasing
  the knob while moving up halves `vy` once (variable height).
- The player faces the last non-zero run direction.

## Stock behaviours
- `walker`: `vx = facing * speed`. Turns round when `hit_wall` is set, or when
  on the ground and the cell just below its leading bottom corner is neither
  solid nor one-way. Starts facing left.
- `pickup`: when the player's box overlaps it, it is removed and `points` is
  added to the score.
- `on_touch_player = hurt` and hazard tiles: the player is hurt. Being hurt, or
  falling out of the level, increments the `hurts` counter and puts the player
  back at their spawn with zero velocity. Other actors that fall out are removed.

## Order within a tick
1. Player input, then movement and behaviours for every actor in pool order.
2. Stock touches: pickups, then hurt.
3. Lua `on.touch` for each overlapping pair, then `on.tick`.
4. Camera follows the player (dead zone: a quarter of the stage wide, half tall).

## Drawing
Background colour; tile layer; actors in pool order (flipped when facing
left; the player's sprite faces right as drawn, a walker's faces left as
drawn); then the status area showing `SCORE <n>`.
