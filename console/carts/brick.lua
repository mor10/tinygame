-- Brick Breaker: the Phase 4 target cart, written against the proposed engine v1 API.
-- Left knob moves the paddle. Right click launches the ball.
cart = { title = "Brick Bop", author = "TinyTwist", api = 1, fps = 30,
         inputs = { "knobs" }, tagline = "Bounce it!",
         icon = [[
           ................
           .66.88.bb.66.88.
           .66.88.bb.66.88.
           ................
           .88.bb.66.88.bb.
           .88.bb.66.88.bb.
           ................
           ................
           ..........44....
           ..........44....
           ................
           ................
           ................
           ....bbbbbbbb....
           ....bbbbbbbb....
           ................
         ]] }

-- Sprites are text art: one character per pixel. "." is see-through,
-- 0-9 and a-f are palette colours 0-15. The size comes from the art itself.
sprites = {
  ball = [[
    .44.
    4444
    4444
    .44.
  ]],
  paddle = [[
    .bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.
    bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb
    bbcccccccccccccccccccccccccccccccccccccccccbb
    .bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb.
  ]],
  red = [[
    666666666666666666
    666666666666666666
    666666666666666666
    666666666666666666
    777777777777777777
  ]],
  yellow = [[
    888888888888888888
    888888888888888888
    888888888888888888
    888888888888888888
    999999999999999999
  ]],
  teal = [[
    bbbbbbbbbbbbbbbbbb
    bbbbbbbbbbbbbbbbbb
    bbbbbbbbbbbbbbbbbb
    bbbbbbbbbbbbbbbbbb
    cccccccccccccccccc
  ]],
  heart = [[
    .66.66.
    6666666
    6666666
    .66666.
    ..666..
    ...6...
  ]],
}

-- Maps are text grids: one character per tile, looked up in the legend.
-- "." is always an empty tile. Tiles here are 20x8 pixels (a brick plus a gap).
maps = {
  level1 = {
    tile_w = 20, tile_h = 8,
    legend = { r = "red", y = "yellow", t = "teal" },
    layers = { [[
      ................
      ................
      rrrrrrrrrrrrrrrr
      yyyyyyyyyyyyyyyy
      tttttttttttttttt
      rrrrrrrrrrrrrrrr
      yyyyyyyyyyyyyyyy
    ]] },
  },
}

function init()
  score, lives = 0, 3
  map.load("level1")
  reset_ball()
end

function reset_ball()
  paddle = { x = gfx.W // 2 - 22, y = 156, w = 45, h = 4 }
  ball = { x = 0, y = 0, w = 4, h = 4, vx = 0, vy = 0, stuck = true }
end

function update()
  paddle.x = clamp(paddle.x + knob.turn("left") * 6, 0, gfx.W - paddle.w)

  if ball.stuck then
    ball.x, ball.y = paddle.x + paddle.w // 2 - 2, paddle.y - 5
    if knob.pressed("right") then ball.stuck, ball.vx, ball.vy = false, 2, -3 end
    return
  end

  ball.x = ball.x + ball.vx
  if ball.x < 0 or ball.x > gfx.W - ball.w then ball.vx = -ball.vx end
  ball.y = ball.y + ball.vy
  if ball.y < 12 then ball.vy = -ball.vy end

  -- Bricks: map_hit returns the first non-empty tile under a rectangle.
  local tile, tx, ty = map_hit(1, ball.x, ball.y, ball.w, ball.h)
  if tile then
    map_set(1, tx, ty, ".")
    ball.vy = -ball.vy
    score = score + 10
  end

  if hit(ball, paddle) and ball.vy > 0 then
    ball.vy = -ball.vy
    ball.vx = clamp((ball.x + 2 - (paddle.x + paddle.w / 2)) / 6, -3, 3)
  end

  if ball.y > gfx.H then
    lives = lives - 1
    if lives == 0 then init() else reset_ball() end
  end
end

function draw()
  gfx.clear(1)
  gfx.map(1)
  gfx.sprite("paddle", paddle.x, paddle.y)
  gfx.sprite("ball", ball.x, ball.y)
  gfx.text("SC " .. string.format("%06d", score), 6, 2, 5)
  for i = 1, lives do gfx.sprite("heart", gfx.W - 9 * i, 2) end
  if ball.stuck then gfx.text("Click right to launch", 100, 120, 12) end
end
