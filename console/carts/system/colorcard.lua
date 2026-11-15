-- Colour Card: a stand-in cart for the OS colour card. The OS recognises the id "colorcard" and shows
-- its own screen (SMPTE-style bars, ramps, the OS colours and live gamma tuning), because a cart can
-- only use the 16 game colours. Click to change page, turn the scroll knob on the Tune page to adjust.
cart = {
  title = "Colour Card",
  tagline = "Check your screen colours",
  api = 1,
  fps = 30,
  icon = [[
    4488bbaaeeff7ddd
    4488bbaaeeff7ddd
    4488bbaaeeff7ddd
    4488bbaaeeff7ddd
    4488bbaaeeff7ddd
    4488bbaaeeff7ddd
    4488bbaaeeff7ddd
    4488bbaaeeff7ddd
    4488bbaaeeff7ddd
    4488bbaaeeff7ddd
    4488bbaaeeff7ddd
    4488bbaaeeff7ddd
    4444333322221111
    4444333322221111
    4444333322221111
    4444333322221111
  ]],
}

function draw()
  gfx.clear(0)
end
