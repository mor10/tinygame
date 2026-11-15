-- Rock Dodge: steer the ship with the right knob (or tilt) and dodge the falling rocks.
cart = { title = "Rock Dodge", tagline = "Don't get squashed", api = 1, fps = 30,
  icon = [[
    ................
    ..66.......88...
    .6666.....8888..
    .6666.....8888..
    ..66.......88...
    ......99........
    .....9999.......
    .....9999.......
    ......99........
    .......4........
    ......444.......
    ......4b4.......
    .....44444......
    ....4444444.....
    ...444444444....
    ...44.....44....
  ]] }

sprites = {
  ship = [[
    ....4....
    ...444...
    ...4b4...
    ..44444..
    .4444444.
    444444444
    44.....44
  ]],
}

function init()
  ship = { x = gfx.W // 2 - 4, y = gfx.H - 12, w = 9, h = 7 }
  rocks = group()
  score, timer, alive = 0, 0, true
  best = save.get("best", 0)
end

local function spawn()
  local r = rnd_int(3, 8)
  rocks:add({ x = rnd(gfx.W - 2 * r) + r, y = -r, r = r,
              vy = 1 + rnd(1.5) + score / 900, c = choose({ 6, 8, 9 }) })
end

local function touches(rock)            -- circle against the ship's box
  local nx = clamp(rock.x, ship.x, ship.x + ship.w)
  local ny = clamp(rock.y, ship.y, ship.y + ship.h)
  return dist(rock.x, rock.y, nx, ny) < rock.r
end

function update()
  if not alive then
    if knob.pressed("left") or knob.pressed("right") then init() end
    return
  end
  ship.x = clamp(ship.x + knob.turn("right") * 6 + tilt.x() * 4, 0, gfx.W - ship.w)
  timer = timer + 1
  if timer % math.max(5, 18 - score // 200) == 0 then spawn() end
  rocks:each(function(r)
    r.y = r.y + r.vy
    if r.y - r.r > gfx.H then rocks:remove(r) end
    if touches(r) then alive = false end
  end)
  if alive then
    score = score + 1
  elseif score > best then
    best = score
    save.set("best", best)
  end
end

local function centre(s, y, c, big)
  gfx.text(s, (gfx.W - gfx.text_width(s, big)) // 2, y, c, big)
end

function draw()
  gfx.clear(1)
  rocks:each(function(r) gfx.fill_circle(r.x, r.y, r.r, r.c) end)
  gfx.sprite("ship", ship.x, ship.y)
  gfx.text("SCORE " .. score // 10, 4, 3, 5)
  local b = "BEST " .. best // 10
  gfx.text(b, gfx.W - gfx.text_width(b) - 4, 3, 12)
  if not alive then
    centre("SQUASHED!", 60, 6, true)
    centre("Click a knob to try again", 84, 5)
  end
end
