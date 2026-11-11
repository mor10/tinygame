# Bundle format (`.tgb`), version 1

A bundle is one game, packed by `tg pack` from the text files in a game folder.
The console and the simulator read only bundles. All integers are
little-endian. A *string* is one length byte followed by that many ASCII bytes
(no terminator).

Limits: whole bundle at most 262,144 bytes; at most 255 sprites, 255 tiles,
64 kinds, 32 levels; a level has at most 16,384 cells and 256 spawns; a sprite
is at most 32 x 32 art pixels; tile sprites are exactly 8 x 8.

## Header (16 bytes)

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | magic `TGB1` |
| 4 | 2 | api version (currently 1) |
| 6 | 2 | section count N |
| 8 | 4 | total size of the bundle in bytes |
| 12 | 4 | CRC-32 (zlib polynomial) of every byte after the header |

## Section table (N x 12 bytes, straight after the header)

| Size | Field |
|---|---|
| 4 | tag (four ASCII characters) |
| 4 | offset from the start of the bundle |
| 4 | size in bytes |

Sections may come in any order. A reader ignores tags it does not know.
`META`, `PALT`, `SPRT`, `TILE`, `KIND`, `TUNE` and `LEVL` are required;
`RULE` is optional.

## Sections

**`META`**: strings `id`, `name`, `author`, `starter`; then one byte `view`
(0 standard, 1 fill), one byte `playfield_cols`, one byte `playfield_rows`
(both 0 when the game uses the whole stage).

**`PALT`** (36 bytes): 16 RGB565 colours (entry 0 is unused: index 0 is
transparent), then the background colour, then the ink (text) colour.

**`SPRT`**: `u16` count, then per sprite: string `name`, `u8` width, `u8`
height, then `ceil(width * height / 2)` bytes of 4-bit palette indexes. Pixel
`i` (row by row) is the low nibble of byte `i / 2` when `i` is even and the
high nibble when `i` is odd.

**`TILE`**: `u8` count, then per tile: `u16` sprite index, `u16` flags.
Flags: bit 0 solid, bit 1 one-way (solid only from above), bit 2 hazard.
Tile numbers in level cells are 1-based: cell value `n` is tile `n - 1`.

**`KIND`**: `u8` count, then per kind (a fixed record in version 1; it becomes
a parameter list when the parts registry drives the packer):

| Size | Field |
|---|---|
| string | name |
| 2 | sprite index |
| 1 | movement: 0 none, 1 platformer |
| 1 | behaviour: 0 none, 1 walker, 2 pickup |
| 1 | flags: bit 0 = controlled by the player |
| 1 | on touching the player: 0 nothing, 1 hurt |
| 4 | speed, `float32`, blocks per second |
| 4 | points, `int32` (score for a pickup) |

**`TUNE`**: `u8` count, then per entry: string key, `float32` value. Version 1
keys: `gravity` (blocks/s^2), `jump_height` (blocks), `run_speed` (blocks/s).
Unknown keys are ignored by the engine and rejected by `tg check`.

**`LEVL`**: `u8` count, then per level: string name, `u16` cols, `u16` rows,
`cols * rows` cell bytes (row by row, 0 = empty), `u16` spawn count, then per
spawn `u16` kind index, `u16` col, `u16` row. The spawn cell is where the
actor stands: the actor's bottom-left block is placed on that cell.

**`RULE`**: Lua source text, exactly as written in `rules.lua`. Never bytecode.

## Text sources (what `tg pack` reads)

`game.toml`:

```toml
api = 1
id = "cave_hop"            # [a-z][a-z0-9_]{0,14}; must equal the folder name
name = "CAVE HOP"          # 1-13 printable ASCII
author = "Sam"
starter = "platformer"
view = "standard"          # or "fill"

[tuning]
gravity = 30
jump_height = 3
run_speed = 6

[tile.ground]              # tiles: sprite plus optional solid / one_way / hazard
sprite = "ground"
solid = true

[kind.slime]               # kinds: sprite plus optional movement, behaviour,
sprite = "slime"           # player, on_touch_player, speed, points
movement = "platformer"
behaviour = "walker"
speed = 2
on_touch_player = "hurt"

[palette]                  # optional: override entries, e.g. 4 = "#F25C54",
background = "#BFE6F5"     # background, ink
```

Without a `[palette]` table the bundle carries the platform default palette
(the BRIGHT candidate in `engine/testcard/testcard.cpp`, until the palette
decision is made).

`sprites.txt`: blocks starting `== <name> <W>x<H>` followed by H rows of W
characters; `.` is transparent, `1`-`9` and `a`-`f` are palette indexes
(`0` is also accepted as transparent). Blank lines and lines starting `#`
between blocks are ignored.

`levels.txt`: blocks starting `== <level name>`, then one or more
`legend: <char> = <tile or kind name>, ...` lines, then the rows. `.` is
always empty. Every row has the same width. A level is at least 17 columns
wide and at least 10 rows tall. A character naming a tile fills the cell; a
character naming a kind leaves the cell empty and adds a spawn.

`rules.lua`: optional. See `lua-api.md`.

## Details the packer settles

- Sprite, tile and kind names match `[A-Za-z0-9_]{1,31}`. A tile and a kind may
  not share a name, because a level legend could not tell them apart.
- Sprite rows accept `.`, `0`-`9` and lowercase `a`-`f` only.
- `TUNE` always lists all three keys. A key missing from `game.toml` takes its
  default from `engine/parts.toml`.
- A kind's `speed` and `points` take their default from `engine/parts.toml`
  only when the kind's behaviour uses them; otherwise they are 0. A value
  written in `game.toml` is range-checked and stored either way.
- `[palette]` keys are `"1"` to `"15"`, `background` and `ink`, each a
  `"#RRGGBB"` string. Entry 0 is always 0.
- The spawn cell is the cell where the character is typed in the level. The
  packer does not move it for taller sprites; the engine places the actor's
  bottom-left block on that cell.
- The engine's loader checks the binary format and its limits. The rules about
  the text files (level at least 17 by 10, exactly one player per level, name
  lengths) are enforced by `tg check`, not by the loader.
- Sections are written in this order: META, PALT, SPRT, TILE, KIND, TUNE, LEVL,
  then RULE when `rules.lua` exists.
