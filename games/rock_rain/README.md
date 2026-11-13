# ROCK RAIN

This is a playable Lua/bundle prototype of the TinyGame Rock Rain game.
Bricks fall toward your robot. Dodge them, shoot them, and catch capsules.

## How to play

- Turn the main knob to move left and right.
- Press either knob to shoot. You start with 5 shots.
- Catch falling capsules for ammo, rapid fire, or an extra life.
- You start with 3 lives. A hit makes the robot briefly invulnerable.
- Click either knob after game over to play again.

Every game starts with six rows of bricks with gaps. Slip through the gaps or
shoot bricks to make your own opening. Small bricks take one shot; big bricks
take two. Survive for points and earn more by breaking bricks. Falling bricks
that pass the robot do no harm.

Every so often a BOSS brick slides across the screen and drops more bricks.
Hit it ten times to clear it and start a faster wave. While it is around,
AMMO and RAPID capsules fall more often. AMMO adds five shots (up to 20),
RAPID gives five seconds of free shooting, and LIFE adds a heart (up to 5).
If six capsules in a row are not LIFE, the next one is guaranteed to be LIFE.

This bundle port keeps the original dodge, shoot, brick-wave, capsule, boss,
and survival loop. Its bounded Lua object pools and drawing all run in the
shared bundle runtime.

Based on the original TinyGame Rock Rain by Morten Rand-Hendriksen (with
Claude).
