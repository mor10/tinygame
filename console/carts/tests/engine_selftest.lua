-- Expect: one log line "engine selftest: N passed" and no error.
cart = { title = "Engine selftest", api = 1 }
sprites = { dot = [[
  .1
  2.
]], wide = [[
  ab
]] }
maps = { m = { tile_w = 4, tile_h = 2, legend = { d = "dot", w = "wide" }, layers = { [[
  d..w
  ....
]], [[
  ....
  ...d
]] } } }

local n = 0
local function check(ok, what) if not ok then error("selftest failed: " .. what, 2) end n = n + 1 end

map.load("m")
check(map_get(1, 0, 0) == "d", "map_get start tile")
check(map_get(1, 3, 0) == "w", "map_get end tile")
check(map_get(1, 1, 0) == ".", "map_get empty tile")
check(map_get(1, 4, 0) == nil and map_get(1, -1, 0) == nil, "map_get outside")
check(map_get(2, 3, 1) == "d", "second layer")
local ch, tx, ty = map_hit(1, 13, 0, 2, 2)
check(ch == "w" and tx == 3 and ty == 0, "map_hit finds tile 3,0")
check(map_hit(1, 4, 0, 8, 4) == nil, "map_hit empty area")
map_set(1, 0, 0, ".")
check(map_get(1, 0, 0) == ".", "map_set clears")
map.load("m")
check(map_get(1, 0, 0) == "d", "map.load resets the level")
check(not pcall(map_set, 1, 0, 0, "q"), "map_set rejects a tile not in the legend")
check(not pcall(map_set, 1, 9, 9, "."), "map_set rejects outside")

check(hit({ x = 0, y = 0, w = 4, h = 4 }, { x = 3, y = 3, w = 4, h = 4 }), "hit overlap")
check(not hit({ x = 0, y = 0, w = 4, h = 4 }, { x = 4, y = 0, w = 4, h = 4 }), "hit touching edges is not a hit")
check(not pcall(hit, nil, { x = 0, y = 0, w = 1, h = 1 }), "hit rejects nil")

local g = group()
local a, b, c = g:add({ id = 1 }), g:add({ id = 2 }), g:add({ id = 3 })
local seen = {}
g:each(function(t) seen[#seen + 1] = t.id; if t.id == 1 then g:remove(b) end end)
check(#seen == 2 and seen[1] == 1 and seen[2] == 3, "remove during each skips the removed one")
check(g:count() == 2, "count after remove")
g:remove(a)
check(g:count() == 1, "remove outside each")

check(clamp(5, 0, 3) == 3 and clamp(-1, 0, 3) == 0 and lerp(0, 10, 0.5) == 5, "clamp and lerp")
check(dist(0, 0, 3, 4) == 5, "dist")
local r = rnd_int(2, 4); check(r >= 2 and r <= 4, "rnd_int range")
check(gfx.text_width("abc") == 17, "text width")
check(not pcall(gfx.sprite, "nope", 0, 0), "unknown sprite is an error")

sys.log("engine selftest: " .. n .. " passed")
function draw() gfx.clear(1); gfx.map(1); gfx.sprite("dot", 10, 10, true, false, 2); gfx.text("Selftest passed", 8, 40, 10) end
