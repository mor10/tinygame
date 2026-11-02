# TILTBLOCK

**Made by:** mor10

Roll a ball over a stack of eight cube layers. Touch every cube on the current
layer to drop to the next one; clear all eight to start a harder level.

## How to play

- At the start of every run, turn the main knob to choose **TILT** or **TWIST**,
  then click either knob.
- In **TILT**, move the dot into the center of the square and click either
  knob to level the play surface. Tilt the console to roll the ball.
- In **TWIST**, the left knob tips the board along its front-left diagonal:
  clockwise rolls the ball toward the center/right, and counterclockwise
  rolls it the other way. The right knob controls the opposite diagonal:
  clockwise rolls toward the center/right, and counterclockwise rolls toward
  the center/left. Each angle stays where you leave it.
- The ball keeps its momentum. Roll into a wall to bounce off it and touch its
  side; a wall cube must be touched just like a floor cube.
- Avoid holes. A hole in the middle traps the ball; a hole on the edge sends it
  off the block. Either costs a life. You start with five lives.
- Touch every safe cube on a layer. Touched cubes stay touched after a fall.
  The ball comes back near the middle with a short safety blink.
- Click either knob to fire an armed laser. Hold either knob for the console's
  pause menu.
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

Walls begin on level 2, holes on level 3, bonuses on level 4, and the alien
appears from level 6. The alien drifts toward the ball without following the
block's tilt. Find the orange laser cube, then click while rolling toward the
alien. A hit is worth 500 points.

Score 10 points for each touched cube, 100 times the level for each cleared
layer, 1,000 times the level for a cleared level, and 500 for an alien. Your
best score is saved; each new run starts at level 1 with five lives.

## Development

The board generator, fixed-step rolling physics and game rules are host-tested
in `test/`. Run `python3 tools/tg.py test`, `python3 tools/tg.py build`, and
`python3 tools/tg.py sim --build-only` from the repository root.

For simulator playtests only, send `turn right 1233` on the mode screen to
start on layer 4, or `turn right 1234` to start at level 6 with an alien on
the first layer. A physical knob cannot produce that many transitions in one
update.
