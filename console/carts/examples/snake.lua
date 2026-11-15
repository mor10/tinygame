-- Snake: turn the right knob to steer (one notch = one quarter turn), eat, grow, don't bite yourself.
cart = { title = "Snake", tagline = "Eat, grow, don't bite yourself", api = 1, fps = 30,
  icon = [[
    ................
    ................
    ..aaaaaaaa......
    ..aaaaaaaa......
    ..aa....aa......
    ..aa....aa......
    ..aa....aa..66..
    ..aa....aa..66..
    ..aa....aa......
    ..aa....aaaaaa..
    ..aa....aaaaaa..
    ..aa............
    ..aa............
    ..a4a...........
    ................
    ................
  ]] }

local CELL, TOP = 8, 12
local COLS, ROWS = gfx.W // CELL, (gfx.H - TOP) // CELL
local DIRS = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } }   -- right, down, left, up (clockwise)

local function place_food()
  repeat
    food = { x = rnd_int(0, COLS - 1), y = rnd_int(0, ROWS - 1) }
    local clash = false
    for _, s in ipairs(body) do if s.x == food.x and s.y == food.y then clash = true end end
  until not clash
end

function init()
  body = { { x = 10, y = 9 }, { x = 9, y = 9 }, { x = 8, y = 9 } }
  dir, grow, tick, alive = 1, 0, 0, true
  best = save.get("best", 0)
  place_food()
end

function update()
  if not alive then
    if knob.pressed("left") or knob.pressed("right") then init() end
    return
  end
  dir = (dir - 1 + knob.turn("right")) % 4 + 1
  tick = tick + 1
  if tick % 4 ~= 0 then return end
  local head = { x = (body[1].x + DIRS[dir][1]) % COLS, y = (body[1].y + DIRS[dir][2]) % ROWS }
  for i = 1, #body - 1 do
    if body[i].x == head.x and body[i].y == head.y then alive = false end
  end
  if not alive then
    if #body - 3 > best then best = #body - 3; save.set("best", best) end
    return
  end
  table.insert(body, 1, head)
  if head.x == food.x and head.y == food.y then grow = grow + 2; place_food() end
  if grow > 0 then grow = grow - 1 else table.remove(body) end
end

function draw()
  gfx.clear(1)
  gfx.fill_rect(0, 0, gfx.W, TOP - 2, 2)
  gfx.text("LENGTH " .. #body, 4, 2, 5)
  gfx.text("BEST " .. best, gfx.W - 60, 2, 12)
  gfx.fill_circle(food.x * CELL + 4, TOP + food.y * CELL + 4, 3, 6)
  for i, s in ipairs(body) do
    gfx.fill_rect(s.x * CELL, TOP + s.y * CELL, CELL - 1, CELL - 1, i == 1 and 12 or 10)
  end
  if not alive then
    gfx.text("OUCH!", 130, 70, 6, true)
    gfx.text("Click a knob to play again", 82, 94, 5)
  end
end
