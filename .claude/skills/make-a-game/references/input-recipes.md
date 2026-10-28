# Input recipes

`tg::Input in` arrives each OS update. `dt` is elapsed seconds; updates can be
much faster than the display refresh. Retain fractional seconds for motion
and timers instead of rounding every update down to whole milliseconds.

## Encoders

The console has two endless rotary encoders. `in.main` is the main one (right by
default, left in left-handed mode). `in.main.delta` is the signed number of
transitions this frame: 80 per full turn, clockwise positive. There is no
absolute position, so keep your own.

```cpp
// Position (paddles, ships, cursors): the standard. Speed-sensitive, then glided.
tg::Steer steer_{tg::WIDTH - player_w};                    // member
steer_.reset((tg::WIDTH - player_w) / 2.0f);              // new game
x_ = tg::glide(x_, steer_.update(in.main.delta, dt), dt);  // every frame

// Lanes or mode choices: 0..count-1, four transitions per step, clamped.
tg::Picker picker_;                                       // member
lane_ = picker_.update(lane_, 3, in.main.delta);

// Steering wheel / rotation: turn by how far the encoder moved.
angle_ += in.main.delta * DEGREES_PER_TRANSITION;

// Throttle or power meter: nudge and clamp.
power_ = std::clamp(power_ + in.main.delta / 80.0f, 0.0f, 1.0f);
```

Use the `tg::Steer` defaults (`tg::SteerTuning`) so all games feel the same.
If physics runs in fixed steps, glide with the step time and snap on new game.
Use `in.left`/`in.right` only for two-knob games (for example two paddles).

## Click (either encoder)

```cpp
if (in.click) jump();                    // short press (fires on release)
if (in.pressed) startCharging();         // the moment it goes down
if (in.down) charge_ += dt;              // while held
if (in.released) shoot(charge_);         // the moment it comes up
```

- A long press (about 0.65 s) always opens the OS pause menu. Keep charge-up moves shorter, or cap them at about 0.5 s.
- `click` comes on release, so it feels about 0.1 s slower than `pressed`. For reaction games, use `pressed`.
- If the player resumes from pause with a long press, that knob is still held: it reports `down` and a large `held_ms` (but no new `pressed`) until released, and then `released`. Start charge-ups from `pressed`, not from `down` alone.

## Tilt (motion sensor)

```cpp
// Marble or ball rolling: tilt is the direction it would roll.
vx_ += in.tilt_x * GRAVITY * dt;
vy_ += in.tilt_y * GRAVITY * dt;

// Steering with a dead zone, so holding it "almost flat" counts as straight.
float t = in.tilt_x;
if (t > -0.1f && t < 0.1f) t = 0;
x_ += t * SPEED * dt;
```

The tilt is smoothed. For raw values use `in.accel_g` (in g, about 1.0 = gravity) and `in.gyro_dps`.

## Shake and flick

```cpp
// Shake: acceleration much bigger than gravity.
float a2 = in.accel_g.x * in.accel_g.x + in.accel_g.y * in.accel_g.y + in.accel_g.z * in.accel_g.z;
if (a2 > 2.5f * 2.5f && shake_cooldown_ <= 0) { onShake(); shake_cooldown_ = 0.5f; }
shake_cooldown_ -= dt;

// Flick or twist: fast rotation (use the size; check the direction on
// System > Diagnostics before relying on the sign).
if (std::fabs(in.gyro_dps.z) > 300) onTwist();
```

## Timing without delay()

```cpp
timer_ -= dt;
if (timer_ <= 0) { spawnEnemy(); timer_ = 1.5f; }   // every 1.5 s
```

Keep timers as `float` seconds in your class. Never call `delay()`, because it freezes the whole console.

## Testing input from the computer

```bash
python3 tools/tg.py send "launch mygame" "turn right -40" "wait 0.5" "turn right 40" click "tilt -1 0" "wait 1"
python3 tools/tg.py shot
python3 tools/tg.py send "up left" "up right" "tilt off"   # release anything still held
```

`turn` is signed transitions (80 per revolution, clockwise positive) and `click` is a full press and release. `down` holds a knob until `up`, which exercises the held state. `hold` injects the OS long press, which opens pause. Each of these takes `left` or `right` (default `left`). Handedness decides which knob is `main`, so turn the right knob unless Settings says LEFT. The CLI exits non-zero if the board reports an error or doesn't answer.
