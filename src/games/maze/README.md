# Maze

Roll a ball through endless random mazes by tilting TinyGame. Reach the green
goal hole to reveal the next maze. Your ball starts exactly where the last
goal was, with its speed reset.

Created with GitHub Copilot from the player's Maze design.

## Controls

- Before play, turn the **main knob** (right knob, or left knob in left-handed mode) to choose **Chill** or **Thrill**, then **click**
  to start.
- **Tilt** to roll: right rolls right, and tipping toward the bottom of the
  screen rolls down. Hold nearly level to slow down.
- Turning and short clicks do nothing while rolling.
- **Long press** opens the console's pause menu: Resume / Restart / Quit.
  Restart returns to the mode picker with a fresh run. Pause freezes the timer,
  lasers, and level transition.

## Chill

Just mazes. No clock, hazards, or losing. Routes stay gentle and the HUD counts
how many mazes you have completed in this session.

## Thrill

Reach each goal before the clock runs out. Routes gradually get longer and
hazards increase to at most four of each type:

| Marking | Meaning |
|---------|---------|
| Green square ring | Goal hole: reach it to continue. |
| Red hole with an X | Trap: sends you back to this maze's starting point. |
| Dim laser anchors | Laser is off: safe to cross. |
| Short yellow beam | Warning: the laser is about to turn on. |
| Full red beam | Active laser: touching it sends you back to the start. |
| Pink posts and arrow | One-way gate through a wall: roll through in the arrow's direction. It blocks reverse entry. |

A laser or trap hit resets the ball's speed, **not the clock**. The ball turns
blue and the HUD says `SAFE` during one second of respawn protection.

Each maze has a safe route. Traps occupy optional dead ends. Gates add shortcuts
without taking away normal passages. Lasers leave safe places to wait and cycle
through 3 seconds off, 0.75 seconds warning, and 1.25 seconds active.

The timer allows for route length and laser waits; it resets for each new maze.
Running out of time ends the run. Long press and choose Restart to play again.
The launcher best score counts completed **Thrill** mazes only. It is saved after
each completion, so Quit does not discard a new best. Chill never changes it.

## Implementation

The grid stays 13 x 6 cells of 24 pixels throughout both modes so a previous
goal's screen coordinates can be reused exactly. Walls are 2 pixels thick and
the ball radius is 4 pixels. Generation uses a per-instance seeded generator and
a bounded iterative maze carver; limited placement uses fewer optional hazards
and reports the reduction in the serial log.

`model.h` / `model.cpp` contain host-safe rules and fixed-step rolling physics.
`view.h` / `view.cpp` pool the LVGL objects once. `game.cpp` connects the model to
TinyGame input, scores, logs, and LEDs. No OS or hardware changes are needed.

Timeout takes precedence over hazard contact, which takes precedence over the
goal. Level transitions last 0.6 seconds and freeze movement and the timer.
Timer budgets use route distance at 30 pixels/second, 2 seconds per laser, and a
20-second margin. Ball acceleration, friction, and maximum speed are the same
in both modes.

From the repository root:

```bash
python3 tools/tg.py build
python3 tools/tg.py test
python3 tools/tg.py flash
python3 tools/tg.py send "launch maze"
python3 tools/tg.py shot
```

Host tests cover seeded generation, safe routes, collisions, gate directions,
hazard phases, respawn, mode selection, timeout, and exact start continuity.
Rolling feel, real-device tilt direction, timer generosity, and LED colors still
need a human play check.
