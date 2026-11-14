# TinyTwist / TinyGame — working notes for Claude Code

TinyGame is a pocket game console for kids and parents to build, play and share games with AI help.
Tagline: "Tiny screen. Twisty knobs. Real games, built by you."

- Board: Waveshare ESP32-S3-LCD-1.47B (default, profile `147b` in `hardware.yaml`). The 1.69 board comes in only once the platform is stable.
- The board is mounted turned 90 degrees clockwise, so the logical screen is 320x172 landscape.
- Input: two undetented EC11 encoders with clicks (80 transitions per turn) plus the onboard QMI8658 IMU.
- Games will be written in Lua 5.4. The full plan is in `docs/scope-v0.1.md`, which is a reference, not doctrine.
- `hardware.yaml` holds the board facts from earlier experiments. It is the source of truth for pins, offsets and MADCTL.

## Current phase: screen lab (nothing else until the display is right)

Getting the display right was the main blocker in earlier experiments. The symptom: horizontal lines scrolling down showed "pixel waves".

**Working diagnosis (not yet confirmed).** The earlier setup used MADCTL 0x70 (MV set), so the controller did the landscape rotation. Writes then run across the panel's physical refresh direction. With no TE pin, the write sweep and the refresh sweep cross at a different point in every column, so a moving horizontal line turns into a drifting wave.

**Fix to test:**
1. Keep the panel in native portrait (MV off) and rotate in software while expanding to RGB565. Writes then follow the refresh, so the wave should become a single seam.
2. Push only changed areas.
3. Run SPI at 80 MHz if it's clean.
4. Tune FRCTRL2 (0xC6) against a locked 30 fps.

ML (MADCTL bit 0x10) may also change the panel's refresh direction, so test it in both modes.

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
