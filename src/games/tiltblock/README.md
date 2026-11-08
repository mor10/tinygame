# TILTBLOCK

**Made by:** mor10

Roll a ball over a full 8x8x8 block of cubes. Touch every cube on the current
layer to drop to the next one; the block gets shorter as you clear layers.
The layers under the ball fade into the dark, one step per layer.
Clear all eight to start a harder level.

## How to play

- At the start of every run, turn the main knob to choose **TILT** or **TWIST**,
  then click either knob.
- In **TILT**, move the dot into the center of the square and click either
  knob to level the play surface. Tilt the console to roll the ball.
- In **TWIST**, the left knob tips the board along its front-left diagonal:
  clockwise rolls the ball toward the center/right, and counterclockwise
  rolls it the other way. The right knob controls the opposite diagonal:
  clockwise rolls toward the center/right, and counterclockwise rolls toward
  the center/left. A quarter turn of either knob reaches full tilt in either
  direction. Each angle stays where you leave it; the board tips visibly as
  you turn.
- The ball speeds up quickly and rolls up to about three cubes a second, so
  tip the other way to brake. It keeps its momentum. Roll into a wall to bounce off it and touch its
  side; a wall cube must be touched just like a floor cube.
- Avoid holes. A hole in the middle traps the ball; a hole on the edge sends it
  off the block. Either costs a life. You start with five lives. The ball
  drops down the outside of the block or sinks into the hole, fading away
  over 0.8 seconds before a life is lost and it returns. A tractor still
  saves an outer-edge fall, including an edge hole, before a life is lost.
- Touch every safe cube on a layer. Touched cubes stay touched after a fall.
  The ball comes back near the middle with a short safety blink.
  Clearing a layer triggers a one-second cyan beam-up: the cubes pulse, a
  bright wave sweeps across them, and they shrink into rising beams of light
  before the block drops to the next layer. The last layer beams away too.
- Click either knob to fire an armed laser. Hold either knob for the console's
  pause menu. Each shot lasts 0.22 seconds, follows the ball's current rolling
  direction as you steer, and reaches the screen edge. Brake to a stop during
  a shot and it keeps its last direction. Shots that miss need three seconds
  to recharge; click while stopped and "ROLL TO AIM" reminds you to move.
- After game over, click either knob to choose a mode and start again.

## Special cubes

| Colour | Cube | Effect |
|---|---|---|
| Green | Life | Adds a life, up to nine |
| Cyan | Zone | Touches the eight surrounding cubes |
| Yellow | Boing | Hop for about one second; pass over holes and walls |
| Red | Quake | Shakes the block and shoves the ball for three seconds |
| Purple | Tractor | Saves one fall from an outer edge |
| Orange | Laser | Arms the ball while an alien is on the layer |

Brief text floats above pickups: **+1 LIFE** (or **LIFE FULL** at nine),
**ZONE**, **BOING**, **QUAKE**, **TRACTOR READY** and **LASER READY**.
A Zone still touches surrounding cubes; it does not teleport the ball.
**SAVED** marks a Tractor rescue, **-1 LIFE** marks a lost life, and
**ALIEN +500** accompanies a hit. Feedback rises for 0.9 seconds; a killed
alien bursts into white and cyan rays for 0.45 seconds, then vanishes.

## Level 1: eight lessons

The first seven layers introduce features; the eighth combines them:

| Layer | Lesson |
|---|---|
| 1 | Plain cubes, just like before |
| 2 | Five random holes to avoid |
| 3 | Five random walls to bounce against and touch |
| 4 | Three random bonus cubes: Life, Zone and Tractor |
| 5 | One Boing cube for hopping |
| 6 | One Quake cube to experience the shaking trap |
| 7 | A live alien and an orange Laser cube, ready from the start |
| 8 | Five holes, five walls, Life/Zone/Tractor, Boing, Quake, a live alien and Laser |

Features appear on their introduction layer and again in the finale.
A collected Tractor still saves one outer-edge fall as usual; if you already
have one, the finale does not add another Tractor cube.
Seeded layouts keep a safe middle spawn, clear corners for the alien and
Laser, connected safe floor cubes, and a reachable side of every wall.
The Level 1 HUD counts lessons from 1 to 8 while the block gets shorter.

From level 2 onward, the original difficulty progression is unchanged:
walls begin on level 2, holes on level 3, Life/Zone/Tractor on level 4,
Boing/Quake on level 5, and random alien encounters from level 6.
The live alien drifts toward the ball without following the
block's tilt. Find the orange laser cube, then click while rolling toward the
alien. A hit is worth 500 points.

Score 10 points for each touched cube, 100 times the level for each cleared
layer, 1,000 times the level for a cleared level, and 500 for an alien. Your
best score is saved; each new run starts at level 1 with five lives.

## Development

The board generator, fixed-step rolling physics and game rules are host-tested
in `test/`, and so is `framebuffer.cpp`. Run `python3 tools/tg.py test`, `python3 tools/tg.py build`, and
`python3 tools/tg.py sim --build-only` from the repository root.

For simulator playtests only, send `turn right 1233` on the mode screen to
start with four layers remaining (Level 1 lesson 5), or `turn right 1234` to
start at level 6 with an alien on the first layer. Use `turn right 1235` for
the live alien/Laser lesson or `turn right 1236` for the mixed-feature finale.
A physical knob cannot produce that many transitions in one
update.

### Drawing

The camera fills more of the screen with the same 8x8 board: the surface stays
225 pixels wide instead of the original 180 (25% wider). One fixed magnification
uniformly scales cubes, holes, walls, actors, Laser and effects. The play pivot
stays below the HUD; turning in TWIST changes the surface slope, never the zoom.
Visual tilt uses a coefficient of 0.0388 (up from 0.015), just below the measured
0.038824 limit for legal walls, edge shadows, 3px shake and 1.25px raster margins.
Opposing full TWIST positions change the surface height from about 97 to 128
pixels, while width stays 225 pixels. The pivot stays fixed at screen y=93.985.
A hop near the HUD has its drawn lift capped to keep the ball visible;
normal hops retain their full lift. Hop physics, duration, knob angles and
rolling physics are unchanged. Walls, hops, layer count and
effects cannot resize or recenter the scene. The layer-drop transition retains
its existing surface framing. The lower
stack still fades off-screen and departing beams still reach the screen edge.
HUD and overlay fonts keep their original sizes.

The scene is drawn by `framebuffer.cpp` straight into the RGB565 memory of one
LVGL canvas, using clipped spans instead of one LVGL call per pixel. LVGL still
puts it on the screen. Each frame erases only last frame's pixels and
invalidates only what changed. The HUD and messages are separate LVGL objects
using the shared fonts: small HUD text, medium clear messages and score, and
a large game-over title. Label text is updated only when its values change.
Floating feedback uses a fixed pool of twelve small shared-font labels,
including simultaneous pickups triggered by a Zone. No objects are created
per frame.
The display flush, not the drawing, is the expensive part. Lower-layer fades,
ball fades and cyan glow ramps are precomputed colours in `fade.h`, not
per-pixel alpha blending. Floors draw first, then the ball and raised walls
in depth order, so a wall in front can hide part of the ball. A Boing lifts
the ball above its shadow without changing that ordering.

The fixed-step model freezes rolling and monsters during fall and beam-up
stages. A fall resolves exactly once after its animation, then either plays
the tractor save, respawns with the existing safety blink, or ends the game.
Beam-up keeps the cleared board visible until it finishes, then starts the
existing layer-drop or level-clear transition.

To measure on a board, build with the opt-in profiler. It prints two `[TB]`
lines every five seconds: frame rate, model and raster time, LVGL plus flush
time, and any time lost to the OS 100 ms frame cap.

```bash
PLATFORMIO_BUILD_FLAGS=-DTILTBLOCK_PROFILE python3 tools/tg.py flash
python3 tools/tg.py monitor -s 12
```

Flash a normal build afterwards. Before the visual polish, the full cube
measured about 30 fps on the ESP32-S3: about 14 ms to draw and 14 ms to flush,
with no lost time. The fall and beam-up effects still need a hardware timing
check; simulator speed is not a device performance measurement.
