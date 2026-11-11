# Spike: can TiltBlock be drawn through raw draw calls from Lua?

- **Date:** 2026-11-11
- **Method:** static read of v1 `src/games/tiltblock/game.cpp` (`drawBoard`,
  `drawStackSide`, `drawWall`, `drawBall`, `drawBeamCube`). Nothing was run or
  measured. The board size is taken to be 8 x 8 cells.
- **Result:** probably yes, with two additions to the kit. Not confirmed until
  the Lua speed spike runs on the console.

## What one frame draws (ordinary play)

| Source | Per item | Items | Filled polygons | Lines |
|---|---|---|---|---|
| Floor cells | 1 diamond, 4 outline lines | 64 | 64 | 256 |
| Stack sides | per band: 1 quad, 3 to 4 lines | 16 edge cells x up to about 5 bands | up to 80 | up to 320 |
| Raised walls | 2 quads, 1 diamond, 5 lines | varies, say 10 | 30 | 50 |
| Ball, shadow, monster, effects | boxes and a few lines | | about 20 boxes | about 10 |

Roughly 150 to 200 filled polygons and 400 to 650 lines per frame, redrawn at
30 frames per second: on the order of 20,000 to 25,000 draw calls per second,
each preceded by an isometric projection of its points.

## Findings

1. **The spec's raw drawing list is missing a filled polygon.** Section 10
   names rectangles, lines, sprites and text. TiltBlock is built almost
   entirely from filled four- and six-point polygons. Add a convex polygon
   call (v1 `framebuffer.cpp` already has one to carry over).
2. **Per-call cost from Lua is the risk, not the drawing.** The rasteriser is
   C++ either way. The cost is about 25,000 binding calls per second plus the
   projection arithmetic in Lua. This is an estimate; whether it fits beside
   the game logic depends on the Lua speed spike.
3. **A batch call would remove most of that cost.** For example one call that
   takes a table of polygons, or an engine-side "iso grid" helper that projects
   and draws a whole board from a cell array. Propose it only if the Lua spike
   shows the per-call path is too slow.
4. TiltBlock's own 3x5 font and colour-fade tables are not needed from Lua:
   the font is now `engine/gfx/font.*`, and fades can be palette ramps.

## Proposed spec changes (not applied)

- Section 10, raw drawing: add filled convex polygon and thick line.
- Section 18: note that TiltBlock may need a batch draw or iso-grid helper,
  to be decided after the Lua speed spike.
