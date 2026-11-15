-- WalkWalk: turn the right knob to walk (faster turning, faster walking), click a knob to jump.
-- Grab the coins and reach the door. Falling in a pit sends you back to the start.
cart = { title = "WalkWalk", tagline = "Walk, jump, find the door", api = 1, fps = 30,
  icon = [[
    ................
    ..........ffff..
    .........f2222f.
    ....8....f2222f.
    .........f2222f.
    ...666...f2282f.
    ..65556..f2222f.
    ..51515..f2222f.
    ..11111..f2222f.
    .1111111.f2222f.
    ..11111..f2222f.
    ..bb.bb..f2222f.
    ..bb.bb..f2222f.
    cccccccccccccccc
    aaaaaaaaaaaaaaaa
    ffffffffffffffff
  ]] }

sprites = {
  ground = [[
    cccccccccccccccc
    aaaaaaaaaaaaaaaa
    ffffffffffffffff
    ff9fffffff9fffff
    ffffffffffffffff
    ffffff9fffffff9f
    ffffffffffffffff
    ff9fffffff9fffff
    ffffffffffffffff
    ffffff9fffffff9f
    ffffffffffffffff
    ff9fffffff9fffff
    ffffffffffffffff
    ffffff9fffffff9f
    ffffffffffffffff
    ffffffffffffffff
  ]],
  block = [[
    9999999999999999
    9888888888888889
    9888888888888889
    9888888888888889
    9888888888888889
    9888888888888889
    9888888888888889
    9888888888888889
    9888888888888889
    9888888888888889
    9888888888888889
    9888888888888889
    9888888888888889
    9888888888888889
    9888888888888889
    9999999999999999
  ]],
  coin = [[
    ................
    ................
    ................
    ................
    ......8888......
    .....888888.....
    ....88899888....
    ....88988888....
    ....88888888....
    ....88888888....
    .....888888.....
    ......8888......
    ................
    ................
    ................
    ................
  ]],
  door = [[
    ....ffffffff....
    ...ffffffffff...
    ..ff22222222ff..
    ..ff22222222ff..
    ..ff22222222ff..
    ..ff22222222ff..
    ..ff22222222ff..
    ..ff222222282f..
    ..ff22222222ff..
    ..ff22222222ff..
    ..ff22222222ff..
    ..ff22222222ff..
    ..ff22222222ff..
    ..ff22222222ff..
    ..ff22222222ff..
    ..ffffffffffff..
  ]],
  hero = [[
    ...6666...
    ..666666..
    ..655556..
    ..515515..
    ..555555..
    ...5555...
    .11111111.
    1111111111
    5.111111.5
    ..111111..
    ..bb..bb..
    ..bb..bb..
    ..bb..bb..
    .11...11..
  ]],
}

maps = {
  level = {
    tile = 16,
    legend = { ["#"] = "ground", ["="] = "block", c = "coin", D = "door" },
    layers = { [[
      ................................................
      ................................................
      ..............................c.c.c.............
      .............................=======............
      ..............c.c.......................ccc.....
      .............=====.....................=====....
      ......c.c.............c............c............
      .....=====...........===........................
      ..................###.........................D.
      ##########...##############...##################
    ]] },
  },
}

local T, TOP, COLS = 16, 12, 48

local function solid(x, y)
  local t = map_get(1, x // T, (y - TOP) // T)
  return t == "#" or t == "="
end

local function hits(x, y)             -- would the hero's 10x14 box at x, y touch ground or a block?
  return solid(x, y) or solid(x + 9, y) or solid(x, y + 13) or solid(x + 9, y + 13)
      or solid(x, y + 7) or solid(x + 9, y + 7)
end

local function respawn()
  hero = { x = 2 * T, y = TOP + 7 * T, vx = 0, vy = 0, ground = false, left = false }
end

function init()
  map.load("level")
  respawn()
  coins, won = 0, false
end

function update()
  if won then
    if knob.pressed("left") or knob.pressed("right") then init() end
    return
  end
  hero.vx = clamp(knob.speed("right") / 32, -2.5, 2.5)
  if hero.vx < -0.1 then hero.left = true elseif hero.vx > 0.1 then hero.left = false end
  if hero.ground and (knob.pressed("left") or knob.pressed("right")) then hero.vy = -5.5 end
  hero.vy = math.min(hero.vy + 0.35, 6)

  local nx = clamp(hero.x + hero.vx, 0, COLS * T - 10)
  if not hits(nx, hero.y) then hero.x = nx end
  local ny = hero.y + hero.vy
  hero.ground = false
  if hits(hero.x, ny) then
    if hero.vy > 0 then hero.ground = true end
    hero.vy = 0
  else
    hero.y = ny
  end
  if hero.y > gfx.H then respawn() end

  local cx, cy = (hero.x + 5) // T, (hero.y + 7 - TOP) // T
  local tile = map_get(1, cx, cy)
  if tile == "c" then map_set(1, cx, cy, "."); coins = coins + 1 end
  if tile == "D" then won = true end
end

function draw()
  local cam = clamp(hero.x - gfx.W // 2, 0, COLS * T - gfx.W)
  gfx.clear(12)
  gfx.fill_circle(260 - cam // 4, 40, 18, 5)          -- a far-away sun scrolls slowly
  gfx.map(1, -cam, TOP)
  gfx.sprite("hero", hero.x - cam, hero.y, hero.left)
  gfx.fill_rect(0, 0, gfx.W, TOP, 1)
  gfx.text("COINS " .. coins, 4, 2, 8)
  if won then
    gfx.fill_rect(80, 60, 160, 40, 1)
    gfx.text("YOU MADE IT!", 92, 68, 8, true)
    gfx.text("Click a knob to play again", 84, 88, 5)
  end
end
