-- Expect: still "stuck"; pcall must not swallow the watchdog.
cart = { title = "Stuck in pcall", api = 1 }
function draw() gfx.clear(1) end
function update()
  while true do pcall(function() while true do end end) end
end
