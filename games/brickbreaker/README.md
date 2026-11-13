# BRICK BREAKER

Turn the main knob to move the paddle. Click either knob to launch the ball.
Bounce it into every brick, then catch it again with the paddle. Grey bricks
need two hits. You have three lives; clearing a wall starts the next layout.
After game over, click to start a new game.

The first bundle-runtime version keeps the familiar paddle-and-ball rules,
five-by-ten brick field, score, lives and six repeating layouts. Power-ups,
lasers, multiball, best-score storage and game-specific pause screens are not part of this
small playable port; the operating system still owns the two-knob pause gesture.

The simulator runs this Lua port instead of the native game. On the device,
the native game still owns the same `brickbreaker` ID; this bundle cannot
currently replace it. Simulator play is not physical bundle-play verification.

Run `python3 tools/tg.py check brickbreaker` and
`python3 tools/tg.py pack brickbreaker` to validate and pack the game.
