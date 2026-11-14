-- Expect: load error: map 'level1' layer 1 row 2 has 'x', which isn't in its legend.
cart = { title = "Bad map", api = 1 }
sprites = { brick = [[
  66
]] }
maps = { level1 = { tile = 2, legend = { b = "brick" }, layers = { [[
  bbbb
  bxbb
]] } } }
function draw() end
