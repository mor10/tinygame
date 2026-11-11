# Steps files (`tg run`)

A steps file is a tiny script that plays a game and checks what happened.
Games keep theirs in `test/*.steps`. One command per line; blank lines are
ignored and `#` starts a comment (a whole line, or the end of a line).

```
# run right, jump, and check the score
tilt 1 0
wait 30
press
wait 1
release
wait 40
expect score >= 10
```

| Line | What it does |
|---|---|
| `tilt <x> <y>` | tilt the console, from -1 to 1; stays that way until the next `tilt` |
| `turn left <n>` / `turn right <n>` | turn the main (right) knob `n` transitions (80 are one full turn) on the next tick |
| `press` | hold the main knob down; it stays down until `release` |
| `release` | let the knob go |
| `wait <ticks>` | let the game run; 60 ticks are one second |
| `expect <name> <op> <number>` | check something; see below |

Time only passes in `wait`. Input lines (`tilt`, `turn`, `press`, `release`)
set up what the game sees during the **next** `wait`. A `press` shows up as a
click on the first tick of that wait, and a `release` as the knob coming up.
So "tap to jump" is `press`, `wait 1`, `release`, `wait 1`.

## expect

`<op>` is one of `==  !=  <  <=  >  >=`. `<name>` is one of:

`score`, `hurts` (times the player was hurt), `tick` (ticks so far),
`actors` (living things on the stage), `player.x`, `player.y`, `player.vx`,
`player.vy` (blocks and blocks per second), `player.ground` (1 on the ground,
else 0), and `count.<kind>` (how many living actors of that kind, for example
`count.gem`; a kind that is not there counts 0, so check the spelling).

A failed `expect` does not stop the script: the run goes on and every failure
is reported, with the file, the line, what was expected and what was found.
The run then fails (non-zero exit). A mistake in the script itself (an unknown
word, a missing number) is reported with its line before anything is played.
`expect player...` fails with "there is no player" if the player is gone.
A game that stops with an error also fails the run.

`tg run <game> --steps <file>` plays the script on Board A (320x172) and Board B
(280x240); `--board a|b` picks one, `--probe` prints the final probe text,
`--shot <png>` saves the last frame, `--json` prints the result as JSON.
