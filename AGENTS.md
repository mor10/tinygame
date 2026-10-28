# TinyGame: agent guide

TinyGame is a tiny handheld game console: an ESP32-S3 with a 320×172 screen, two continuous rotary encoders with click switches, and a tilt sensor. Kids and grown-ups use coding agents to make games for it and share them. Users are often around 10 years old, so explain things simply.

## Hard rules

- **Say "knobs" to players** ("left knob", "right knob", "main knob") in device text, game instructions and player docs. Code and hardware docs may say "encoder".
- **Inputs** are only the two knobs (turn + click) and the IMU (accelerometer and gyro). The RGB LED is the only extra output. There's no pot, no third button, no sound and no touch.
- **Main knob:** `tg::Input::main` is the right knob, or the left one when Settings > HANDEDNESS = LEFT. `left`/`right` are the physical knobs. Click, press, release and held state merge both knobs, so either click is the action button.
- **Turning is continuous** (no detents): `delta` counts signed transitions, 80 per turn, clockwise positive. Menus take 4 per step and clamp at their ends. Steer positions with `tg::Steer` + `tg::glide` and choose options with `tg::Picker`, so every game feels the same.
- **Long press belongs to the OS** (either knob): it opens the pause menu (Resume / Restart / Quit) or goes back in menus. Games never see it.
- **Sleep:** System > Sleep or 3 idle minutes (turns, clicks and holds count as activity; tilt and serial don't) enters deep sleep, and a knob click wakes it. Game state isn't kept. Never add blocking wait loops; see the `waveshare-esp32-s3-lcd-147b` skill.
- **Screen:** 320×172 landscape, dark mode only, styled by the style library (`include/tg_style.h`). Use its tokens. Raw hex colours are only for game-specific art inside a game folder.
- **LVGL 9.6 APIs only.** Many examples online are LVGL 8.
- **Pins:** never invent or change them. Map left/right only in `src/hal/board_config.h` (the final assembly swapped the physical positions, wiring unchanged), never in games or handedness code. Hardware facts are in the `waveshare-esp32-s3-lcd-147b` skill and `docs/wiring.md`.
- **Stability:** no `delay()` in the loop, no per-frame allocations, and no creating or deleting LVGL objects every frame.
- **Timing:** `update()` gets elapsed seconds, not a fixed 60 Hz. Keep fractional time and use fixed steps for physics.
- **Scope:** reviews and plans only inspect: no edits, scaffolding or flashing until approved. Fixing a game edits that game; don't scaffold another one.
- **Be honest:** never report a failed command, build, flash or save as success.

## Layout

```
include/tinygame.h      Game API: tg::Game, TINYGAME(...) macro, scores, LED, log
include/tg_types.h      tg::Input, screen size, Steer/glide/Picker (pure C++, host-testable)
include/tg_random.h     seeded random numbers; each game owns its seed
include/tg_style.h      style library: colours, fonts, spacing, ui helpers
include/lv_conf.h       LVGL config
src/games/<id>/         one folder per game (auto-registered; nothing else to edit)
src/os/                 shell (menus), shell_state (pure logic), style, registry, devtools, settings
src/hal/                board drivers: display, input, imu, rgb_led, storage, power, pins
tests/                  host tests for src/os and tools/tg.py; games keep theirs in src/games/<id>/test/
tools/tg.py             build / flash / test / monitor / send / shot / new
docs/wiring.md          encoder wiring and the sleep/wake design
```

## Commands

```bash
python3 tools/tg.py new <id> "<NAME>"   # new game from the template
python3 tools/tg.py build               # compile firmware
python3 tools/tg.py flash               # compile + upload
python3 tools/tg.py test                # host unit tests (OS, tools, every game)
python3 tools/tg.py monitor -s 10       # read the serial log
python3 tools/tg.py send "launch <id>" "turn right 20" click "tilt 0.5 0" "wait 1"
python3 tools/tg.py shot [out.png]      # screenshot; read it to check your work
```

`flash`, `monitor`, `send` and `shot` take `--port <device>`; it's required when several boards are connected.

Dev commands for `send`: `turn <left|right> <signed-transitions>`, `click|hold|down|up [left|right]` (default left), `tilt <x> <y>|off`, `launch <id>`, `info`, `log on|off`, `wait <seconds>`. Simulated input counts as activity for the idle timer but can't wake the board. Release held or injected input when done (a stuck `down` blocks idle sleep and hides that knob's real presses): `send "up left" "up right" "tilt off"`. System > Diagnostics shows live knob readings (see the README).

## Definition of done

1. `tg.py test` and `tg.py build` pass, with no new warnings.
2. If a board is plugged in, `tg.py flash`, then drive to the change with `send` and check a `shot`.
3. Say clearly what still needs a human to check on the device: feel, knob direction, tilt direction, LED colours. Never claim a screenshot or flash you didn't get.

## Skills

- `make-a-game`: making, changing, fixing or sharing games (API, rules, template, recipes, style guide).
- `waveshare-esp32-s3-lcd-147b`: board, drivers, pins, sleep/wake, PlatformIO and hardware debugging.

`.claude/skills/` is the single source. `.agents/skills/` holds symlinks to it (for Codex), so never copy skills. If your harness doesn't list a skill, read its `SKILL.md` directly. Claude Code reads this file through `CLAUDE.md`, and Copilot also reads `.github/copilot-instructions.md`.
