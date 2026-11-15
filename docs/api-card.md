# TinyGame API card (api 1)

Write TinyGame games ("carts") from this page. A cart is one `.lua` file in Lua 5.4. The
console runs it at a fixed 30 frames per second on a 320x172 screen, with two clicky knobs and a
tilt sensor. Check every cart with `tinygame-sim --check cart.lua` before sending it.

## Shape of a cart

```lua
cart = {
  title = "Rock Dodge",            -- up to 23 characters, shown in the game gallery
  tagline = "Don't get squashed",  -- up to 47 characters
  api = 1, fps = 30,
  icon = [[                         -- 16x16 text art, shown on the gallery tile (see Sprites)
    ................
    ......6666......
    ...  (16 rows of 16) ...
  ]],
}

function init() end     -- once, at the start
function update() end   -- every frame: read input, move things
function draw() end     -- every frame: draw everything (required)
```

- Every frame is the same length (1/30 s). There is no `dt`: move things a fixed amount per frame.
- The screen keeps last frame's picture until you draw over it. Start `draw()` with `gfx.clear(c)`.
- Positions are pixels from the top-left. Fractional positions are rounded down when drawn.
- Always use `gfx.W` and `gfx.H` for the screen size; another TinyGame model is 280x240.

## Colours

Use the numbers 0-15:

| 0 black | 1 navy | 2 dark grey | 3 grey | 4 white | 5 cream | 6 coral | 7 red |
|---|---|---|---|---|---|---|---|
| **8 yellow** | **9 orange** | **10 green** | **11 teal** | **12 mint** | **13 blue** | **14 pink** | **15 brown** |

In text art, `0`-`9` and `a`-`f` are these colours (`a` = 10 ... `f` = 15).

## Drawing: `gfx`

| Call | What it does |
|---|---|
| `gfx.clear(c)` | fill the screen with colour c |
| `gfx.pixel(x, y, c)` | one pixel |
| `gfx.line(x1, y1, x2, y2, c)` | a line |
| `gfx.rect(x, y, w, h, c)` / `gfx.fill_rect(x, y, w, h, c)` | box outline / filled box |
| `gfx.circle(x, y, r, c)` / `gfx.fill_circle(x, y, r, c)` | circle around the centre x, y |
| `gfx.text(s, x, y, c, [big])` | text, 5x7 pixel letters (6 px apart); `big` doubles it. Returns the width |
| `gfx.text_width(s, [big])` | width of text in pixels, for centring |
| `gfx.sprite(name, x, y, [flip_x], [flip_y], [scale])` | draw a sprite; scale is 1 or 2 |
| `gfx.map(layer, [x], [y])` | draw a layer of the loaded map, offset by x, y (use this to scroll) |
| `gfx.W`, `gfx.H` | screen width and height (320 and 172) |

Text has upper and lower case, digits and most punctuation (no `{ } | ~` or backtick).

## Sprites

```lua
sprites = {
  ship = [[
    ..4..
    .444.
    44444
  ]],
}
```

One character per pixel: `.` is see-through, `0`-`9` and `a`-`f` are colours. Every row must be
the same length; the size comes from the art (up to 64x64). Leading spaces are ignored.

## Maps (tile levels)

```lua
maps = {
  level1 = {
    tile = 8,                          -- or tile_w = 20, tile_h = 8 for non-square tiles
    legend = { ["#"] = "wall", c = "coin" },   -- tile character -> sprite name
    layers = { [[
      ########
      #..c...#
      ########
    ]] },
  },
}
```

| Call | What it does |
|---|---|
| `map.load(name)` | make a map the current one and reset it to its original art (restarts a level) |
| `map_get(layer, tx, ty)` | the tile character at tile column tx, row ty (from 0); `"."` is empty, `nil` is off the map |
| `map_set(layer, tx, ty, ch)` | change a tile (`"."` empties it); ch must be in the legend |
| `map_hit(layer, x, y, w, h)` | for a box in pixels: returns `ch, tx, ty` of the first non-empty tile it touches, or `nil` |

Layers are numbered from 1 (up to 4). Tile coordinates are `x // tile` and `y // tile`.

## Knobs: `knob`

The two knobs turn forever (no end stops) and click when pressed. `side` is `"left"` or `"right"`.

| Call | What it gives |
|---|---|
| `knob.turn(side)` | notches turned this frame: + is clockwise, - is anticlockwise (20 notches per full turn) |
| `knob.pos(side)` | total position in fine steps (80 per full turn): good for direct control like a paddle |
| `knob.speed(side)` | fine steps per second, smoothed |
| `knob.pressed(side)` | true on the frame the click goes down |
| `knob.held(side)` | true while held down |
| `knob.released(side)` | true on the frame it is let go |
| `knob.long(side)` | true once, after holding for 0.6 s |

Clicking both knobs together pauses the game (the console handles that), so don't give that a
meaning in your game.

## Tilt: `tilt`

| Call | What it gives |
|---|---|
| `tilt.x()` | -1 to 1: positive when the right edge is lower (things should roll right) |
| `tilt.y()` | -1 to 1: positive when the bottom edge is lower (things should roll down) |
| `tilt.shake()` | true on a frame with a shake |
| `tilt.zero()` | make the current angle count as flat |

Tilt is for gentle steering, never precise aiming. Always offer a knob way to play too, and show
which way the game thinks is down.

## Saving: `save`

| Call | What it does |
|---|---|
| `save.set(key, value)` | store a number, string or true/false under a key (up to 32 characters); `nil` removes it |
| `save.get(key, default)` | the stored value, or `default` if there is none |
| `save.clear()` | forget everything this game saved |

Saves survive power cuts and restarts. Each game gets 4 KB; storing more is an error. Typical use:
`best = save.get("best", 0)`, and `save.set("best", best)` when it changes.

## System: `sys`

`sys.time()` milliseconds since the game started, `sys.frame()` frame number, `sys.fps()` the
measured frame rate, `sys.log(...)` a message for the developer (also `print`).

## Helpers

| Helper | What it does |
|---|---|
| `hit(a, b)` | true if two tables with `x, y, w, h` overlap |
| `group()` | a list for many things: `g:add(t)`, `g:remove(t)`, `g:each(fn)`, `g:count()`. Removing inside `each` is safe |
| `clamp(v, lo, hi)`, `lerp(a, b, t)`, `dist(x1, y1, x2, y2)` | maths |
| `rnd(n)` | a random number from 0 up to n (not including n) |
| `rnd_int(a, b)` | a random whole number from a to b, both included |
| `choose(list)` | a random item from a list |

## Lua on TinyGame

- Lua 5.4 with `string`, `table`, `math`, `utf8`, `pcall`, `setmetatable` and friends.
- Not available: `io`, `os`, `require`, `load`, `dofile`, `debug`, `collectgarbage`, coroutines.
- Numbers are 32-bit. Whole numbers stay whole (`//` is whole-number division, `%` remainder) and
  wrap around past about 2 billion. Decimals have about 7 digits of precision.
- A number that has been through `/` or a decimal prints with a `.0` ("SCORE 39.0"). Keep scores
  whole by adding whole numbers, or show them with `math.floor(score)` or `score // 1`.

## Limits

1 MB of memory per game. Each `update()` or `draw()` must finish in 0.25 s, or the game stops with
"Your game got stuck". Carts up to 128 KB, 256 sprites, 16 maps of up to 256x256 tiles.

## Don't

- Don't use other consoles' words: no `btn()`, `btnp()`, `spr()`, `cls()`, `pset()`, `rectfill()`,
  `love.*`, `pygame`, `screen.blit`. Use the calls on this page.
- Don't write `+=`, `-=`, `!=`, `++` or `//` comments: Lua has `x = x + 1`, `~=` and `--` comments.
- Don't assume a 128x128 (or any other) screen: use `gfx.W` and `gfx.H`.
- Don't take a `dt` argument or time things with `os.clock()`: count frames or use `sys.time()`.
- Don't call `require`, `load` or anything in `io` or `os`.
- Don't use colours above 15 or names like `"red"`: colours are numbers.
- Don't loop forever waiting for something inside `update()`: keep state and check it each frame.
