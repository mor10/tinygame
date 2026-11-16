# TinyTwist / TinyGame — working notes for Claude Code

**Making or changing a game? Follow `AGENTS.md` (the `make-a-tinygame` skill points there) and use `./tg`.** The rest of this file is about the console, the engine and the tools.

TinyGame is a pocket game console for kids and parents to build, play and share games with AI help.
Tagline: "Tiny screen. Twisty knobs. Real games, built by you."

- Board: Waveshare ESP32-S3-LCD-1.47B (default, profile `147b` in `hardware.yaml`). The 1.69 board comes in only once the platform is stable.
- The board is mounted turned 90 degrees clockwise, so the logical screen is 320x172 landscape.
- Input: two undetented EC11 encoders with clicks (80 transitions per turn) plus the onboard QMI8658 IMU.
- Games will be written in Lua 5.4. The full plan is in `docs/scope-v0.1.md`, which is a reference, not doctrine.
- `hardware.yaml` holds the board facts from earlier experiments. It is the source of truth for pins, offsets and MADCTL.

## Phase status

**Phase 0, screen lab: done (2026-11-13).** Settings and measurements are in `hardware.yaml` under `lab_result`. In short: `sw` mode (native portrait, rotated in software), 80 MHz SPI, vendor-style panel init, FRCTRL2 `0x00`, porch 1, diff flush, locked 30 fps. Game-like content (falling blocks, parallax scroller) looks clean even with forced full-frame writes. A fast full-width line (`hline` at speed 8) still glitches because there is no TE pin. The panel scans left to right on the landscape screen. 60 fps was much worse and stays opt-in. The lab code stays in `firmware/` as the bench tool.

**Phase 1 (display): done.** A 10-minute soak held 30 fps with no late frames or resets.

**Phase 2 (input): done.** PCNT encoders count exactly, and clicks, long press and both chords work. The IMU axis map is confirmed (see `imu_map` in `hardware.yaml`).

**Phase 3 (Lua core): done (2026-11-14).** It lives in `console/`; see the Console section below. An infinite loop reaches a readable "stuck" screen in about 0.2 s with no reset. All failure tests in `console/carts/tests` pass. Still open: the explicit GC step after `draw()` and GC pause measurement aren't done.

**Phase 4 (engine v1): done (2026-11-14).** Sprites, maps, `hit`, `group`, the helpers and lowercase text are in; the format is in `docs/engine-v1.md`. Brick Breaker (`console/carts/brick.lua`) ran 175 s on autoplay at 30 fps with no late frames and a p99 frame time of 2 ms (budget 33 ms). The explicit GC step after `draw()` costs 0.1-0.4 ms.

**Phase 5 (simulator + CLI): done (2026-11-14).** `sim/` builds the same engine natively and to WebAssembly. Board, native simulator and browser engine produce identical frame hashes on `hello`, `engine_selftest`, `determinism` (600 frames of random numbers, trig and moving objects) and Brick Breaker on autoplay (900 frames). In the browser the engine takes about 0.1 ms per frame.

**Phase 6 (OS shell): done (2026-11-14), restyled to the current look since.** `components/tinygame/os.c` has Welcome, the game shelf, Game Card, Settings (light, hands), Paused, the error state and the built-in Colour Card. The screens are in `docs/ref-designs/design-OS.png` and the colours, type and components in `docs/ref-designs/design-system.png` (regenerate both with `docs/ref-designs/make_refs.py`).
  - It is written in C, not as a Lua system cart, so the shell can't hit a Lua error and needs no special API.
  - Controls: right-handed (the default) turns the right knob to scroll and adjust; left-handed turns the left knob. A click on either knob selects. Back is the scrollable "<" on the shelf and game card, and the Back row in Settings. In a game, clicking both knobs pauses and holding both for 3 s leaves the game.
  - Look ("Deep Toy Blue"): a flat electric-blue field with white type, one yellow action block per screen, plain text lists with a pointer and a yellow underline, and a bobbing isometric block hero on Welcome. Blocks and cartridges have a drop-edge 4 px down and right, in a colour 20% darker than the block. Type is anti-aliased Space Grotesk at four sizes (`components/tinygame/fonts/`, regenerate with `gen_font.py`).
  - Game shelf: notched cartridges in a strip centred on the selected one, which is bigger and raised. Cartridges at the ends run off the screen to show there are more. Each game gets its own cartridge colour and icon-window colour, picked from its icon by contrast (`style_entry` in `os.c`), so any icon stays readable.
  - Palette slots (`gfx.h`, `gfx.c`): 0-31 the game palette (0-15 the original soft colours, 16-31 brighter and purer, with pinks for animals), 32-63 dimmed copies for the pause screen, 64-74 the OS colours, 80-105 block tones, cartridge windows and drop-shadows, and blended text edges from 112. Carts can only use 0-31, so they can't break the OS screens.
  - Colour Card: a built-in entry in the gallery (`console/carts/system/colorcard.lua` is a stand-in cart; the OS recognises the id `colorcard` and draws its own pages): SMPTE bars and greys, RGB and grey ramps, the OS colours, the 32 game colours, and a Tune page that changes the per-channel correction live. Use it to judge colour on the board.
  - Panel colours: the board's panel crushes the mid-tones, so the display pipeline lifts each channel (R 0.40, G 0.60, B 0.60, tuned on the Colour Card; `hardware.yaml`, `colour_correction`). The simulator shows exact colours, so it looks different from the glass: judge colour on the board. The correction applies to games too.
  - Diagnostics: the Games shelf lists only games. The Colour Card and any cart with `diagnostic = true` in its `cart` table (test and bench carts, never kids' games) are listed under Settings > Diagnostics (`OS_DIAG`, hidden from the pause menu's Settings) and return there when they exit.
  - Settings persist in NVS. Holding the right knob at power-on boots safe mode, which ignores saved settings.
  - Still open: Resume relaunches the last game from the start (real resume needs saves, Phase 7). Games read both knobs directly; the Hands setting reaches them as `sys.hands()` (and `ui.sides()`), which each game uses to swap its controls. Carts can't have a hero image yet.

**Phase 7 (storage): done (2026-11-14).** `console/partitions.csv` has nvs, otadata, factory (2 MB), ota_0/ota_1 (3 MB each), carts (LittleFS, 7.5 MB), saves (LittleFS, 256 KB) and coredump.
  - Uploaded carts are stored as `/carts/<id>.lua` and stay in the gallery after a reboot. Built-in carts stay compiled in.
  - Games get `save.set(key, value)`, `save.get(key, default)` and `save.clear()`. Values are numbers, strings and booleans, up to 4 KB per cart. Saves are written at most once a second and when the game stops, via a temp file and an atomic rename, with a CRC; a damaged save is reported and reset.
  - Torture test (`sim/torture.py`): 50 board resets with the save rewritten every frame left every save intact.
  - Still open: a real power-cut run (USB unplugged mid-save). The `save_torture` cart shows SAVE OK or SAVE DAMAGED on screen for that.

**Phase 8 (audio): not planned.** Audio is a hypothetical future feature, not something to build on the 1.47 or 1.69 boards in their current configuration. There is no `sfx`/`music` API and no Sound setting.

**Phase 9 (WebSerial uploader): done (2026-11-14).** `uploader/` is a static page for Chrome or Edge. It checks a cart for three seconds in the WASM engine with a live preview, shows errors with the code around the line, and copies a ready-made request for an AI helper. It then sends the cart over Web Serial with the console's own commands (`cart <bytes> <id>`, `play`, `rm`, `ls`). Connect, send and play were tested by hand on the board.
  - Serve the repository root over HTTP and open `/uploader/`. It loads `web-engine/tinygame.js`, which is committed; `./tg serve` serves it.

**Phase 10 (AI kit): done (2026-11-14).**
  - `docs/api-card.md` is the API card. `llms.txt` indexes it, and `.claude/skills/make-a-tinygame/SKILL.md` is the write, check, look and fix loop.
  - Golden carts: removed in the clean-slate reset (2026-11-15; see git history). Games now live in `games/`; the only built-in gallery entry is the Colour Card.
  - `sim/build/tinygame-sim --check` is the checker: a dialect lint, cart-table checks, and two 300-frame strict runs.
  - The cart API gained `tilt.x/y/shake/zero` and `gfx.circle`/`gfx.fill_circle`.
  - `ai/eval/run_eval.py` measures how many kid-style prompts (`ai/eval/prompts.txt`) pass on the first try. It needs the `anthropic` package (in `ai/eval/.venv`) and `ANTHROPIC_API_KEY`, which lives in `~/.zshrc`, so run it via `zsh -ic`. A run costs a few dollars.
  - First run, 2026-11-14 (`claude-opus-5-5`, effort medium): 15/20 under the original checker. Four of the five failures were the checker wrongly rejecting `if not x then` reads of unset globals, so the strict mode now rejects only foreign names (`btn`, `spr`, ...). A malformed icon is now a warning. Re-checked, the same 20 answers pass 20/20 (one with an icon warning).
  - Fresh run, 2026-11-14, same settings: **20/20 passed first try, 0 warnings** (about 74k output tokens, 34 s per answer). The first run cost $1.86. Spot-checked games, including a space shooter and Tetris with a ghost piece, are coherent and show their controls.

**Phase 11 (clean slate + Brick Breaker): done (2026-11-15).** All old games and failure tests were deleted (git history has them); the only built-in gallery entry is the Colour Card. `games/brickbreaker.lua` is the first new game and the reference for the look.
  - Engine additions made along the way, all documented in `docs/api-card.md`: `gfx.label`/`gfx.label_width` (the OS's smooth Space Grotesk text for carts), the `ui` kit in `prelude.lua` (`ui.card`, `ui.pill`, `ui.controls`, `ui.icon`, `ui.heart`, `ui.plate`, `ui.sides`, optional `ui.bar`), `sys.hands()` (the Hands setting; games swap knob roles with `ui.sides()`), and an optional `stats()` cart function that feeds the pause sidebar.
  - Pause screen (`os.c`, `draw_paused`): wears the paused game's cartridge and window colours (from its icon), sidebar on the left with icon, name and up to 4 stats, menu card on the right. Blended text edges are dimmed through `gfx_dim_slot`.
  - Control icon language: a ring with an arrowhead = TURN, an arrow pressing onto a surface = CLICK. The same 13x13 bitmaps live in `os.c` (`ICON_TURN`/`ICON_CLICK`, used by `footer`) and in `prelude.lua` (`ICONS`); change both together.
  - Learnings, so the next game goes faster:
    - Engine, prelude or `os.c` changes need `cmake --build sim/build` (the simulator embeds the prelude), `./tg web-build`, and `./tg flash` before the board sees them. `./tg send` alone only updates the cart.
    - The glass is small and the bezel is round: keep 16 px from the sides, avoid tiny text, favour few big flat shapes. Bright reds read orange and pale yellows read cream on the panel, so judge colour on the board.
    - Knob feel: don't scale a step by `knob.speed` directly (one step spikes it). Smooth your own velocity from `knob.pos` deltas, ramp gain gently, and glide the paddle toward a target. Half a turn per second should cross the screen.
    - Balls trapped in a perfectly vertical loop: keep a minimum sideways speed and jitter paddle bounces.
    - Autoplay without engine support: copy the cart, replace `knob.pos`/`knob.pressed` with a fake driven by game state, and run `tinygame-sim` for thousands of frames. Declare helpers after the locals they read, or they see nil globals.
    - Icons only accept `.` and `0-9a-f`; sprites may use `a-v`.
    - `--left-handed` on `tinygame-sim` tests mirrored controls.

**Phase 12 (battery + sleep): built (2026-11-16), not yet tried on the board.**
  - Battery: GPIO1 (ADC1 ch0) behind a x3 divider (`BOARD_BAT_ADC_GPIO`/`BOARD_BAT_DIV` in `board.h`; the pin and ratio come from a Waveshare note and are unverified, so check them with a meter). `plat_battery_mv()` takes the median of 9 calibrated reads. The shell reads it once a second, smooths it and shows 4 bars with 40 mV hysteresis (on at 3.55/3.68/3.80/3.95 V). No fuel gauge, so the bars are a rough guide. Below 3.55 V the icon is red. Under 2.5 V it counts as "no battery" and the icon is hidden. The icon is on Welcome, the Games shelf, Settings and the pause card; a yellow bolt beside it means USB power.
  - USB: `plat_usb_powered()` uses `usb_serial_jtag_is_connected()` unless `BOARD_USB_SENSE_GPIO` names a VBUS pin. A charger with no data host does not show up that way, so a console on a plain wall charger will still auto-sleep until a VBUS pin is set.
  - Sleep: deep sleep (`plat_deep_sleep` in `platform_esp.c`): backlight off and held, panel SLPIN, wake on either click (GPIO 6/7, ext1 any-low with RTC pull-ups). It waits for the clicks to be released before sleeping, since both are held for the gesture. A wake is a reboot: the wake click is not safe mode and is swallowed by `os_wake()`, which relaunches the game that was running and shows it paused (its save is flushed by `os_sleep_prepare`). Wake from Welcome or the shelf goes to Welcome.
  - Triggers: idle timeout (no knob turn, click or tilt for the Settings > Sleep after time; 3, 5 (default), 10 min or Off; never while USB powered), Sleep in the pause menu, and holding both knobs for 3 s outside a game (in a game that gesture still leaves the game). The shell sets `os_sleep_wanted()`; the platform acts on it.
  - Settings layout is now version 2: the old unused `sound` byte became `sleep`; version 1 data is migrated with the default.
  - Simulator: `--battery MV`, `--usb 0|1`, `--wake ID`; a sleep request prints `{"t":"sleep","frame":N,"resume":"id"}` and ends the run.
  - Not done: low-battery power-down near 3.3 V, putting the IMU to sleep, measuring sleep current, and wake-from-sleep on the real board.

### Screen lab notes (history)

Getting the display right was the main blocker in earlier experiments. The symptom: horizontal lines scrolling down showed "pixel waves".

**Original diagnosis (largely confirmed: `sw` mode cut the wave a lot; the rest came from panel init and refresh tuning).** The earlier setup used MADCTL 0x70 (MV set), so the controller did the landscape rotation. Writes then run across the panel's physical refresh direction. With no TE pin, the write sweep and the refresh sweep cross at a different point in every column, so a moving horizontal line turns into a drifting wave.

**Fix to test:**
1. Keep the panel in native portrait (MV off) and rotate in software while expanding to RGB565. Writes then follow the refresh, so the wave should become a single seam.
2. Push only changed areas.
3. Run SPI at 80 MHz if it's clean.
4. Tune FRCTRL2 (0xC6) against a locked 30 fps.

ML (MADCTL bit 0x10) may also change the panel's refresh direction, so test it in both modes.

## Layout

- `components/tthal/`: board drivers shared by both apps (`board.h`, `lcd`, `input`, `imu`). It is named `tthal` because ESP-IDF already has a component called `hal`.
- `components/font/`: the 5x7 bitmap font.
- `components/lua/`: Lua 5.4.9 from lua.org, vendored. The only change is a guard in `luaconf.h` so `LUA_32BITS=1` can come from the build. io/os/package/debug libraries and the standalone tools are left out of the build.
- `firmware/`: the screen lab (bench tool).
- `components/tinygame/`: the portable engine, plain C11 plus Lua with no ESP-IDF headers. It holds `vm.c` (the Lua host), `gfx.c`, `engine.c` and `prelude.lua`. Each target implements `include/platform.h`.
- `console/`: the Lua console (the product firmware). It is the board platform plus the cart runner.
- `sim/`: the simulator. It is the host platform plus a headless CLI.
- `web-engine/`: the engine compiled to WebAssembly, committed so the browser pages work without Emscripten. `./tg web-build` rebuilds it, and so does building `sim/build-web`.
- `games/`: user games. `_template.lua` is what `./tg new` copies.
- `tg`: the helper command (setup, new, check, play, send, serve, web-build, flash). See `README.md`.

## Console: `console/` (ESP-IDF)

Build and flash with `idf.py -C console -p /dev/cu.usbmodem* build flash`. The USB port name changes between replugs, so use the glob.

- `main/game.c`: the cart runner on core 1. It handles cart handoff, frame pacing, stats, error screens and the `hash` command. `main/platform_esp.c` implements `platform.h`.
- `main/display.c`: triple 8-bit buffers. The game draws into the back buffer; a flush task on core 0 diffs, expands, rotates and DMA-pushes. It uses the lab-winning panel settings.
- `components/tinygame/gfx.c`: clipped primitives and text. Colours 0-31 are the game palette and everything from 32 up is OS-only, so carts can't break error screens.
- `components/tinygame/vm.c`: the Lua host. It runs one cart at a time in a sandbox (base without load/dofile/loadfile/collectgarbage, plus string without dump, table, math and utf8). `pcall`/`xpcall` can't swallow the watchdog or out-of-memory errors. The API tables are read-only, and only text source is accepted, never bytecode.
  - Memory is capped at 1 MB per cart. Blocks of 256 bytes or less go to internal RAM up to 64 KB, the rest to PSRAM.
  - The watchdog fires after 250 ms in one callback.
  - The API so far is `cart`, `init`/`update`/`draw`, `gfx.clear/pixel/line/rect/fill_rect/text/text_width/sprite/map/W/H` (colours 0-31), `map.load`, `map_get/map_set/map_hit`, `knob.turn/pos/speed/pressed/held/released/long` (sides are `"left"`/`"right"`; `turn` is in notches, 20 per revolution) and `sys.time/frame/fps/log`.
- `components/tinygame/engine.c`: text-art sprites and tile maps (`docs/engine-v1.md`), decoded on first use or right after the cart file runs, kept outside the Lua cap with their own 512 KB cap.
- `components/tinygame/prelude.lua`: Lua helpers every cart gets: `hit`, `group`, `clamp`, `lerp`, `dist`, `rnd`, `rnd_int`, `choose`.
- `components/tinygame/os.c`: the OS shell around the vm. Carts declare `title`, `tagline` and a 16x16 text-art `icon` in their `cart` table; the shell reads them with `vm_scan` at boot.
- `main/main.c`: the serial dev console.
  - `cart <bytes> [id]` followed by the source stores it as `/carts/<id>.lua` (default id `uploaded`) and plays it. Ids are 1-31 characters of a-z, 0-9, `_` and `-`.
  - `play <id>` launches a cart, `rm <id>` deletes a stored one, and `ls` lists the gallery.
  - `saveflush <ms>` sets the save write interval (tests only).
  - `gamma <r> <g> <b>` retunes the panel colour correction live (exponents, 1.0 = none, lower = brighter). Not saved; the defaults are in `console/main/display.c` and `hardware.yaml` (`colour_correction`).
  - `hash <frames> [seed]` relaunches the last game deterministically and prints frame hashes.
  - `restart` relaunches the last game, and `reboot` restarts the board.
- `main/platform_esp.c` mounts `/carts` and `/saves` (LittleFS, from the `joltwallet/littlefs` managed component; `idf.py` fetches it on the first build).
- `components/tinygame/save.c`: the save store.
- `tools/send_cart.py PORT FILE.lua [--seconds N] [--timestamps]`: uploads a cart (as `uploaded`) and prints its JSON output. Run it with the ESP-IDF Python environment.
- `carts/system/colorcard.lua` is the only built-in gallery entry. The old failure tests (`carts/tests/*.lua`: stuck, bug, oom, sandbox, engine_selftest, determinism, save_torture, ...) were deleted in the clean-slate reset (2026-11-15) and are recoverable from git history; they covered: stuck, stuck inside pcall, bug, out of memory (also inside pcall), the sandbox probe, a cart without a `cart` table, bad sprite and map art, and `engine_selftest.lua` (22 checks). `brick_autoplay_patch.lua` is appended to `brick.lua` for unattended frame-time runs.

## Simulator: `sim/`

Build it with `cmake -S sim -B sim/build && cmake --build sim/build`.

- `sim/build/tinygame-sim CART.lua [--frames N] [--seed S] [--png FILE] [--scale K] [--press side@frame] [--turn side@frame:notches]` runs a cart headless.
  - It prints the same JSON lines as the board, plus frame hashes for frame 1, every 30th frame and the last frame.
  - It writes a PNG of the final frame, or of the error screen if the cart failed.
  - Exit status: 0 when the cart ran every frame, 1 after an error, 2 for usage problems.
- `sim/build/tinygame-sim --check CART.lua [--frames N] [--png FILE]` is the cart checker (`sim/lint.c` plus strict runs). The AI kit's loop is built on it.
- `sim/build/tinygame-sim --shell CART.lua... [--frames N] [--press ...] [--turn ...] [--chord pause|exit@frame] [--png FILE]` runs the OS shell with those carts in the gallery and prints a `mode` line at each screen change.
- `--storage DIR` keeps carts and saves in `DIR/carts` and `DIR/saves` (without it, saves last one run). `--runs R` plays a cart R times in a row to test saves across runs.
- `sim/torture.py PORT [--cycles N]` resets the board at random moments while `save_torture` rewrites its save, and checks that every save survives.
- Runs are deterministic: `math.random` is seeded, `sys.time()` follows the frame count, and input comes only from `--press` and `--turn`.
- Browser build: Emscripten lives in `~/emsdk` (`source ~/emsdk/emsdk_env.sh`). Build with `./tg web-build` (or `emcmake cmake -S sim -B sim/build-web && cmake --build sim/build-web`), which refreshes `web-engine/`. Then `./tg serve` and open `/sim/web/`. The page runs carts live with keyboard (A/D/S and J/L/K), mouse or touch knobs, and takes dropped `.lua` files. WASM can't load from `file://`.
- `node sim/web/hash_node.cjs CART.lua [frames] [seed]` prints the WASM engine's frame hashes for comparison with `tinygame-sim`.
- `sim/compare.py PORT CART.lua [--frames N] [--seed S]` runs a cart on the board (`hash` command) and in the simulator, then compares hashes line by line. Run it with the ESP-IDF Python environment.
- Both builds use `-ffp-contract=off` so float results match; the S3 FPU has a fused multiply-add.

## Firmware: `firmware/` (ESP-IDF, v5.5 line)

**Status:** builds and runs on the board (ESP-IDF v5.5 in `~/esp/esp-idf`; `source ~/esp/esp-idf/export.sh` first). Build with `idf.py -C firmware build flash monitor` (or `-p /dev/cu.usbmodem*`). Opening the serial port resets the board to its boot defaults, so send a whole config in one session. Boot defaults are the best settings found (see `lab_result` in `hardware.yaml`), and the two encoders tune FRCTRL2 and porch live with an on-screen readout.

**Files**
- `main/board.h`: profile 147b constants, mirroring `hardware.yaml`.
- `main/lcd.c`: raw ST7789 commands over esp_lcd SPI panel IO, with 3 pipelined DMA strip buffers (RAMWR, then RAMWRC 0x3C for continuation strips). Backlight PWM is 20 kHz so phone slow-motion video doesn't band (`hardware.yaml` says 1 kHz).
- `main/lab.c`: renders into an 8-bit indexed 320x172 framebuffer, then flushes in one of two modes:
  - `hw`: controller rotation, MADCTL 0x70, row offset 34. This is the old behaviour, kept as the A/B baseline.
  - `sw`: native portrait, MADCTL 0x00, column offset 34, rotation in software. Use `swflip fx fy` to fix the orientation until the geometry page reads correctly.
- `main/main.c`: console on USB-Serial-JTAG. The firmware deliberately uses this instead of TinyUSB CDC, so crashes don't kill the port and esptool auto-reset works.

**Pages:** `geometry` (edges, corner labels, UP arrow, labelled colour bars), `hline`, `vline`, `box`, `checker`, `stripes`. The BOOT button cycles pages.

**Serial commands (one per line):**
- `page <n|name|next>`
- `mode hw|sw`
- `madctl <hex>`
- `swflip <fx> <fy>`
- `mhz <n>`
- `fps <n>` (0 = unlimited)
- `diff 0|1`
- `strip <1-32>`
- `speed <px/frame>`
- `hud 0|1`
- `inv 0|1`
- `bl <0-100>`
- `frctrl <hex>`
- `raw <cmd> [bytes]`
- `cfg`
- `reboot`

**Output:** JSON lines. `boot` is printed once, `cfg` after every command, and `stats` once per second (fps, late frames, render time, flush min/avg/max in µs, KB per frame).

## Test sequence

1. Build and flash. The board may currently run TinyUSB CDC firmware; if esptool can't connect, hold BOOT, tap RESET, then retry.
2. Geometry, in `sw` mode at 40 MHz. Find the `swflip` values where TL/TR/BL/BR, UP and the R/G/B labels are all correct and the outer white ring is complete on all four edges. Then do the same check in `hw` mode.
3. `hline` at speed 2, 4 and 8. Compare `mode hw` against `mode sw`, both with `diff 0` and `diff 1`. Morten films each in phone slow-motion and reports what he sees. The agent can't see the glass.
4. Repeat at `mhz 80` and check the stats for errors or glitches. Then try `frctrl` values with `fps 30` and `fps 60`.
5. Write the winning settings back into `hardware.yaml`, including `physical_scan_direction`, and record the measured flush times.

## Decisions so far

- Locked 30 fps is the promise; 60 fps is opt-in only if the bench numbers support it.
- 8-bit indexed framebuffer in internal RAM. No LVGL.
- Use USB-Serial-JTAG during development; TinyUSB only if a USB-drive mode is wanted later.
- The API exposes screen width and height from day one, because the 1.69 board is 280x240.
- Keep a HAL boundary so a desktop or WASM simulator is possible later. There is no CI rig or "software factory"; it's one board on the desk.
