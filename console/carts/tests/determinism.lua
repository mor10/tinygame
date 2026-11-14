-- Determinism check for the simulator: random numbers, float maths (sin, cos, sqrt, division),
-- sys.time and a group of moving objects. Board and simulator hashes must match frame for frame:
--   sim/compare.py PORT console/carts/tests/determinism.lua --frames 600
cart = { title = "Determinism", api = 1 }
sprites = { star = [[
  .c.
  ccc
  .c.
]] }

function init()
  dots = group()
  for i = 1, 60 do
    dots:add({ x = rnd(gfx.W), y = rnd(gfx.H), a = rnd(6.283), s = 0.5 + rnd(2), c = rnd_int(2, 15) })
  end
end

function update()
  local t = sys.time() / 1000
  dots:each(function(d)
    d.a = d.a + 0.05 * math.sin(t + d.s)
    d.x = (d.x + math.cos(d.a) * d.s) % gfx.W
    d.y = (d.y + math.sin(d.a) * d.s) % gfx.H
    if dist(d.x, d.y, gfx.W / 2, gfx.H / 2) < 12 then dots:remove(d) end
  end)
  if dots:count() < 60 and sys.frame() % 7 == 0 then
    dots:add({ x = rnd(gfx.W), y = 0, a = rnd(6.283), s = 0.5 + rnd(2), c = choose({ 6, 8, 10, 12 }) })
  end
end

function draw()
  gfx.clear(1)
  dots:each(function(d) gfx.fill_rect(d.x, d.y, 3, 3, d.c) end)
  gfx.sprite("star", gfx.W / 2 - 1, gfx.H / 2 - 1, false, false, 2)
  gfx.text(string.format("t=%.2f n=%d", sys.time() / 1000, dots:count()), 4, 4, 5)
end
