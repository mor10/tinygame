# Engine v1: sprites, maps, collision (Phase 4)

Status: approved and built (2026-11-14). The worked example is `console/carts/brick.lua`.
Names follow `docs/scope-v0.1.md` section 9.2; this note pins down the parts that section left open.

## Sprites

```lua
sprites = {
  ball = [[
    .44.
    4444
    4444
    .44.
  ]],
}
```

- One character per pixel. `.` is see-through; `0`-`9` and `a`-`v` are palette colours 0-31.
- The size comes from the art: every row must be the same length, up to 64x64. Leading
  indentation is ignored.
- Sprites and maps are decoded once, on first use or right after the cart file runs, so they can be used at the top of a cart. A bad character or a ragged row is a load error
  that names the sprite and the row.
- `gfx.sprite(name, x, y, [flip_x], [flip_y], [scale])`. `scale` is 1 or 2.

## Maps

```lua
maps = {
  level1 = {
    tile_w = 20, tile_h = 8,           -- or tile = 8 for square tiles
    legend = { r = "red", y = "yellow" },
    layers = { [[
      rrrrrrrr
      yyyyyyyy
    ]] },
  },
}
```

- One character per tile. `.` is always empty; every other character must appear in `legend`,
  which names the sprite to draw.
- Up to 4 layers per map. All layers in a map share one size.
- `map.load(name)` makes a map active and resets it to its original art, so restarting a level
  is one call.
- `gfx.map(layer, [x], [y])` draws a layer of the active map at an offset. Off-screen tiles
  are skipped.
- `map_get(layer, tx, ty)` returns the tile character, or `nil` outside the map.
  `map_set(layer, tx, ty, ch)` changes it. Tile coordinates start at 0.
- `map_hit(layer, x, y, w, h)` returns `ch, tx, ty` for the first non-empty tile under a
  pixel rectangle, or `nil`.

## Collision and groups

- `hit(a, b)` is true when two tables with `x, y, w, h` overlap.
- `group()` returns a list for many objects: `g:add(t)`, `g:remove(t)`, `g:each(fn)`,
  `g:count()`. It is safe to remove objects while `each` runs.
- Helpers from scope 9.2 that come along: `clamp`, `lerp`, `rnd`, `rnd_int`, `choose`, `dist`.

## Text

- The font gains lowercase, so `gfx.text("Click right to launch", ...)` prints as written.

## Decisions (approved in review)

1. Pixels are the characters `0`-`9` and `a`-`v` for colours 0-31, with `.` as see-through; there is no per-sprite legend.
2. Map tiles are single characters, so `map_get` returns `"r"`.
3. `map.load(name)` is the one way to pick and reset a level.
4. Tiles can be rectangular (`tile_w`/`tile_h`).
