
-- Autoplay patch for the Phase 4 done test. Append to carts/brick.lua and upload:
--   cat carts/brick.lua carts/tests/brick_autoplay_patch.lua > /tmp/brick_auto.lua
-- The paddle follows the ball, launches are automatic, and the score is logged every 10 s.
knob = {
  turn = function(side)
    if side ~= "left" or ball.stuck then return 0 end
    local d = ball.x + 2 - (paddle.x + paddle.w / 2)
    if d > 4 then return 1 elseif d < -4 then return -1 else return 0 end
  end,
  pressed = function(side) return side == "right" end,
}
local game_update = update
function update()
  game_update()
  if sys.frame() % 300 == 0 then sys.log("autoplay score " .. score .. " lives " .. lives) end
end
