-- Built-in test cart: left knob moves the paddle, right click serves the ball.
cart = { title = "Hello", author = "TinyTwist", api = 1, fps = 30, tagline = "Paddle, ball, go",
         icon = [[
           ................
           ................
           ......8888......
           ....88888888....
           ...8888888888...
           ...8818888188...
           ...8888888888...
           ...8888888888...
           ...8866666688...
           ....88666688....
           ......8888......
           ................
           ................
           ..bbbbbbbbbbbb..
           ..bbbbbbbbbbbb..
           ................
         ]] }

function init()
  px = gfx.W // 2
  bx, by, vx, vy = px, 120, 0, 0
  served = false
  bounces = 0
end

function update()
  px = math.max(24, math.min(gfx.W - 24, px + knob.turn("left") * 8))
  if not served then
    bx, by = px, 140
    if knob.pressed("right") then served, vx, vy = true, 3, -3 end
    return
  end
  bx, by = bx + vx, by + vy
  if bx < 3 or bx > gfx.W - 3 then vx = -vx end
  if by < 20 then vy = -vy end
  if by >= 145 and by <= 150 and math.abs(bx - px) <= 24 and vy > 0 then
    vy = -vy
    bounces = bounces + 1
  end
  if by > gfx.H then served = false end
end

function draw()
  gfx.clear(1)
  gfx.text("HELLO TINYTWIST", 8, 6, 5, true)
  gfx.text("BOUNCES " .. bounces, 8, 26, 8)
  gfx.fill_rect(px - 24, 150, 48, 5, 11)
  gfx.fill_rect(bx - 3, by - 3, 6, 6, 6)
  if not served then gfx.text("CLICK RIGHT TO SERVE", 100, 100, 12) end
end
