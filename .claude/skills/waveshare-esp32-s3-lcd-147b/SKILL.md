---
name: waveshare-esp32-s3-lcd-147b
description: Hardware facts and debugging for the TinyGame board (Waveshare ESP32-S3-LCD-1.47B). Use for board bring-up, the ST7789 display driver, pins, EC11 encoders, buttons, deep sleep, QMI8658 IMU, RGB LED, backlight, PSRAM, PlatformIO config, flashing/serial problems, or anything in src/hal/. Not needed for writing games.
---

# Waveshare ESP32-S3-LCD-1.47B

The sources of truth are `src/hal/board_config.h` (pins) and `src/hal/display_config.h` (panel). Never invent pins.

## Board

- ESP32-S3R8: dual-core 240 MHz, 16 MB flash, 8 MB OPI PSRAM
- 1.47" ST7789 IPS panel, 172×320, used in landscape at 320×172. No touch.

## Pins

Onboard pins are verified. External controls follow `docs/wiring.md`; verify their encoder polarity and pulse count on the physical assembly.

Left/right mean physical positions when holding the assembled console normally.
The final assembly swaps the original encoder positions with wiring unchanged;
the side mapping belongs in `board_config.h`, not in handedness or game logic.

| Function | GPIO | Notes |
|---|---|---|
| LCD MOSI / SCLK / CS / DC / RST | 45 / 40 / 42 / 41 / 39 | SPI mode 0, 80 MHz |
| LCD backlight | 46 | LEDC PWM, 1 kHz, 10-bit, duty = percent × 10 |
| RGB LED (WS2812) | 38 | `neopixelWrite(r, g, b)`; the core handles GRB order |
| IMU QMI8658 SDA / SCL | 48 / 47 | I2C 400 kHz, address 0x6B, WHO_AM_I (0x00) = 0x05 |
| Left EC11 A / B / click | 9 / 8 / 7 | Common C and other switch contact to GND; internal pull-ups |
| Right EC11 A / B / click | 4 / 5 / 6 | Common C and other switch contact to GND; internal pull-ups |
| SD card (unused) | CMD 15, SCK 14, D0-D3 16/18/17/21 | |

Never put more than 3.3 V on any GPIO or ADC pin.

GPIO2/GPIO3 are no longer pot/action inputs; GPIO10 is unused. EC11 detents have been removed, so decoding must not assume a mechanical resting position. Nominal 20-pulse encoders give 80 electrical transitions per revolution. ISR capture uses both A/B edges with atomic snapshots; never apply the 25 ms switch debounce to encoder A/B.

The main encoder (right by default; left when Settings > HANDEDNESS = LEFT) navigates menus (four transitions/step, clamped). A click on either encoder selects, and a 650 ms hold on either pauses/goes back. Both clicks remain on Diagnostics; either hold exits. Games get `tg::Input::main/left/right` transition deltas plus merged button state, without long-press or sleep events. There is no pot.

## Deep sleep

System > Sleep or 3 minutes without encoder activity requests deep sleep. Either encoder click (GPIO6/7) wakes to startup via ext1 `ESP_EXT1_WAKEUP_ANY_LOW`. RTC pull-ups remain powered; restore both switch GPIOs with `rtc_gpio_deinit()` before normal input setup. Wait nonblockingly for both switches to release before shutdown and ignore all input until both release stably on boot, so the wake press cannot select anything. GPIO10 and the third button are removed.

The OS validates wake setup before shutdown, disables IMU sensors, turns off RGB/backlight, holds the backlight low, sends panel sleep commands, and waits 120 ms nonblockingly before deep sleep. Do not copy a `while (pressed) delay(...)` release loop. Turns, clicks, switch events and holds on either encoder reset idle time, including injected events; IMU motion and serial monitoring do not. Confirmed settings/scores persist, but RAM/game state and encoder counters reset. USB CDC can disconnect; serial and encoder rotation cannot wake the board, only a physical switch press. Measure whole-board current if power consumption matters.

## Display facts (verified; don't change without a reason)

- Waveshare demo init sequence, with COLMOD `0x3A`=`0x05` (RGB565) and inversion on (`0x21`).
- Landscape uses MADCTL `0x36`=`0x70` (MV=1). Because MV swaps axes in the controller, CASET takes logical x and RASET takes logical y. The panel's **34 px offset goes on the row axis**. The demo's own landscape path swaps twice and is wrong.
- LVGL partial render buffer of 16 lines; flush is a blocking SPI write.

## Software stack

- `platform = espressif32 @ 6.12.0` ships the **Arduino core 2.x** API: `ledcSetup`/`ledcAttachPin`, not `ledcAttach`.
- LVGL 9.6.0, configured in `include/lv_conf.h`. It uses the CLIB malloc, so large buffers go to PSRAM.
- USB CDC on boot (`ARDUINO_USB_MODE=1`). The port is `/dev/cu.usbmodem*` (VID 0x303A); `tools/tg.py` finds it.

## Commands

```bash
python3 tools/tg.py build | flash | test
python3 tools/tg.py monitor -s 10      # boot log; non-interactive
python3 tools/tg.py send info          # heap, PSRAM, installed games
python3 tools/tg.py shot               # screenshot -> shot.png
```

PlatformIO directly: `python3 -m platformio run [-t upload]`. `pio` is not on PATH. With several boards connected, `tg.py` stops and asks for `--port <device>` (accepted by `flash`, `monitor`, `send` and `shot`).

## Expected boot log

The first boot also logs `nvs_open failed: NOT_FOUND` from Preferences. That's expected while nothing is saved yet.

```
[BOOT] TinyGame starting / [BOOT] PSRAM: 8388608 bytes / [IMU] QMI8658 ready
[LCD] Waveshare ST7789 initialized / [BOOT] game <id>... / [BOOT] ready, N games
```

A game that fails registration (invalid id or name, duplicate id, missing factory, or the 32-game cap) is skipped and logged as `[BOOT] rejected game <id>: <reason>`.

## Debugging, one layer at a time

1. **No serial output:** unplug and replug. If upload fails, hold BOOT, tap RESET, then flash.
2. **Blank, shifted or mirrored screen:** check `display.cpp`, the offsets and MADCTL first. Don't touch the UI code.
3. **Wrong colours:** check RGB565 byte order and inversion against a known swatch (System > Style).
4. **Encoder or switch:** System > Diagnostics shows physical L/R totals, A/B state, direction, held/click/last-event state, invalid transitions (`ERR`), overflows (`O`), sleep state, and tilt. Bottom `E L/R` rows include injected input. Use `turn left 4`, `turn right -4`, `click right`, and `log on`; verify real contacts physically, not just with simulation.
5. **IMU missing:** System > Diagnostics labels the tilt box `NO IMU` (never started) or `IMU ERROR` (reads failing); the log has matching `[IMU]` lines. Run an I2C scan on 48/47. The floating bus gives false ACKs, so trust only WHO_AM_I = 0x05.
6. **Out of memory:** use `send info`. Keep game objects under about 300 and allocate big buffers once.
7. **Wrong chip on upload:** stop if esptool identifies a non-S3 chip (such as ESP32-C6). Do not change this project's target or force a write to a different board.
