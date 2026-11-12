# TinyGame: agent guide

TinyGame is a tiny handheld game console: an ESP32-S3 with a 320×172 screen, two continuous rotary encoders with click switches, and a tilt sensor. Kids and grown-ups use coding agents to make games for it and share them. Users are often around 10 years old, so explain things simply.

## Hard rules

- **Say "knobs" to players** ("left knob", "right knob", "main knob") in device text, game instructions and player docs. Code and hardware docs may say "encoder".
- **Inputs** are the two knobs (turn + click) and the IMU (accelerometer and gyro). The RGB LED is the only extra output; there is no sound or touch.
- **Main knob:** `tg::Input::main` is the right knob, or the left one when Settings > HANDEDNESS = LEFT. `left`/`right` are the physical knobs. Click, press, release and held state merge both knobs, so either click is the action button.
- **Turning is continuous** (no detents): `delta` counts signed transitions, 80 per turn, clockwise positive. Menus take 4 per step and clamp at their ends. Steer positions with `tg::Steer` + `tg::glide` and choose options with `tg::Picker`, so every game feels the same.
- **Individual holds belong to games.** A solo knob hold never pauses or navigates away; games may use `down`, `held_ms`, and `released`.
- **Pause / Sleep chord:** when both knobs are down, the OS captures both inputs and freezes the game. Releasing both before three continuous seconds opens Pause (or goes back in menus); keeping both down for three seconds opens the Yes/No sleep prompt. The chord and its releases are swallowed. Release both, then make a fresh click to confirm; No restores the prior game/menu state.
- **Sleep:** System > Sleep or the auto-off idle timeout enters deep sleep, and a knob click wakes it. Settings > AUTO-OFF defaults to ON; OFF AFTER offers 1, 5 (default), 10, 15 or 30 minutes. Turns, clicks and holds count as activity; tilt doesn't. An active USB CDC serial host suspends auto-off; USB power alone doesn't. Closing the host starts a fresh countdown. Manual Sleep still works. Game state isn't kept. Never add blocking wait loops; see the `waveshare-esp32-s3-lcd-147b` skill.
- **Screen:** 320×172 landscape, dark mode only, styled by the style library (`include/tg_style.h`). Use its tokens. Raw hex colours are only for game-specific art inside a game folder.
- **LVGL 9.6 APIs only.** Many examples online are LVGL 8.
- **Pins:** never invent or change them. Map physical left/right only in `src/hal/board_config.h`, never in games or handedness code. Hardware facts are in the `waveshare-esp32-s3-lcd-147b` skill and `docs/wiring.md`.
- **Stability:** no `delay()` in the loop, no per-frame allocations, and no creating or deleting LVGL objects every frame.
- **Timing:** `update()` gets elapsed seconds, not a fixed 60 Hz. Keep fractional time and use fixed steps for physics.
- **Simulator parity:** games use the same C++ OS, LVGL and game sources on the board and in WebAssembly. Never add a browser-only copy of a game's rules or controls. Keyboard/mouse inputs map to the existing knobs and tilt, not extra game actions.
- **Scope:** reviews and plans only inspect: no edits, scaffolding or flashing until approved. Fixing a game edits that game; don't scaffold another one.
- **Be honest:** never report a failed command, build, flash or save as success.

## Working loop

- Read this guide and the matching skill once. For a game change, start with
  only that game's README, adapter, model/view and relevant tests; follow
  dependencies only when needed. Load targeted references, not every game.
- Ask one question at a time, only for a blocking design decision; at most
  three unless the user requests iterative ideation. State reversible defaults
  and proceed. Preserve explicit requirements; do not substitute an easier design.
- Turn requirements into a short acceptance checklist before editing:
  controls and expected movement, win/loss, visual geometry/scale and timing
  as applicable. Use exact requested values, not just "looks good".
- Make one coherent change, then verify against that checklist. Batch related
  commands and scenarios. Do not rescan, rebuild or repeat a passed check
  without a relevant change; retry a failed check only after addressing its cause.
- For slowness, measure model, rendering and display time on the affected
  target before changing physics or the renderer. Simulator speed is not
  device performance. Compare the same scene before and after.
- Use existing checks and reuse passing results for unchanged code. Report
  blocked checks honestly; declare completion once per request, only after
  all requested work is verified or a genuine blocker is explained.
- Keep durable guidance here or in its owning skill/reference. Replace stale
  facts; do not add duplicate knowledge files, run histories or private reasoning.

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
tools/sim_build.py      pinned cached Emscripten/LVGL, incremental WASM build
sim/                   Arduino/HAL replacements, browser UI and loopback server
.github/extensions/tinygame-simulator/  Copilot canvas; action/setup reference
docs/wiring.md          encoder wiring and the sleep/wake design
```

## Commands

```bash
python3 tools/tg.py new <id> "<NAME>"   # new game from the template
python3 tools/tg.py doctor              # check host prerequisites and USB discovery
python3 tools/tg.py build               # compile default 1.47B firmware
python3 tools/tg.py build --board 169   # compile 1.69 firmware
python3 tools/tg.py flash --board 147b # compile + upload to the explicit profile
python3 tools/tg.py test                # host unit tests (OS, tools, every game)
python3 tools/tg.py monitor -s 10       # read the serial log
python3 tools/tg.py send "launch <id>" "turn right 20" click "tilt 0.5 0" "wait 1"
python3 tools/tg.py shot [out.png]      # screenshot; read it to check your work
python3 tools/tg.py sim                # real firmware in a local browser/canvas
python3 tools/tg.py sim --build-only   # build WebAssembly without serving
python3 tools/tg.py bridge             # USB console as simulator controller
```

`flash` requires `--board 147b|169`; `flash`, `monitor`, `send`, `shot` and `bridge` take `--port <device>`, required when several boards are connected. For `sim`, `--port` is an HTTP port number, not a serial device.

Dev commands for `send`: `turn <left|right> <signed-transitions>`, `click|hold|down|up [left|right]` (default left), `tilt <x> <y>|off`, `launch <id>`, `info`, `log on|off`, `wait <seconds>`. Simulated input counts as activity for the idle timer but can't wake the board. Release held or injected input when done (a stuck `down` blocks idle sleep and hides that knob's real presses): `send "up left" "up right" "tilt off"`. System > Diagnostics shows live knob readings (see the README).

## Simulator workflow

Use the simulator for visual and interaction checks during game development,
even without hardware. Read `.github/extensions/tinygame-simulator/README.md`
for setup, controls, action schemas and troubleshooting. If setup is unavailable,
report the blocked check instead of claiming visual verification.

- In Copilot, discover `tinygame-simulator`, open it with optional
  `{ "game": "<id>" }`, and wait for `get_output` to report `pageReady: true`
  and `status: "ready"`. Keep the same instance handle for subsequent actions.
  Keep one page per server URL; do not reopen a ready canvas between scenarios.
  If discovery fails, try one extension reload or use `tg.py sim --no-open`
  with the Browser canvas. The fallback does not verify extension RPC routing.
- Use `launch`, `send`, `screenshot` and `get_output` to exercise the change.
  Start with three scenarios: core action, pause/resume/restart, and the
  changed edge case; add only uncovered acceptance criteria. Batch commands
  in `send`, read logs incrementally using `next` as `since`, and save/inspect
  representative screenshots, not every intermediate frame.
  CLI `tg.py send/shot` target USB, not the browser.
- Saves under `src/`, `include/`, `sim/`, or text game sources under `games/`
  trigger rebuild/reload. A compile error leaves the previous
  firmware visible, so check `buildError` before calling a preview current.
  `reload` restarts loaded firmware, not a build. Reuse a successful watcher
  build; do not rebuild unchanged sources just to obtain the same evidence.
- USB controller mode forwards physical knobs and tilt at about 50 Hz.
  Disconnect it before any physical `flash/send/shot/monitor`, and reconnect
  afterward only if the user is playing with the board. Closing the page
  releases the bridge after its page-heartbeat timeout. Report any deliberately
  left-running server or controller connection, and don't leave injected holds.
- The simulator synthesizes acceleration from tilt and has no gyro gestures.
  USB bridging also sends tilt only. Verify shake/flick, real knob feel,
  sensor direction, electrical faults and deep-sleep power on the device.

## Definition of done

1. For firmware/game changes, `tg.py test` and the firmware build pass with no
   new warnings. `tg.py test` has no game filter; run it once after the edit
   batch. `tg.py flash --board <147b|169>` includes the build, so a separate `build` is unnecessary
   when flashing the same sources.
2. For game/OS behaviour or visuals, the WebAssembly build passes (via
   `tg.py sim --build-only` or the simulator watcher); exercise the acceptance
   scenarios above and inspect saved screenshots. A boot alone is not enough.
3. For firmware/game changes with a board attached, disconnect any controller
   bridge, run `tg.py flash --board <147b|169>`, drive the change with `send` and inspect a `shot`.
   Browser-only UI changes do not need a firmware flash.
4. Say what remains physical: knob feel/direction, tilt direction, LED colours
   and gyro gestures. Documentation-only changes need a consistency/diff
   check, not tests, builds, simulator startup or flashing.

## Skills

- `make-a-game`: making, changing, fixing or sharing games (API, rules, template, recipes, style guide).
- `waveshare-esp32-s3-lcd-147b`: board, drivers, pins, sleep/wake, PlatformIO and hardware debugging.

`.claude/skills/` is the single source. `.agents/skills/` holds symlinks to it (for Codex), so never copy skills. If your harness doesn't list a skill, read its `SKILL.md` directly. Claude Code reads this file through `CLAUDE.md`, and Copilot also reads `.github/copilot-instructions.md`.
