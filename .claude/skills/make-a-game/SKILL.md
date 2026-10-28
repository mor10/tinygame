---
name: make-a-game
description: Create, change, fix, or share a game for the TinyGame console (ESP32 handheld, 320x172 screen, two rotary knobs with click switches + tilt sensor). Use whenever someone wants a new game, wants to change a game's rules, graphics, or controls, has a game bug, or wants to share a game.
---

# Make a TinyGame

Read `AGENTS.md` first. You're often helping a **kid (around 10)**, so explain things simply and match the user's experience. Explain meaningful changes, not every tool call. Use screenshots when hardware is available.

## Workflow

1. **Choose the task path.**
   - **Create:** clarify the idea only where needed. Ask at most 3 quick questions: What's the game? What do turning, clicking and tilting do? How do you win or lose? If they're unsure, offer 2–3 ideas that fit the controls.
   - **Change/debug:** read the existing game's README, adapter, model/view and relevant tests. Reproduce the problem; don't create another game.
   - **Review/plan:** inspect and explain; don't edit source, scaffold or flash before approval.
   - **Share:** follow the sharing checks below and the user's harness workflow.
2. **For a new game only:** `python3 tools/tg.py new <id> "<NAME>"`
   - `id`: lowercase letters, digits and `_`; start with a letter, max 15. No C++ keywords or double underscores. It's also the namespace.
   - `NAME`: 1–13 printable ASCII characters; the tool makes it uppercase.
   - This makes `src/games/<id>/game.cpp`, a small working catch game, and a `README.md`. Rewrite the game from there.
3. **Implement and verify:**
   - Use the API below. Load the style guide for screen changes and other references only as needed.
   - `python3 tools/tg.py test`, then `python3 tools/tg.py build`. Fix compile errors before saying anything works.
   - With a board connected: `python3 tools/tg.py flash`, then `send "launch <id>"`, then `shot` to save `shot.png`. Read the PNG and check it.
   - Drive the game with `send "turn right 20" click "tilt 0.5 0" "wait 1"`. `turn` is signed transitions (80 per full turn, clockwise positive). Turn the **right** knob unless Settings says HANDEDNESS = LEFT, because games steer with the main knob.
   - Before finishing, release injected input: `send "up left" "up right" "tilt off"`.
   - Use `--port <device>` when more than one board is connected.
   - Without a board, stop after host and build checks and say which device checks remain (feel, knob direction, tilt, LED). Never claim a screenshot or flash you didn't get.
4. **Fill in `README.md`:** how to play, and who made it.

The game appears in the launcher automatically. Don't edit any list or OS file to add it.

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

- **Inputs:** only the two knobs (via `tg::Input`) and the motion sensor. There is no pot or analog knob. Never call `analogRead`, `digitalRead` or `Wire`.
- **Long press is the OS pause menu.** Games never see it, so don't design a "hold" action. Use `down`/`held_ms` only for charge-up moves shorter than 0.6 s. After a long-press resume the knob is still held, so start actions from `pressed` or `click`, not `down` alone (see `references/input-recipes.md`).
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

## Sharing a game

A game is just its folder.

- **To share:** when asked, commit `src/games/<id>/` and use the user's harness-supported pull-request workflow. Prefer its native PR tool when it has one, and use the `gh` CLI only where the harness allows it. Or send the folder to a friend.
- **Before sharing:** `tg.py build` and `tg.py test` pass, the README says how to play, and the id is unique.
- **To receive a game:** inspect the folder, copy it into `src/games/`, build and test, then flash if a board is connected. If two games share an id, rename the folder, the namespace and the `TINYGAME(...)` id together. The console holds at most 32 games; a game that can't register is skipped and the boot log says why (`[BOOT] rejected game`).

## References (read when needed)

- `references/lvgl-recipes.md`: how to draw things, including sprites, pixel art, pools and LVGL 8→9 renames
- `references/input-recipes.md`: knob steering and pickers, click and tilt recipes, shake detection, timing, driving input from the computer
- `references/style-guide.md`: colours, fonts, layout grid, and do's and don'ts
