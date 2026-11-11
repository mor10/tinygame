# Board B: questions to answer before any driver is written

Board B is a Waveshare ESP32-S3-Touch-LCD-1.69: a 240x280 panel, used rotated
as 280x240. Nothing about its wiring is known to this repository. No pin
number, part name or register value below is a guess; every item is a question
the schematic (or Waveshare's demo code) must answer first. Record the answers
in this file, with the page or file they came from, and only then add
`hal/boards/board_b/` (see the comment in `hal/board.h`).

## Knobs (two EC11 encoders with click: six lines)
1. Which GPIOs are free (not used by the panel, touch, motion sensor, storage,
   battery or USB) and broken out to a header or pad? Six are needed:
   left A, left B, left click, right A, right B, right click.
2. Which of those are RTC-capable (GPIO0 to GPIO21 on the ESP32-S3)? The two
   click lines must be, because deep-sleep wake (ext1) is on either click.
3. Do any of the free GPIOs have a pull-up, pull-down or other part on the
   board (strapping pins, boot button, I2C pull-ups)? Which are strapping pins?

## Panel
4. Which controller is the panel (the part number), and which interface
   (SPI, how many data lines; or something else)?
5. Pins: chip select, data/command, reset, clock, data, backlight. Is the
   backlight a plain GPIO that PWM can drive, or behind another chip?
6. What is the init command sequence Waveshare's demo uses? (Copy it exactly;
   do not adapt v1's.)
7. What are the row and column offsets of the 240x280 glass inside the
   controller's memory, and do they change when rotated to 280x240?
8. Which memory-access (rotation) value gives 280x240 landscape the right way
   up, with the knobs placed as on Board A? Is the byte order of pixel data
   configurable (as 0xB0 is on Board A's controller), or must it be swapped?
9. Highest reliable SPI clock for this panel.

## Motion sensor
10. Which part is it (QMI8658 as on Board A, or another)? Its I2C address,
    and the SDA and SCL pins?
11. Is an interrupt line wired, and where?
12. How are its axes oriented relative to the landscape screen?

## Sound, light, power
13. Is there a buzzer? On which pin, and is it passive (needs a tone) or active?
14. Is there an RGB LED or any status LED? Which pin and which type?
15. Is battery voltage sensed? Which pin, and what divider ratio? Is there a
    charger status line?
16. Is there a power-hold or power-latch pin that firmware must set? What
    does deep sleep do to it?

## Parts to leave alone
17. Which pins belong to the touch controller (I2C lines, interrupt, reset),
    and which part is it? They must not be reused for knobs. Are there
    others (RTC chip, SD card, IO expander) sharing the I2C bus or pins?
18. Does the board have an IO expander that owns any of the panel or touch
    reset lines?

## Build side
19. Which PlatformIO board definition matches this board's flash size and
    PSRAM mode (flash mode, size, PSRAM type)?
