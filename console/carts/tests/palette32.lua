-- Shows all 32 game colours as a grid, plus a pink pig drawn with text art that uses the new
-- characters g-v: s is bubblegum pink (28), t pale pink (29), r hot pink (27). A run that completes with no errors means colours 16-31 and the g-v sprite
-- characters work.
cart = { title = "Palette 32", tagline = "All 32 colours", api = 1, fps = 30 }

sprites = {
  pig = [[
    ................
    ..tt........tt..
    .tttt......tttt.
    .ssssssssssssss.
    .ssssssssssssss.
    .ss1ssssssss1ss.
    .ssssssssssssss.
    .ssssrrrrrrssss.
    .ssssr1rr1rssss.
    .ssssrrrrrrssss.
    .ssssssssssssss.
    ..ssssssssssss..
    ...ssssssssss...
    ...ss......ss...
    ...tt......tt...
    ................
  ]],
}

function draw()
  gfx.clear(0)
  for i = 0, 31 do
    gfx.fill_rect((i % 8) * 20, math.floor(i / 8) * 18, 20, 18, i)
  end
  gfx.sprite("pig", 220, 100)
  gfx.text("32", 10, 100, 4, true)
end
