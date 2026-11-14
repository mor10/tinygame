-- Expect: "stuck" error with line 6 within 0.5 s of the first update.
cart = { title = "Stuck", api = 1 }
function draw() gfx.clear(1) end
function update()
  local n = 0
  while true do n = n + 1 end
end
