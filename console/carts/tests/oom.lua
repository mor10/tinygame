-- Expect: "memory" error; the 1 MB cap stops it and the board does not reset.
cart = { title = "Memory hog", api = 1 }
local t = {}
function draw() gfx.clear(1) end
function update()
  for i = 1, 1000 do t[#t + 1] = string.rep("x", 100) .. #t end
end
