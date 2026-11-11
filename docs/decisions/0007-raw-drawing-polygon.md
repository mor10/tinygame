# Raw drawing includes filled convex polygons and thick lines

- **Date:** 2026-11-11
- **Status:** decided (approved by the maintainer with the stage plan)

## Question
The spec's raw drawing list had rectangles, lines, sprites and text. Is that enough?

## Options considered
- Keep the list as it was.
- Add a filled convex polygon and a line thickness.

## Decision
Add both. Spec section 10 is updated. Not yet implemented in `engine/gfx/`.

## Evidence
`docs/spikes/tiltblock-raw-drawing.md`: v1 TiltBlock draws roughly 150 to 200 filled polygons per frame and cannot be ported without them. v1 `src/games/tiltblock/framebuffer.cpp` has a polygon fill to carry over.

## Reopen if
The Lua speed spike shows per-call drawing from Lua is too slow, in which case a batch call or an iso-grid helper replaces most uses.
