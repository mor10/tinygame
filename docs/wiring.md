# Wiring

TinyGame is controlled by two rotary encoders, each with a click switch. There
is no third button. The screen, motion sensor and RGB LED are onboard; their
pins are in `src/hal/board_config.h`.

Left and right refer to physical positions when holding the assembled console
normally. The final assembly swaps the original encoder positions without
changing their wires. The mapping lives in `src/hal/board_config.h`.
HANDEDNESS = RIGHT (the default) selects the physical right encoder; LEFT selects
the physical left. Either click switch remains an action button.

## EC11 5-pin rotary encoders with normally open click

The encoders are a nominal 20-pulse incremental device. Their detents have been
removed, so firmware counts continuous electrical transitions (nominally 80 per
revolution), not mechanical detents. Verify pulse count and clockwise polarity
on the physical assembly.

| Pin | Left encoder | Right encoder |
|---|---|---|
| A | GPIO9 | GPIO4 |
| B | GPIO8 | GPIO5 |
| Click switch S1 | GPIO7 | GPIO6 |
| Common C and switch S2 | GND | GND |

A, B and the click switch use internal pull-ups. No 5 V supply is connected to
these passive contacts.

## Deep sleep and wake

System > Sleep or 3 minutes without encoder activity requests deep sleep.
Either normally open encoder switch (GPIO6 or GPIO7 to GND) wakes to normal
startup. No additional wiring is needed; GPIO10 is unused.

`src/os/power_state.h` waits for both switches to release and for panel settling
without blocking the loop. `src/hal/power.cpp` keeps RTC pull-ups powered and
enables ext1 `ESP_EXT1_WAKEUP_ANY_LOW` on GPIO6 and GPIO7. The OS disables the IMU
sensors, RGB LED and LCD/backlight before sleep; after wake it restores both
switch GPIOs from RTC mode and suppresses input until both release stably.
Game state is not restored. USB may disconnect during sleep. Do not replace
this with blocking `while (pressed) delay(...)` loops.

## Retired parts

The old potentiometer on GPIO2 and action button on GPIO3 are no longer used.