---
name: make-a-game
description: Create, change, fix, or share a game for the TinyGame console (ESP32 handheld, 320x172 screen, two rotary knobs with click switches + tilt sensor). Use whenever someone wants a new game, wants to change a game's rules, graphics, or controls, has a game bug, or wants to share a game.
---

# Make a TinyGame

Follow `AGENTS.md` for the working loop, simulator checks and definition of done.
Explain meaningful changes simply; don't narrate every tool call.

## Workflow

1. **Choose the task path.**
   - **Create:** establish the game, controls and win/loss rules. Ask only for blocking choices within the root guide's question budget; offer 2–3 ideas only if the user is unsure.
   - **Change/debug:** read the target game's README, adapter, model/view and relevant tests. Reproduce the failed criterion; don't scaffold another game.
   - **Review/plan:** inspect and explain; don't edit source, scaffold or flash before approval.
   - **Share:** follow the sharing checks below and the user's harness workflow.
2. **For a new game only:** `python3 tools/tg.py new <id> "<NAME>"`
   - `id`: lowercase letters, digits and `_`; start with a letter, max 15. No C++ keywords or double underscores. It's also the namespace.
   - `NAME`: 1–13 printable ASCII characters; the tool makes it uppercase.
   - This makes `src/games/<id>/game.cpp`, a small working catch game, and a `README.md`. Rewrite the game from there.
3. **Implement the acceptance checklist:** use the API below and load the style
   guide for screen changes; other references only as needed. Check requested
   dimensions, scale, visibility and animation duration explicitly. For control
   changes, test actual movement, not only an axis coefficient's sign.
4. **Document and verify:** update the game's README with controls, how to play
   and authorship (don't invent a creator). Run the root guide's checks once
   per edit batch; reuse successful builds and capture representative evidence.

The game appears in both the device and simulator launchers automatically.
Don't edit any list, OS file or browser code to add it; keep the same game
rules and `tg::Input` controls on both targets.

## The API (`include/tinygame.h`, `include/tg_types.h`, `include/tg_style.h`)

```cpp
class MyGame : public tg::Game {
  void start(lv_obj_t* screen) override;                // build LVGL objects once
  void update(const tg::Input& in, float dt) override;  // dt = elapsed seconds, variable cadence
  void paused(bool p) override;                         // optional
};
TINYGAME(mygame::MyGame, "mygame", "MY GAME", tg::color::MINT)
```

- **`tg::Input`:**
  - `main`: the main knob (right by default, left in left-handed mode). `main.delta` is signed transitions this update (80 per turn, clockwise positive); use it for smooth steering. `left`/`right` are the physical knobs.
  - `click` (short press on either knob, one update), `pressed`, `released`, `down`, `held_ms`
  - `tilt_x`, `tilt_y` (-1..1, the way a marble would roll)
  - `accel_g`, `gyro_dps` (Vec3, screen axes: +x right, +y down, +z toward the player)
- **Screen:** `tg::WIDTH` 320, `tg::HEIGHT` 172, with (0,0) at the top-left.
- **Standard controls** (use these so every game feels the same):
  - **Steering a position** (paddles, ships, cursors): `tg::Steer steer_{travel_px};` then on each update `target = steer_.update(in.main.delta, dt)` and `x_ = tg::glide(x_, target, dt)`. Slow turns move 2.5 px per transition for precision; fast spins move up to 8 px per transition. `steer_.reset(px)` on a new game; `steer_.fraction()` gives 0..1. Tune with `tg::SteerTuning` only if a game really needs it.
  - **Choosing one of N options** (modes, lanes): `tg::Picker picker_;` then `choice_ = picker_.update(choice_, count, in.main.delta)`. Four transitions per step, clamped at the ends, same as the OS menus.
  - **Rotating or throttling:** add `in.main.delta * DEGREES_PER_TRANSITION` to an angle or value.
  - **Action:** `click`/`pressed` fire from either knob. Use `in.left`/`in.right` only for games that need two independent knobs.
- **Scores:** `tg::bestScore()`, and `tg::submitScore(score)` (returns true on a new best, and only after it's saved; a write failure returns false and is logged). The launcher shows the best score.
- **Randomness:** include `tg_random.h`, keep a `std::uint32_t seed_` in your class and call `tg::randomNext(seed_)`. Seed with `lv_tick_get()` in `start()`, or a fixed value in tests.
- **LED:** `tg::led(rgb)` sets the colour; `tg::flashLed(rgb, ms)` flashes it.
- **Logging:** `tg::log("fmt", ...)` writes to serial. Don't log every update.
- **Drawing helpers** (`tg::ui::`):
  - `label(parent, text, Font, rgb)`, `text(label, fmt, ...)`
  - `block(parent, x, y, w, h, rgb)`, `panel(...)`
  - `header(...)`, `footer(...)`
  - `popup(...)` plus `popupTitle`/`popupValue`/`popupHint`
  - `result(popup, score, best, new_best)` fills a game-over popup with bounded score rows
  - `Menu`

## Rules (the console depends on them)

- **Inputs:** games receive the two knobs and motion sensor through `tg::Input`. Never add other inputs or call `analogRead`, `digitalRead` or `Wire`.
- **Individual holds are game input.** Use `down`, `held_ms`, and `released` for hold actions. When both knobs are down, the OS captures the chord and freezes the game; releasing both before three continuous seconds opens Pause (or goes back in menus), while three seconds opens Sleep confirmation. Chord input and releases are swallowed (see `references/input-recipes.md`).
- **All mutable game state lives in your class.** Constants may be shared. Restart deletes the object and makes a new one, so there are no globals or `static` variables. Don't delete LVGL objects in the destructor; the OS deletes the screen first. Release separately owned buffers in the destructor.
- **Create LVGL objects once in `start()`,** then move, hide, recolour or retext them in `update()`. Never create or delete objects every update, never `delay()`, and don't use `new` each update.
- **Objects:** keep it under about 300. Pool and reuse bullets, particles and so on (see the recipes).
- **LVGL 9.6 only.** Many examples online are LVGL 8. See `references/lvgl-recipes.md` for the renames.
- **Namespace:** put your code in `namespace <id> { }` so names can't clash with other games.
- **Files:** keep everything in `src/games/<id>/`. Never edit `src/os`, `src/hal` or `include/` for one game.
- **Text:** ASCII only. Per line it fits about 40 characters in `Font::S`, 20 in `M`, 13 in `L` and 10 in `XL`.

## Look and feel

Use the style tokens: `tg::color::*` (16-colour palette plus UI colours), `tg::Font::S/M/L/XL`, and the `tg::ui` helpers. The look is dark, square and chunky 8-bit. Colours for game-specific art may differ, but only inside the game's folder. Read `references/style-guide.md` before designing screens.

## Testing logic

Put rules that don't draw anything (movement, collisions, scoring) in their own `.cpp`/`.h` with no `lvgl.h` or `Arduino.h` includes. Then add `src/games/<id>/test/test_<name>.cpp` with a `main()` that uses `assert`. `python3 tools/tg.py test` finds and runs it. `src/games/brickbreaker/` is the full example.

Add a regression test for every bug fix. Cover fixed-step timing, pool limits and restart state when relevant. Don't truncate `dt` to whole milliseconds on each update.

For input changes, feed signed knob transitions or tilt through the game's
input-to-model path and assert the resulting movement/action. Cover both signs
and each independent knob; check handedness when using `main`, and ensure
pause/resume cannot accidentally trigger an action. A screenshot cannot prove
control direction.

## Sharing a game

A game is just its folder.

- **To share:** when asked, commit `src/games/<id>/` and use the user's harness-supported pull-request workflow. Prefer its native PR tool when it has one, and use the `gh` CLI only where the harness allows it. Or send the folder to a friend.
- **Before sharing:** satisfy `AGENTS.md`'s definition of done, ensure the id is unique and the README explains how to play. Reuse completed checks; don't repeat them just to share.
- **To receive a game:** inspect the folder, copy it into `src/games/`, build and test, then flash if a board is connected. If two games share an id, rename the folder, the namespace and the `TINYGAME(...)` id together. The console holds at most 32 games; a game that can't register is skipped and the boot log says why (`[BOOT] rejected game`).

## References (read when needed)

- `references/lvgl-recipes.md`: how to draw things, including sprites, pixel art, pools and LVGL 8→9 renames
- `references/input-recipes.md`: knob steering and pickers, click and tilt recipes, shake detection, timing, driving input from the computer
- `references/style-guide.md`: colours, fonts, layout grid, and do's and don'ts
