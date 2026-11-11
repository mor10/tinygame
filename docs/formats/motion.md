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
- Spawns place the actor's bottom-left block on the spawn cell. Only the first
  spawn of a player kind becomes the player. Spawns beyond the 64-actor pool are
  dropped. A player who starts exactly on a solid or one-way tile starts on the
  ground.

## Platformer movement
- Gravity: `vy += gravity / 60` each tick, capped at 30 blocks/s.
- Move X by `vx / 60` and resolve against tiles; then move Y by the average of
  `vy` before and after this tick's gravity, divided by 60, and resolve.
  Hitting a wall zeroes `vx` and sets `hit_wall`; landing zeroes `vy` and sets
  `on_ground`; hitting a ceiling zeroes `vy`. Averaging makes a full jump peak
  at `jump_height` to within a few thousandths of a block; using the speed after
  gravity alone peaks about 0.11 block short at the default tuning.
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
- The knob direction is used on the tick the knob turns and on the next 12 ticks.
- Between tilt 0.15 and 0.6 the speed rises in a straight line from zero to `run_speed`.
- Jump input is read at the start of a tick, using where the player was at the end of the previous tick. A press jumps if the player is on the ground at the start of any of the next 6 ticks, and not at the 7th. After leaving the ground, a press on any of the next 6 ticks still jumps, and not on the 7th. A jump uses up both allowances, so there is no double jump.
- Only a release halves the upward speed, once per jump, and only while the player is moving up.

## Stock behaviours
- `walker`: `vx = facing * speed`. Turns round when `hit_wall` is set, or when
  on the ground and the cell just below its leading bottom corner is neither
  solid nor one-way. The leading bottom corner is in the last column its box
  overlaps on the side it is walking toward, so it turns once it is actually
  over a column with no floor. Starts facing left.
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
drawn); then the status area showing `SCORE <n>`. On an error the screen shows
GAME STOPPED and the message instead.

## Probe text
Lines are `tick <n>`, `score <n>`, `hurts <n>`, `sparkles <n>` (calls to
`fx.sparkle`), then one `actor <slot> <kind> x=.. y=.. vx=.. vy=.. ground=0|1`
line per living actor. The slot number is the actor's place in the pool, the
same number Lua actor handles use.
