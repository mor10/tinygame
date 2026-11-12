local PLAYER_SIZE = 8

local world_width = 152
local world_height = 86
local player_x = 72
local hazard_x = 72
local hazard_y = -8
local score = 0
local game_over = false

local function reset(width, height)
  world_width = width
  world_height = height
  player_x = (world_width - PLAYER_SIZE) / 2
  hazard_x = player_x
  hazard_y = -8
  score = 0
  game_over = false
end

function update(dt, input)
  if input.screen.width ~= world_width or input.screen.height ~= world_height then
    reset(input.screen.width, input.screen.height)
  end
  local player_y = world_height - PLAYER_SIZE - 2

  if game_over then
    if input.click then reset(world_width, world_height) end
  else
    player_x = math.max(0, math.min(world_width - PLAYER_SIZE,
                                    player_x + input.main.delta * 0.5))
    hazard_y = hazard_y + 30 * dt
    if hazard_y > world_height then
      hazard_y = -PLAYER_SIZE
      hazard_x = math.random(0, world_width - PLAYER_SIZE)
      score = score + 1
    end
    if player_x < hazard_x + PLAYER_SIZE and player_x + PLAYER_SIZE > hazard_x and
       player_y < hazard_y + PLAYER_SIZE and player_y + PLAYER_SIZE > hazard_y then
      game_over = true
    end
  end

  tg.rect(0, 0, world_width, world_height, 1)
  tg.rect(player_x, player_y, PLAYER_SIZE, PLAYER_SIZE, 2)
  tg.rect(hazard_x, hazard_y, PLAYER_SIZE, PLAYER_SIZE, 3)
  tg.text(2, 2, "SCORE " .. tostring(score), 4)
  if game_over then
    tg.text((world_width - 72) / 2, world_height / 2 - 8, "GAME OVER", 5)
    tg.text((world_width - 112) / 2, world_height / 2 + 1, "CLICK TO RESET", 5)
  end
end
