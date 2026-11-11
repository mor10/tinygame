# Board A: after flashing the first firmware cut

Nothing in this firmware has run on hardware yet. This list is how the
maintainer finds out. Report what you see, including anything that fails.

## Get the firmware onto the board

Option 1, from the CI build (no tools needed except esptool):

1. Open the latest "Firmware" workflow run for the branch on GitHub and
   download the artifact `firmware-board_a`. Unzip it.
2. Put the board in the bootloader (hold BOOT, tap RESET, release BOOT) if it
   does not appear on its own, then:

       python3 -m pip install esptool
       python3 -m esptool --chip esp32s3 --port /dev/ttyACM0 --baud 921600 \
           write_flash 0x10000 firmware.bin

   This writes the program only. It assumes the board was flashed with v1
   before (same partition table). On a blank board also write
   `0x0 bootloader.bin` and `0x8000 partitions.bin` from the same download,
   but `0xe000 boot_app0.bin` is then also needed; build locally instead.

Option 2, build and flash locally (needs PlatformIO and internet):

       pip install platformio
       pio run -e board_a -t upload

Open the serial monitor (115200 baud):

       pio device monitor -b 115200

## What should appear on screen

- The backlight comes on and the screen goes black, then a side-scrolling
  level: sky, ground blocks, parallax layers, and a character walking back
  and forth while the camera follows. Colours should look like the pictures
  from `python3 tools/tg.py demo`. Note: wrong colours (red and blue swapped,
  or garbled) point at the pixel byte order; a picture shifted or with a
  stripe of noise at one edge points at the 34-pixel offset.
- A dark strip along the bottom edge with white text such as
  `L+12 R-4 M-4 C3 D0 T+10 -25`.
- The RGB LED shows a dim green.

## What the serial port should show

At boot:

       [BOOT] TinyGame v2 first cut (engine scrolling demo)
       [BOOT] board=board_a panel=320x172
       [POWER] wake=POWER/RESET reset=...
       [LCD] Waveshare ST7789 initialized
       [BOOT] ready, free heap ...

(An `[IMU]` line may appear; if the sensor does not answer, tilt stays 0.)
Then, every 5 seconds, one line:

       [TIME] ticks/s=60.0 frames/s=30.0 draw_us avg=... max=... present_us avg=... max=... heap=...

Good looks like ticks/s close to 60.0 and frames/s close to 30.0. If
frames/s is lower, the present time (a blocking SPI write of 110 kB) is the
first suspect.

## Knobs and tilt

The text strip shows: `L` left knob total, `R` right knob total, `M` the main
knob total (the right one), `C` click count, `D` 1 while a knob is held, `T`
tilt x and y in hundredths.
- Turn the right knob clockwise: R and M go up. Counter-clockwise: down.
- Turn the left knob clockwise: L goes up; M does not move.
- Click each knob: C goes up by one per click, D shows 1 while held.
- Tilt the board: T changes sign with the direction, about 100 at a firm tilt.
  Lay it flat: both values near 0.

## Sleep and wake

1. Hold both knob clicks for 3 seconds, then release both. The screen turns
   off, the LED goes off, and the serial port prints
   `[POWER] sleeping; click either knob to wake`. (In this cut there is no
   confirmation screen; sleep follows the release directly.)
2. Click the left knob: the board restarts into the demo (`wake=LEFT`).
   Sleep again and click the right knob (`wake=RIGHT`).
3. The click that wakes the board must not count: C starts at 0 after a wake.

## What to send back

- Whether each check above passed, and for any failure what you saw.
- Three or four `[TIME]` lines copied from the serial monitor.
- A photo of the screen, if colours or position look wrong.
- The `[BOOT]` lines from power-on.
