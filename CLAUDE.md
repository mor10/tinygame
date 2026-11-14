# TinyTwist / TinyGame — working notes for Claude Code

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

**Phase 3 (Lua core): in progress in `console/`.** See the Console section below. Roadmap: `docs/scope-v0.1.md` section 14.

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
- `console/`: the Lua console (the product firmware).

## Console: `console/` (ESP-IDF)

Build and flash with `idf.py -C console -p /dev/cu.usbmodem* build flash`. The USB port name changes between replugs, so use the glob.

- `main/display.c`: triple 8-bit buffers. The game draws into the back buffer; a flush task on core 0 diffs, expands, rotates and DMA-pushes. It uses the lab-winning panel settings.
- `main/gfx.c`: clipped primitives and text. Colours 0-15 are the game palette and 16-22 are OS-only, so carts can't break error screens.
- `main/game.c`: the Lua host. It runs one cart at a time in a sandbox (base without load/dofile/loadfile/collectgarbage, plus string without dump, table, math and utf8). `pcall`/`xpcall` can't swallow the watchdog or out-of-memory errors. The API tables are read-only, and only text source is accepted, never bytecode.
  - Memory is capped at 1 MB per cart. Blocks of 256 bytes or less go to internal RAM up to 64 KB, the rest to PSRAM.
  - The watchdog fires after 250 ms in one callback.
  - The API so far is `cart`, `init`/`update`/`draw`, `gfx.clear/pixel/line/rect/fill_rect/text/text_width/W/H`, `knob.turn/pos/speed/pressed/held/released/long` (sides are `"left"`/`"right"`; `turn` is in notches, 20 per revolution) and `sys.time/frame/fps/log`.
- `main/main.c`: the serial dev console. `cart <bytes>` followed by the source uploads and runs a cart; `builtin`, `restart` and `reboot` also work.
- `tools/send_cart.py PORT FILE.lua [--seconds N] [--timestamps]`: uploads a cart and prints its JSON output. Run it with the ESP-IDF Python environment.
- `carts/hello.lua`: the built-in cart. `carts/tests/*.lua` are failure tests: stuck, stuck inside pcall, bug, out of memory (also inside pcall), the sandbox probe, and a cart without a `cart` table.

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
