-- Knob Sketch: the left knob draws across, the right knob draws up and down, like an Etch A Sketch.
-- Click left to change colour, click right to lift or lower the pen, shake to wipe the picture.
cart = { title = "Knob Sketch", tagline = "Draw with two knobs, shake to wipe", api = 1, fps = 30,
  icon = [[
    5555555555555555
    5..............5
    5..66666.......5
    5......6.......5
    5......6.......5
    5......66666...5
    5..........6...5
    5..........6...5
    5..bbbbbbbbb...5
    5..b...........5
    5..b...........5
    5..............5
    5555555555555555
    .33..........33.
    .33..........33.
    ................
  ]] }

local TOP = 12
local COLOURS = { 1, 6, 8, 10, 11, 13, 14, 4 }

local function wipe()
  gfx.clear(5)
  wiped = true
end

function init()
  x, y = gfx.W // 2, (gfx.H + TOP) // 2
  last_l, last_r = knob.pos("left"), knob.pos("right")
  px, py = x, y
  colour, pen = 1, true
  wiped = false
end

function update()
  if knob.pressed("left") then colour = colour % #COLOURS + 1 end
  if knob.pressed("right") then pen = not pen end
  if tilt.shake() then wiped = false end
  local l, r = knob.pos("left"), knob.pos("right")
  px, py = x, y                           -- this frame's stroke goes from px, py to x, y
  x = clamp(x + (l - last_l) / 2, 0, gfx.W - 1)
  y = clamp(y + (r - last_r) / 2, TOP, gfx.H - 1)
  last_l, last_r = l, r
end

function draw()
  if not wiped then wipe() end
  -- The picture stays on screen between frames, so each frame only adds its own stroke.
  if pen then gfx.line(px, py, x, y, COLOURS[colour]) end
  gfx.fill_rect(0, 0, gfx.W, TOP - 1, 1)
  for i, c in ipairs(COLOURS) do
    gfx.fill_rect(4 + (i - 1) * 12, 2, 10, 7, c)
    if i == colour then gfx.rect(3 + (i - 1) * 12, 1, 12, 9, 4) end
  end
  gfx.text(pen and "PEN DOWN" or "PEN UP", 110, 2, pen and 12 or 3)
  gfx.text("SHAKE TO WIPE", gfx.W - 82, 2, 3)
end
