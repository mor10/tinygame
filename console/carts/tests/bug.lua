-- Expect: "bug" error at line 5 (calling a nil value).
cart = { title = "Bug", api = 1 }
function draw()
  gfx.clear(1)
  not_a_function()
end
