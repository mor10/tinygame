# Wiring

TinyGame is controlled by two rotary encoders, each with a click switch. The
screen and motion sensor are onboard; their pins are in
`src/hal/board_config.h`. Board A has an RGB LED; Board B does not.

Left and right refer to physical positions when holding the assembled console
normally. The pin assignments below match those positions, and
`src/hal/board_config.h` is the source of truth. HANDEDNESS = RIGHT (the
default) selects the physical right encoder; LEFT selects the physical left.
Either click switch is an action button.

## EC11 5-pin rotary encoders with normally open click

The encoders are nominal 20-pulse incremental devices with continuous,
detentless rotation. Firmware counts electrical transitions (nominally 80 per
revolution). Verify pulse count and clockwise polarity on the device.

| Pin | Left encoder | Right encoder |
|---|---|---|
| A | GPIO9 | GPIO4 |
| B | GPIO8 | GPIO5 |
| Click switch S1 | GPIO7 | GPIO6 |
| Common C and switch S2 | GND | GND |

A, B and the click switch use internal pull-ups. No 5 V supply is connected to
these passive contacts.

## Board profiles and diagnostic test card

The explicit firmware profiles are:

| Profile | Board | Landscape display | Left A/B/button | Right A/B/button | IMU SDA/SCL | RGB LED |
|---|---|---:|---|---|---|---|
| `147b` | ESP32-S3-LCD-1.47B | 320x172 | 9/8/7 | 4/5/6 | 48/47 | GPIO38 |
| `169` | ESP32-S3-LCD-1.69, non-Touch | 280x240 | 43/2/3 | 16/17/18 | 11/10 | None |

Build the isolated display/input/IMU test card with an explicit target:

```bash
python3 tools/tg.py test-card-build --board 147b
python3 tools/tg.py test-card-build --board 169
```

`test-card-flash` also requires `--board`; provide `--port` when multiple USB
devices are connected. Flash only with explicit maintainer permission and an
identified physical board. The on-screen profile is the compiled software
configuration, **not** physical board identification. The test card displays
color swatches, text sizes, encoder totals/deltas/clicks/errors, and live tilt
and gyro readings. The revised test-card source also prints a `[DIAG]` line
every 100 ms while an active USB CDC host is open: input status, each knob's
total/delta/button/click/hold/error fields, IMU status, tilt, and gyro values.
`python3 tools/tg.py send info` reports the compiled profile and display size,
plus input and IMU health; this is not physical board identification. The test
card accepts only `info` and `help` serial commands. Capture its responses and
telemetry only after the revised image is explicitly flashed. A build or
simulator run does not pass a physical gate.

Board B's encoder direction and downhill-tilt mapping were confirmed on the
assembled board in prior work. Its exact counts/click feel, gyro signs,
GPIO3 strap/wake behavior, sleep/wake, and USB-host-versus-power-only auto-off
remain unverified. Inspect the hardware revision before any revision-dependent
power or buzzer connection. Do not drive GPIO38 as an LED on Board B.

## Board A deep sleep and wake

System > Sleep or the configured auto-off timeout without encoder activity
requests deep sleep. AUTO-OFF defaults to ON; OFF AFTER offers 1, 5 (default),
10, 15 or 30 minutes. An active computer USB CDC serial connection suspends
automatic sleep, but USB power without an open serial connection does not.
Closing the serial connection starts a fresh idle countdown. Manual Sleep
remains available while connected.
Individual encoder holds are delivered to games and do not pause or navigate.
When both switches are down, the OS captures both inputs and freezes the game.
Releasing both before three continuous seconds opens Pause or goes back in menus;
holding both continuously for three seconds opens sleep confirmation, defaulting
to Yes. The chord and its releases are swallowed. Both switches must release
before a fresh click can confirm; No restores the previous game or menu,
including any settings edit.
Either normally open encoder switch (GPIO6 or GPIO7 to GND) wakes to normal
startup. No additional wiring is needed; GPIO10 is unused.

`src/os/power_state.h` waits for both switches to release and for panel settling
without blocking the loop. `src/hal/power.cpp` keeps RTC pull-ups powered and
enables ext1 `ESP_EXT1_WAKEUP_ANY_LOW` on GPIO6 and GPIO7. The OS disables the IMU
sensors, RGB LED and LCD/backlight before sleep; after wake it restores both
switch GPIOs from RTC mode and suppresses input until both release stably.
Game state is not restored. USB may disconnect during sleep. Do not replace
this with blocking `while (pressed) delay(...)` loops.

Firmware uses TinyUSB CDC (`ARDUINO_USB_MODE=0`) on the existing USB connector.
Auto-off checks TinyUSB's mounted state and CDC DTR control line (asserted
on serial open, cleared on close), independently of RTS flow control.
Hardware CDC mode 1 only latches
traffic until unplug/reset, so it cannot detect a closed serial monitor reliably.
No VBUS sensing or wiring changes are needed.

## USB power and lights

The [official board schematic](https://files.waveshare.com/wiki/ESP32-S3-LCD-1.47B/ESP32-S3-LCD-1.47B_schematic_diagram.pdf)
routes USB VBUS to the supply, ETA6098 charger and header, not to an ESP32 input.
GPIO1's BAT_ADC divider measures the battery, not VBUS; battery voltage cannot
reliably identify a plugged-in charger. TinyUSB's mounted state identifies an
enumerated USB host, not a power-only cable or wall charger. CDC DTR only identifies
an open serial app. Neither is an all-USB-power detector.

LED1 is wired directly to the ETA6098 STAT output and is not software-controlled.
LED2 is the addressable RGB on GPIO38, owned by the OS/game status-light policy
and brightness setting. Its current behavior is unchanged: a reliable red
indicator for every plugged-in power source is unsupported on the unmodified
board. Do not invent a sensing pin or connect 5 V VBUS directly to a GPIO.

On Board A, GPIO2, GPIO3, and GPIO10 are unused. Board B uses those pins as
encoder/I2C signals; GPIO3 strap/wake behavior remains an outstanding physical
safety check.
