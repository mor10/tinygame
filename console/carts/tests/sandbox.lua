-- Expect: one log line listing what is reachable; every entry should be "nil" except the last two
-- checks, which should report the errors raised by writing to gfx and by a bad colour.
cart = { title = "Sandbox probe", api = 1 }
local r = {}
for _, name in ipairs({ "io", "os", "debug", "package", "require", "load", "loadfile", "dofile",
                        "collectgarbage", "loadstring" }) do
  r[#r + 1] = name .. "=" .. type(_G[name])
end
r[#r + 1] = "string.dump=" .. type(string.dump)
r[#r + 1] = "gfx_write=" .. tostring(select(2, pcall(function() gfx.clear = nil end)))
r[#r + 1] = "bad_colour=" .. tostring(select(2, pcall(function() gfx.clear(99) end)))
r[#r + 1] = "getmetatable(gfx)=" .. tostring(getmetatable(gfx))
sys.log(table.concat(r, " "))
function draw() gfx.clear(10) gfx.text("SANDBOX PROBE DONE", 8, 8, 4) end
