-- Expect: "memory" error; pcall must not swallow out-of-memory either.
cart = { title = "Memory hog in pcall", api = 1 }
local t = {}
function draw() gfx.clear(1) end
function update()
  pcall(function() for i = 1, 1000 do t[#t + 1] = string.rep("y", 10000) .. i end end)
end
