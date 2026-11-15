-- Tilt Roll: tilt the TinyGame to roll the ball, grab every coin, then drop into the hole.
-- No tilting? The left knob pushes it left and right, the right knob up and down.
-- Click a knob while holding the TinyGame the way you like it to make that "flat".
cart = { title = "Tilt Roll", tagline = "Roll the ball, grab the coins", api = 1, fps = 30,
  icon = [[
    bbbbbbbbbbbbbbbb
    b..............b
    b.88.......bbb.b
    b.88.........b.b
    b......66....b.b
    b.....6666...b.b
    bbbb..6666.....b
    b......66......b
    b..............b
    b.bbbbbbbb..00.b
    b.........00000b
    b..88.....00000b
    b..88......00..b
    b..............b
    b..............b
    bbbbbbbbbbbbbbbb
  ]] }

sprites = {
  wall = [[
    cccccccccccccccc
    bbbbbbbbbbbbbbbb
    bbbbbbbbbbbbbbbb
    bbbbbbbbbbbbbbbb
    bbbbbbbbbbbbbbbb
    bbbbbbbbbbbbbbbb
    bbbbbbbbbbbbbbbb
    bbbbbbbbbbbbbbbb
    bbbbbbbbbbbbbbbb
    bbbbbbbbbbbbbbbb
    bbbbbbbbbbbbbbbb
    bbbbbbbbbbbbbbbb
    bbbbbbbbbbbbbbbb
    bbbbbbbbbbbbbbbb
    bbbbbbbbbbbbbbbb
    2222222222222222
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
  hole = [[
    ................
    ................
    ................
    .....222222.....
    ....20000002....
    ...2000000002...
    ...2000000002...
    ...2000000002...
    ...2000000002...
    ...2000000002...
    ...2000000002...
    ....20000002....
    .....222222.....
    ................
    ................
    ................
  ]],
}

maps = {
  maze = {
    tile = 16,
    legend = { ["#"] = "wall", c = "coin", o = "hole" },
    layers = { [[
      ####################
      #c.....#......#...c#
      #.####.#.####.#.##.#
      #......#....#......#
      ###.######..#.####.#
      #c...........#..c..#
      #.#####.####.#.###.#
      #.....#....#......o#
      #.c.......c.....#..#
      ####################
    ]] },
  },
}

local TOP, T, R = 12, 16, 5          -- top bar height, tile size, ball radius

local function wall_at(x, y)          -- is the pixel x, y inside a wall?
  return map_get(1, x // T, (y - TOP) // T) == "#"
end

local function blocked(x, y)          -- would the ball centred at x, y touch a wall?
  return wall_at(x - R, y - R) or wall_at(x + R, y - R) or wall_at(x - R, y + R) or wall_at(x + R, y + R)
end

function init()
  map.load("maze")
  ball = { x = 1.5 * T, y = TOP + 1.5 * T, vx = 0, vy = 0 }
  coins, total, frames, won = 0, 0, 0, false
  for ty = 0, 9 do                    -- count the coins in the map, so the level can change freely
    for tx = 0, 19 do
      if map_get(1, tx, ty) == "c" then total = total + 1 end
    end
  end
  best = save.get("best", 0)
end

function update()
  if won then
    if knob.pressed("left") or knob.pressed("right") then init() end
    return
  end
  if knob.pressed("left") or knob.pressed("right") then tilt.zero() end
  frames = frames + 1
  ball.vx = clamp((ball.vx + tilt.x() * 0.25 + knob.turn("left") * 0.8) * 0.96, -3, 3)
  ball.vy = clamp((ball.vy + tilt.y() * 0.25 + knob.turn("right") * 0.8) * 0.96, -3, 3)
  if blocked(ball.x + ball.vx, ball.y) then ball.vx = -ball.vx * 0.4 else ball.x = ball.x + ball.vx end
  if blocked(ball.x, ball.y + ball.vy) then ball.vy = -ball.vy * 0.4 else ball.y = ball.y + ball.vy end

  local tx, ty = ball.x // T, (ball.y - TOP) // T
  local tile = map_get(1, tx, ty)
  if tile == "c" then
    map_set(1, tx, ty, ".")
    coins = coins + 1
  elseif tile == "o" and coins == total then
    won = true
    if best == 0 or frames < best then best = frames; save.set("best", best) end
  end
end

function draw()
  gfx.clear(5)
  gfx.map(1, 0, TOP)
  gfx.fill_circle(ball.x, ball.y, R, 6)
  gfx.fill_rect(0, 0, gfx.W, TOP, 1)
  gfx.text("COINS " .. coins .. "/" .. total, 4, 2, 8)
  gfx.text(string.format("TIME %.1f", frames / 30), 110, 2, 5)
  -- Which way is down: a dot that leans with the tilt.
  gfx.circle(gfx.W - 10, 5, 4, 12)
  gfx.pixel(gfx.W - 10 + math.floor(tilt.x() * 3 + 0.5), 5 + math.floor(tilt.y() * 3 + 0.5), 4)
  if won then
    gfx.fill_rect(70, 56, 180, 50, 1)
    gfx.text("IN THE HOLE!", 88, 64, 8, true)
    gfx.text(string.format("%.1f s  (best %.1f s)", frames / 30, best / 30), 92, 88, 5)
  elseif coins < total and map_get(1, ball.x // T, (ball.y - TOP) // T) == "o" then
    gfx.text("Get all the coins first!", 92, gfx.H - 10, 6)
  end
end
