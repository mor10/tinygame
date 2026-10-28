# TinyGame style guide

**The look:** Pocket Arcade. Dark cartridge-style menus, little pixel emblems,
accent rails and framed score panels. Keep it blocky, readable and restrained.
Tokens live in `include/tg_style.h`; shared changes bump `STYLE_VERSION`
(currently 2). To check the result, open System > Style and take a `shot`.

## Colours (`tg::color::`)

| Token | Use |
|---|---|
| `BG` | screen background (near-black blue) |
| `SURFACE` | panels, HUD strips |
| `LINE` | borders, dividers, empty bar tracks |
| `TEXT` / `TEXT_DIM` | normal text / hints and labels |
| `ACCENT` | titles, the selection, the main highlight |
| `GOOD` / `WARN` / `BAD` | success, extra life / caution, best score / danger, damage |

**Game palette:** 16 bright colours, also in `tg::PALETTE[16]`:

- `BLACK`, `NAVY`, `PLUM`, `FOREST`
- `BROWN`, `SLATE`, `SILVER`, `WHITE`
- `RED`, `ORANGE`, `YELLOW`, `LIME`
- `SKY`, `MINT`, `PINK`, `PEACH`

## Fonts (`tg::Font::`)

These are one 8×8 pixel face, scaled by whole pixels so they stay crisp.

| Size | Height | Characters per line | Use |
|---|---|---|---|
| `S` | 8 | 40 | HUD numbers, hints, small labels |
| `M` | 16 | 20 | default text, menus |
| `L` | 24 | 13 | titles, big score |
| `XL` | 32 | 10 | splash titles only |

## Layout

- Snap positions and sizes to a 4 px grid: `tg::GAP_S` 4, `GAP_M` 8, `GAP_L` 16.
- Borders are `tg::BORDER` (2 px) and corners are always square.
- **Standard page:**
  - `ui::header(screen, "TITLE")` across the top, 28 px
  - content in between
  - `ui::footer(screen, "TURN: MOVE  CLICK: JUMP")` along the bottom edge
- **Game HUD:** an 18 px `SURFACE` strip at the top, with `Font::S` text. Score on the left, level in the middle, lives as small blocks on the right (see Brick Breaker).
- **Messages:** `ui::popup` for game over or level up, centred.
- **Results:** `ui::result(popup, score, best, new_best)` gives the score its own
  medium-size row and keeps the best/action hint separate. `popupValue` is
  available for other counts. Do not concatenate every value into one long line.
- **Launcher:** `ui::Menu` derives letter badges from game names; no game list
  or icon manifest is needed. The best-score row is separate from controls.
- Font widths are 8/16/24/32 px per character, but line boxes are taller.
  Leave room for the line box, borders and padding, not just glyph pixels.
  Cartridge rows use 1 px outlines so an 18 px text line fits a 20 px row;
  larger panels retain the shared 2 px border.
- Check maximum values, not just zero: a 32-bit score can have ten digits.

## Do

- Use solid fills and square blocks. Pixel-art shapes are made from `block`s or a canvas.
- Put a few strong colours on a dark background. Leave empty space.
- Show the selection the 8-bit way, as an inverted block: coloured fill, `BG` text (`tg::style::selected()` or `ui::Menu`).
- Write hint text in capitals, in the form `VERB: ACTION`, for example `CLICK: LAUNCH  HOLD: PAUSE`.
- Flash the LED for big moments: hit, power-up, level clear.

## Don't

- No light backgrounds, gradients, shadows, rounded corners or semi-transparent fades.
- No anti-aliased fonts (Montserrat). They break the pixel look.
- No more than 3–4 colours fighting for attention on one screen.
- Use `M` for menus and primary result values. Reserve `S` for compact HUD
  readouts and control hints; do not shrink text further to hide an overflow.

## When a game wants something different

A game can define its own colours, styles or a custom font inside its own folder. Name them for what they are (`LAVA_ORANGE`), and keep the console chrome (HUD, popups, hints) on the shared tokens so every game still feels like TinyGame. If several games want the same new thing, add it to `tg_style.h` instead.
