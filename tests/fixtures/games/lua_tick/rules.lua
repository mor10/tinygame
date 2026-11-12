local ticks = 0

function update(dt, input)
  assert(dt > 0 and dt <= 1)
  assert(input.screen.width == 152 and input.screen.height == 86)
  assert(input.main.delta == 6)
  assert(input.left.delta == -3 and input.left.down)
  assert(input.right.pressed and input.click)
  assert(input.held_ms == 24)
  tg.rect(2, 3, 4, 5, 5)
  tg.text(1, 2, "OK", 2)
  ticks = ticks + 1
  assert(ticks <= 2)
end
