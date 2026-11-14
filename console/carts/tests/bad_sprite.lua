-- Expect: load error naming sprite 'ship' row 2 and the bad character 'z'.
cart = { title = "Bad sprite", api = 1 }
sprites = { ship = [[
  .11.
  1z11
]] }
function draw() end
