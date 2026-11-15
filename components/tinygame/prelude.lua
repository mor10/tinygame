-- Helpers every cart gets (engine v1). Runs in the cart's sandbox before the cart itself.
-- Errors use level 2 so the message points at the cart line that called the helper.

local function need_box(v, what)
  if type(v) ~= "table" or type(v.x) ~= "number" or type(v.y) ~= "number"
     or type(v.w) ~= "number" or type(v.h) ~= "number" then
    error(what .. " needs a table with x, y, w and h", 3)
  end
end

function hit(a, b)
  need_box(a, "hit(a, b): a")
  need_box(b, "hit(a, b): b")
  return a.x < b.x + b.w and b.x < a.x + a.w and a.y < b.y + b.h and b.y < a.y + a.h
end

function clamp(v, lo, hi)
  if v < lo then return lo elseif v > hi then return hi else return v end
end

function lerp(a, b, t) return a + (b - a) * t end

function dist(x1, y1, x2, y2)
  local dx, dy = x2 - x1, y2 - y1
  return math.sqrt(dx * dx + dy * dy)
end

function rnd(n) return math.random() * (n or 1) end
function rnd_int(a, b) return math.random(a, b) end

function choose(list)
  if type(list) ~= "table" or #list == 0 then error("choose(list) needs a list with something in it", 2) end
  return list[math.random(#list)]
end

-- group(): a list of objects that is safe to change while you loop over it.
local Group = {}
Group.__index = Group

function group()
  return setmetatable({ items = {}, looping = 0, holes = false }, Group)
end

function Group:add(t)
  if type(t) ~= "table" then error("group:add needs a table", 2) end
  self.items[#self.items + 1] = t
  return t
end

function Group:remove(t)
  local items = self.items
  for i = 1, #items do
    if items[i] == t then
      if self.looping > 0 then items[i] = false; self.holes = true
      else table.remove(items, i) end
      return
    end
  end
end

function Group:each(fn)
  local items = self.items
  self.looping = self.looping + 1
  for i = 1, #items do
    local t = items[i]
    if t then fn(t) end
  end
  self.looping = self.looping - 1
  if self.looping == 0 and self.holes then
    local j = 1
    for i = 1, #items do
      if items[i] then items[j] = items[i]; j = j + 1 end
    end
    for i = j, #items do items[i] = nil end
    self.holes = false
  end
end

function Group:count()
  local n = 0
  for i = 1, #self.items do if self.items[i] then n = n + 1 end end
  return n
end

-- ui: the console's look for games. The screen has rounded corners and a thick bezel, so keep
-- everything inside ui.PAD of the sides and below the bar. Colours: 24 deep blue (bars), 23 electric
-- blue (backgrounds), 18 yellow (the one action), 4 white (cards), 1 navy (text and drop-edges).
ui = { BAR = 28, PAD = 16 }

-- the top bar: draw it last so nothing slides under it. Put your own text at ui.PAD, y = 9.
function ui.bar()
  gfx.fill_rect(0, 0, gfx.W, ui.BAR, 24)
  gfx.fill_rect(0, ui.BAR - 1, gfx.W, 3, 1)
end

local HEART = { "..XXX.XXX..", ".XXXXXXXXX.", "XXXXXXXXXXX", "XXXXXXXXXXX", ".XXXXXXXXX.", "..XXXXXXX..",
                "...XXXXX...", "....XXX....", ".....X....." }
-- a heart 11 px wide and 9 px tall with its top left corner at x, y
function ui.heart(x, y, c)
  for r = 1, #HEART do
    local row = HEART[r]
    for i = 1, #row do
      if row:byte(i) == 88 then gfx.pixel(x + i - 1, y + r - 1, c or 16) end
    end
  end
  gfx.pixel(x + 2, y + 1, 4)
end

-- Control icons, the same ones the console uses at the bottom of its own screens: a ring with an
-- arrowhead means TURN the knob, an arrow pressing onto a surface means CLICK it.
local ICONS = {
  turn = { "........#####", "....####.###.", "..######..#..", "..##.........", ".##..........", ".##.......##.",
           ".##.......##.", ".##.......##.", ".##.......##.", "..##.....##..", "..#########..", "....#####....", "............." },
  click = { ".....###.....", ".....###.....", ".....###.....", ".....###.....", "..#########..", "...#######...",
            "....#####....", ".....###.....", "......#......", ".............", ".###########.", ".###########.", "............." },
}
-- ui.icon("turn" or "click", x, y, colour): a 13 x 13 icon with its top left corner at x, y
function ui.icon(kind, x, y, c)
  local g = ICONS[kind]
  if not g then return end
  for r = 1, 13 do
    local row = g[r]
    for i = 1, 13 do
      if row:byte(i) == 35 then gfx.pixel(x + i - 1, y + r - 1, c) end
    end
  end
end

-- which knob moves things and which one acts, for the player's Hands setting: returns "right", "left"
-- for right-handed players and "left", "right" for left-handed ones. Call it every frame.
function ui.sides()
  if sys.hands() == "left" then return "left", "right" end
  return "right", "left"
end

-- a pill with a hint in it. `icon` ("turn" or "click") adds a control icon in front of the text.
-- x is the left edge (default: centred on the screen); returns the pill's width.
function ui.pill(s, y, c, icon, x)
  local w = gfx.label_width(s, 2) + 28 + (icon and 19 or 0)
  x = x or (gfx.W - w) // 2
  gfx.fill_rect(x + 3, y + 3, w, 24, 1)
  gfx.fill_rect(x, y, w, 24, c or 18)
  local tx = x + 14
  if icon then ui.icon(icon, x + 12, y + 5, 1); tx = x + 31 end
  gfx.label(s, tx, y + 6, 1, 2)
  return w
end

-- Tell the player the controls, laid out like the knobs: each item is { icon, side, text }, e.g.
-- ui.controls(116, { { "click", act, "Launch" }, { "turn", mv, "Move" } }). Items for the left knob sit on the
-- left edge, items for the right knob on the right edge (inside the safe margin).
function ui.controls(y, list)
  for _, it in ipairs(list) do
    local w = gfx.label_width(it[3], 2) + 47
    local x = it[2] == "left" and ui.PAD or gfx.W - ui.PAD - w - 3
    ui.pill(it[3], y, it[1] == "click" and 18 or 4, it[1], x)
  end
end

-- a block with its four corners cut off at 45 degrees (cut = how many pixels)
function ui.plate(x, y, w, h, c, cut)
  cut = cut or 4
  for j = 0, cut - 1 do
    local inset = cut - j
    gfx.fill_rect(x + inset, y + j, w - 2 * inset, 1, c)
    gfx.fill_rect(x + inset, y + h - 1 - j, w - 2 * inset, 1, c)
  end
  gfx.fill_rect(x, y + cut, w, h - 2 * cut, c)
end

-- a four-point sparkle of radius r centred on x, y
function ui.star(x, y, r, c)
  gfx.fill_rect(x - r, y, 2 * r + 1, 1, c)
  gfx.fill_rect(x, y - r, 1, 2 * r + 1, c)
  if r > 2 then gfx.fill_rect(x - 1, y - 1, 3, 3, c) end
end

local STRIPE = { 16, 17, 18, 20, 23, 27, 25 }

-- a big card in the middle of the screen for level intros, wins and game over. It drops in from
-- the top and bounces when `age` (frames since it appeared) is small. c = the card colour
-- (default violet 25; 16 red is good for game over), line = a second line, hint = a yellow pill.
function ui.card(title, line, hint, c, age, icon)
  c = c or 25
  age = age or 99
  local size = 4
  if gfx.label_width(title, 4) > 210 then size = 3 end
  local cap = size == 4 and 23 or 17
  local w = 252
  local h = 24 + cap + (line and 25 or 0) + (hint and 38 or 0) + 24
  local dy = 0
  if age < 10 then dy = -math.floor(gfx.H * ((10 - age) / 10) ^ 2)
  elseif age < 17 then dy = -math.floor(math.sin((age - 10) / 7 * math.pi) * 6) end
  local x, y = (gfx.W - w) // 2, (gfx.H - h) // 2 + 10 + dy
  ui.plate(x + 4, y + 4, w, h, 24, 8)
  ui.plate(x, y, w, h, c, 8)
  local f = sys.frame() // 2
  for i = 0, 17 do   -- a strip of little bricks along the top and bottom that scrolls sideways
    local sx = x + 12 + i * 13 - (f % 13)
    if sx >= x + 12 and sx + 11 <= x + w - 12 then
      local col = STRIPE[(i + f // 13) % #STRIPE + 1]
      gfx.fill_rect(sx, y + 8, 11, 6, col)
      gfx.fill_rect(sx, y + h - 14, 11, 6, col)
    end
  end
  local ty = y + 24
  gfx.label(title, x + (w - gfx.label_width(title, size)) // 2, ty, 4, size)
  ty = ty + cap + 14
  if line then
    gfx.label(line, x + (w - gfx.label_width(line, 2)) // 2, ty, 18, 2)
    ty = ty + 25
  end
  if hint then ui.pill(hint, ty - 2 + (line and 0 or -2) + math.floor(math.sin(sys.frame() * 0.2) * 1.5), 18, icon) end
  local t = sys.frame()
  local spots = { { x - 6, y - 4 }, { x + w + 2, y + 6 }, { x + w - 14, y + h + 4 }, { x + 10, y + h + 2 } }
  for i, sp in ipairs(spots) do
    local r = 1 + (t + i * 5) % 20 // 4   -- twinkles from small to big and back
    if r > 3 then r = 6 - r end
    ui.star(sp[1], sp[2], r + 1, i % 2 == 0 and 18 or 4)
  end
end
