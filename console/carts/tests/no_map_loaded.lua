-- Expect: "bug" at line 4: call map.load("name") before using a map.
cart = { title = "No map loaded", api = 1 }
maps = {}
function draw() gfx.map(1) end
