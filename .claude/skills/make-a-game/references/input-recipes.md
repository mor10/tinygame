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

- Individual knob holds remain available to the game; there is no solo-hold timeout for Pause.
- When both knobs are down, the OS captures the chord and pauses game updates. Releasing both before three continuous seconds opens Pause (or goes back in menus); holding both continuously for three seconds opens Sleep confirmation. The chord and its releases are swallowed.
- `click` comes on release, so it feels about 0.1 s slower than `pressed`. For reaction games, use `pressed`.
- After Pause or Sleep confirmation, release both knobs and use a fresh click to confirm or resume. Do not rely on the captured chord to trigger an in-game action.

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

## Testing in the simulator

Run `python3 tools/tg.py sim` or open the `tinygame-simulator` Copilot canvas.
The real OS and game code run with browser HAL replacements; don't create a
JavaScript version of the game. Source saves rebuild/reload to a fresh run.

| Knob | Counterclockwise | Click / hold | Clockwise |
|---|---|---|---|
| Left | A | S | D |
| Right | J | K | L |

Each turn key tap sends four transitions; holding repeats. S/K stay down until
released. Hold both together and release before three seconds to test the real
OS pause/back path; hold both for three seconds to test Sleep. Drag a
knob's rim or scroll to turn it, and press its centre to click/hold. Use arrow
keys or drag the screen for tilt, returning to level on release.

For the canvas, wait for `get_output` to report ready, then call:

```json
{"game":"mygame"}
```

as input to `launch`, followed by this input to `send`:

```json
{"commands":["turn right -40","wait 0.5","turn right 40","click","tilt -1 0","wait 1","up left","up right","tilt off"]}
```

Use `screenshot` with `{ "label": "mygame" }` to save a 320x172 PNG and inspect
it. Use `get_output`'s `next` cursor as the next `since` value when reading logs;
check `buildError` so you don't mistake the last good build for the current edit.
Full action schemas and discovery instructions live in
`.github/extensions/tinygame-simulator/README.md`.

In a normal browser, expand Serial log and enter the same commands separately;
use its Screenshot button. `wait` is supported by the command field/canvas
`send`, not by the physical firmware parser. `hold` is a convenient injected
OS event; use a held S/K or mouse centre press when testing actual timing.

With **Use connected console**, physical knobs and tilt control the simulator
alongside browser input. Flash controller-enabled firmware once, and disconnect
the bridge before CLI serial tools. The bridge sends tilt, not raw acceleration
or gyro. The browser synthesizes gravity from tilt; verify shake/flick/spin
gestures with the actual game running on the device.

## Testing on the physical console from the computer

```bash
python3 tools/tg.py send "launch mygame" "turn right -40" "wait 0.5" "turn right 40" click "tilt -1 0" "wait 1"
python3 tools/tg.py shot
python3 tools/tg.py send "up left" "up right" "tilt off"   # release anything still held
```

`turn` is signed transitions (80 per revolution, clockwise positive) and `click` is a full press and release. `down` holds a knob until `up`, which exercises the held state. To test Pause, hold both knobs with `down left` and `down right`, then release both with `up left` and `up right` before three seconds. Hold both for three seconds to test Sleep confirmation. `hold` injects a legacy long-press event for diagnostics; it does not open Pause. Each command takes `left` or `right` (default `left`) where applicable. Handedness decides which knob is `main`, so turn the right knob unless Settings says LEFT. The CLI exits non-zero if the board reports an error or doesn't answer.
These CLI commands target the USB board, not a running simulator. Disconnect
**Use connected console** first so the serial port is available.
