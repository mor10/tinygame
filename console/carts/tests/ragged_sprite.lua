-- Expect: load error: sprite 'ship' row 2 is 3 wide, but row 1 is 4 wide.
cart = { title = "Ragged sprite", api = 1 }
sprites = { ship = [[
  .11.
  111
]] }
function draw() end
