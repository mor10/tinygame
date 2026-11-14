-- OS shell mock (Phase 6 review): Welcome, Cartridges, Game Card, Settings, Sound and Paused,
-- drawn to match docs/ref-designs. It runs as an ordinary cart with a made-up catalogue so it can
-- be reviewed in the simulator before the firmware wiring exists.
-- Controls (OS-wide): right-handed (default) turns the right knob to browse or adjust; left-handed
-- turns the left knob. Either knob clicks to select. Back is something you scroll to: the "<" on
-- the gallery and game card, and the BACK row in Settings. Settings > HANDS switches sides.
-- In the mock, any click during the fake game opens the pause menu (the firmware uses the chord).
cart = { title = "TinyGame", api = 1, fps = 30 }

sprites = {
  knob = [[
    ..111..
    .1...1.
    1..1..1
    1..1..1
    1.....1
    .1...1.
    ..111..
  ]],
  knob_light = [[
    ..555..
    .5...5.
    5..5..5
    5..5..5
    5.....5
    .5...5.
    ..555..
  ]],
  pig = [[
    ................
    ...ee......ee...
    ..eee......eee..
    ..eeeeeeeeeeee..
    .eeeeeeeeeeeeee.
    .eee1eeeeee1eee.
    .eeeeeeeeeeeeee.
    .eeeee7777eeeee.
    .eeee717717eeee.
    .eeeee7777eeeee.
    .eeeeeeeeeeeeee.
    ..eeeeeeeeeeee..
    ...eeeeeeeeee...
    ...ee......ee...
    ...ee......ee...
    ................
  ]],
  boat = [[
    ................
    .......f........
    .......f44......
    .......f444.....
    .......f4444....
    .......f44444...
    .......f444444..
    .......f4444444.
    .......f........
    ..666666666666..
    ...6666666666...
    ....66666666....
    ................
    bb..bb..bb..bb..
    .bb..bb..bb..bb.
    ................
  ]],
  goose = [[
    ................
    ......444.......
    .....44414......
    .....4444999....
    .....4444.......
    ......444.......
    ......444.......
    ......4444......
    .....444444.....
    ...4444444444...
    ..444444444444..
    ..444444444444..
    ...4444444444...
    .....44444......
    ......9..9......
    .....99.99......
  ]],
  runner = [[
    ................
    .........44.....
    .........44.....
    ......4444......
    .....4.444......
    ....4..4.44.....
    ......44...4....
    ......44........
    .....4..4.......
    ....4....4......
    ...4......4.....
    ..4.......4.....
    ................
    ................
    ................
    ................
  ]],
  bricks = [[
    ................
    .66.88.bb.66.88.
    .66.88.bb.66.88.
    ................
    .88.bb.66.88.bb.
    .88.bb.66.88.bb.
    ................
    ................
    ..........44....
    ..........44....
    ................
    ................
    ................
    ....bbbbbbbb....
    ....bbbbbbbb....
    ................
  ]],
}

-- Colours
local NAVY, GREY, WHITE, CREAM, CORAL, RED, YELLOW, TEAL, MINT = 1, 3, 4, 5, 6, 7, 8, 11, 12

local games = {
  { name = "Brick Bop", tagline = "Bounce it!", icon = "bricks", tile = TEAL },
  { name = "Sail Away", tagline = "Catch the wind", icon = "boat", tile = YELLOW },
  { name = "Snack Heist", tagline = "A little snack adventure", icon = "pig", tile = CORAL },
  { name = "Goose Chase", tagline = "Honk if you're winning", icon = "goose", tile = YELLOW },
  { name = "Runner", tagline = "Don't stop now", icon = "runner", tile = TEAL },
}

-- ---------- drawing helpers ----------
local function box(x, y, w, h, c)            -- a box with its corners clipped by one pixel
  gfx.fill_rect(x + 1, y, w - 2, h, c)
  gfx.fill_rect(x, y + 1, w, h - 2, c)
end

local function button(x, y, w, h, c, shadow)  -- raised button, like the reference designs
  box(x, y + 3, w, h, shadow)
  box(x, y, w, h, c)
end

local function centre_text(s, cx, y, c, big)
  gfx.text(s, cx - gfx.text_width(s, big) // 2, y, c, big)
end

local function logo(x, y, s)                  -- the three-block mark
  gfx.fill_rect(x + s // 2, y, s, s, YELLOW)
  gfx.fill_rect(x, y + s, s, s, TEAL)
  gfx.fill_rect(x + s, y + s, s, s, CORAL)
end

local function battery(x, y)
  gfx.rect(x, y, 18, 9, NAVY)
  gfx.fill_rect(x + 18, y + 3, 2, 3, NAVY)
  for i = 0, 2 do gfx.fill_rect(x + 2 + i * 5, y + 2, 4, 5, NAVY) end
end

-- Handedness: the scroll knob turns to browse and adjust; either knob clicks to select.
-- Right-handed (default) scrolls with the right knob; left-handed with the left.
local hand = "right"

local function footer(scroll_label, select_label, light)
  local y = gfx.H - 16
  local icon, ink = light and "knob_light" or "knob", light and CREAM or NAVY
  local left, right = select_label, scroll_label
  if hand == "left" then left, right = scroll_label, select_label end
  gfx.sprite(icon, 92, y)
  gfx.text(left, 103, y, ink)
  gfx.sprite(icon, 182, y)
  gfx.text(right, 193, y, ink)
end

local function back_arrow(focused)            -- the scrollable "<" that stands in for a back button
  if focused then button(6, 4, 22, 20, CORAL, RED) end
  gfx.text("<", 12, 8, focused and WHITE or NAVY, true)
end

local function header(title)
  logo(10, 6, 5)
  gfx.text(title, 30, 6, NAVY, true)
  battery(gfx.W - 30, 8)
end

-- ---------- screens ----------
local screen, focus = "welcome", 1
local pick = 3                                 -- selected cartridge
local settings_row, sound, light = 1, 6, 8      -- levels out of 10
local SETTINGS_ROWS = { "SOUND", "LIGHT", "HANDS", "BACK" }
local game_t, paused_from = 0, nil

local function go(s, f) screen, focus = s, f or 1 end

local function draw_welcome()
  gfx.clear(CREAM)
  header("TinyGame")
  logo(62, 52, 16)
  local items = { "START", "Resume", "Settings" }
  button(146, 50, 124, 30, focus == 1 and CORAL or GREY, focus == 1 and RED or 2)
  centre_text("START", 208, 58, WHITE, true)
  for i = 2, 3 do
    local y = 92 + (i - 2) * 14
    local c = focus == i and CORAL or NAVY
    if focus == i then gfx.text(">", 172, y, c) end
    gfx.text(items[i], 182, y, c)
  end
  footer("Browse", "Select")
end

local function draw_tile(g, x, y, selected)
  local size = selected and 48 or 40
  if selected then y = y - 6 end
  button(x, y, size, size, selected and CORAL or g.tile, NAVY)
  box(x + 4, y + 4, size - 8, size - 8, CREAM)
  local s = 2
  gfx.sprite(g.icon, x + size // 2 - 16, y + size // 2 - 16, false, false, s)
end

local function draw_gallery()
  gfx.clear(CREAM)
  back_arrow(pick == 0)
  gfx.text("GAMES", 34, 8, NAVY, true)
  local n = #games
  local spacing = 58
  local p = math.max(pick, 1)
  local first = math.max(1, math.min(p - 2, n - 4))
  for i = first, math.min(n, first + 4) do
    local slot = i - first
    local x = 22 + slot * spacing
    draw_tile(games[i], i == pick and x - 4 or x, 46, i == pick)
  end
  if pick == 0 then
    centre_text("BACK", gfx.W // 2, 110, NAVY, true)
  else
    local sel = 22 + (pick - first) * spacing + 20
    gfx.fill_rect(sel - 3, 104, 7, 1, NAVY)
    gfx.fill_rect(sel - 2, 103, 5, 1, NAVY)
    gfx.fill_rect(sel - 1, 102, 3, 1, NAVY)
    gfx.fill_rect(sel, 101, 1, 1, NAVY)
    centre_text(string.upper(games[pick].name), gfx.W // 2, 110, NAVY, true)
    centre_text(pick .. " / " .. n, gfx.W // 2, 128, NAVY)
  end
  footer("Browse", "Select")
end

local function draw_card()
  local g = games[pick]
  gfx.clear(CREAM)
  back_arrow(focus == 0)
  gfx.text(pick .. " / " .. #games, gfx.W - 40, 10, NAVY)
  -- hero: the icon, big, on a little stage
  gfx.fill_rect(30, 112, 100, 2, MINT)
  gfx.sprite(g.icon, 48, 46, false, false, 2)
  gfx.sprite(g.icon, 92, 70, true, false, 2)
  centre_text(string.upper(g.name), 228, 40, NAVY, true)
  centre_text(g.tagline, 228, 62, NAVY)
  button(176, 82, 104, 26, focus == 1 and CORAL or GREY, focus == 1 and RED or 2)
  centre_text("PLAY", 228, 88, WHITE, true)
  footer("Browse", "Select")
end

local function bars(x, y, level)               -- five-step meter for the settings rows
  for i = 1, 5 do gfx.fill_rect(x + (i - 1) * 9, y, 7, 7, i <= (level + 1) // 2 and TEAL or MINT) end
end

local function draw_settings()
  gfx.clear(CREAM)
  gfx.text("SETTINGS", 12, 10, NAVY, true)
  for i, r in ipairs(SETTINGS_ROWS) do
    local y = 34 + (i - 1) * 22
    if settings_row == i then gfx.text(">", 14, y + 3, CORAL) end
    gfx.text(r, 26, y, settings_row == i and CORAL or NAVY, true)
    if i == 1 then bars(240, y + 4, sound)
    elseif i == 2 then bars(240, y + 4, light)
    elseif i == 3 then gfx.text(hand == "right" and "RIGHT" or "LEFT", 240, y, NAVY, true) end
  end
  footer("Browse", settings_row == 3 and "Switch" or "Edit")
end

local function draw_level(title, level)
  gfx.clear(CREAM)
  gfx.text(title, 12, 10, NAVY, true)
  centre_text((level * 10) .. "%", gfx.W // 2, 46, NAVY, true)
  for i = 1, 10 do
    gfx.fill_rect(70 + (i - 1) * 18, 76, 14, 14, i <= level and TEAL or MINT)
  end
  centre_text("TURN THE " .. string.upper(hand) .. " KNOB TO ADJUST", gfx.W // 2, 104, GREY)
  footer("Level", "Done")
end

local function draw_fake_game()
  gfx.clear(NAVY)
  for i = 0, 4 do gfx.fill_rect(20 + i * 60, 30 + (i % 2) * 10, 40, 6, i % 2 == 0 and CORAL or YELLOW) end
  local x = 160 + math.floor(math.sin(game_t / 10) * 120)
  gfx.fill_rect(x, 120, 6, 6, WHITE)
  centre_text("Pretend game. Click a knob to pause.", gfx.W // 2, 150, MINT)
end

local function draw_paused()
  -- The game frame stays visible behind the menu; the firmware will dim it with a palette swap.
  draw_fake_game()
  gfx.fill_rect(0, 0, gfx.W, gfx.H - 18, NAVY)
  for i = 0, 4 do gfx.fill_rect(20 + i * 60, 30 + (i % 2) * 10, 40, 6, 2) end
  box(84, 20, 152, 112, CREAM)
  gfx.text("PAUSED", 98, 30, NAVY, true)
  local items = { "CONTINUE", "SETTINGS", "EXIT GAME" }
  for i, it in ipairs(items) do
    local y = 54 + (i - 1) * 24
    if focus == i then
      button(106, y - 4, 118, 20, CORAL, RED)
      gfx.text(">", 96, y + 1, NAVY)
    end
    gfx.text(it, 112, y, focus == i and WHITE or NAVY, true)
  end
  gfx.fill_rect(0, gfx.H - 18, gfx.W, 18, NAVY)
  footer("Browse", "Select", true)
end

-- ---------- input ----------
local function scroll() return knob.turn(hand) end

local function browse(lo, hi)
  local t = scroll()
  if t ~= 0 then focus = clamp(focus + t, lo, hi) end
end

function update()
  local select = knob.pressed("left") or knob.pressed("right")
  if screen == "welcome" then
    browse(1, 3)
    if select then
      if focus == 1 then go("gallery") elseif focus == 2 then go("game") else paused_from = nil; go("settings") end
    end
  elseif screen == "gallery" then
    pick = clamp(pick + scroll(), 0, #games)
    if select then if pick == 0 then pick = 1; go("welcome") else go("card") end end
  elseif screen == "card" then
    browse(0, 1)
    if select then if focus == 0 then go("gallery") else go("game") end end
  elseif screen == "game" then
    game_t = game_t + 1
    if select then go("paused") end
  elseif screen == "paused" then
    browse(1, 3)
    if select then
      if focus == 1 then go("game") elseif focus == 2 then paused_from = "paused"; go("settings") else go("gallery") end
    end
  elseif screen == "settings" then
    settings_row = clamp(settings_row + scroll(), 1, #SETTINGS_ROWS)
    if select then
      if settings_row == 1 then go("sound")
      elseif settings_row == 2 then go("light")
      elseif settings_row == 3 then hand = hand == "right" and "left" or "right"
      else settings_row = 1; go(paused_from or "welcome", paused_from and 2 or 3) end
    end
  elseif screen == "sound" then
    sound = clamp(sound + scroll(), 0, 10)
    if select then go("settings") end
  elseif screen == "light" then
    light = clamp(light + scroll(), 1, 10)
    if select then go("settings") end
  end
end

function draw()
  if screen == "welcome" then draw_welcome()
  elseif screen == "gallery" then draw_gallery()
  elseif screen == "card" then draw_card()
  elseif screen == "game" then draw_fake_game()
  elseif screen == "paused" then draw_paused()
  elseif screen == "settings" then draw_settings()
  elseif screen == "sound" then draw_level("SOUND", sound)
  elseif screen == "light" then draw_level("LIGHT", light)
  end
end
