# Wiring

TinyGame is controlled by two rotary encoders, each with a click switch. The
screen, motion sensor, and RGB LED are onboard; their pins are in
`src/hal/board_config.h`.

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

## Deep sleep and wake

System > Sleep or the configured auto-off timeout without encoder activity
requests deep sleep. AUTO-OFF defaults to ON; OFF AFTER offers 1, 5 (default),
10, 15 or 30 minutes. An active computer USB CDC serial connection suspends
automatic sleep, but USB power without an open serial connection does not.
Closing the serial connection starts a fresh idle countdown. Manual Sleep
remains available while connected.
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

GPIO2, GPIO3, and GPIO10 are unused.
