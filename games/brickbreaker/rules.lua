local BRICK_ROWS = 5
local BRICK_COLUMNS = 10
local BRICK_TOP = 12
local BRICK_HEIGHT = 4
local BRICK_GAP = 1
local PADDLE_HEIGHT = 3
local BALL_RADIUS = 1.5
local STEP = 1 / 120

local layouts = {
  { {0x3ff, 0x3ff, 0x3ff, 0x3ff, 0x3ff}, {} },
  { {0x030, 0x078, 0x0fc, 0x1fe, 0x3ff}, {} },
  { {0x155, 0x2aa, 0x155, 0x2aa, 0x155}, {} },
  { {0x3ff, 0x000, 0x3ff, 0x000, 0x3ff}, {0x3ff, 0, 0, 0, 0} },
  { {0x030, 0x0fc, 0x3ff, 0x0fc, 0x030}, {0, 0x030, 0x078, 0x030, 0} },
  { {0x3ff, 0x201, 0x2fd, 0x201, 0x3ff}, {0x3ff, 0x201, 0x201, 0x201, 0x3ff} },
}

local world_width = 152
local world_height = 86
local paddle_x = 0
local paddle_width = 22
local ball_x = 0
local ball_y = 0
local ball_vx = 0
local ball_vy = 0
local bricks = {}
local level = 1
local lives = 3
local score = 0
local launched = false
local game_over = false
local accumulator = 0
local initialized = false

local function paddle_y()
  return world_height - 10 - PADDLE_HEIGHT
end

local function reset_ball()
  ball_x = paddle_x + paddle_width / 2
  ball_y = paddle_y() - BALL_RADIUS - 1
  ball_vx = 0
  ball_vy = 0
  launched = false
  accumulator = 0
end

local function load_level()
  local layout = layouts[(level - 1) % #layouts + 1]
  local present = layout[1]
  local tough = layout[2]
  bricks = {}
  for row = 1, BRICK_ROWS do
    for column = 1, BRICK_COLUMNS do
      local bit = 1 << (column - 1)
      local hits = 0
      if present[row] & bit ~= 0 then
        hits = tough[row] ~= nil and tough[row] & bit ~= 0 and 2 or 1
      end
      bricks[(row - 1) * BRICK_COLUMNS + column] = hits
    end
  end
end

local function reset(width, height)
  world_width = width
  world_height = height
  paddle_width = math.min(22, world_width * 0.2)
  paddle_x = (world_width - paddle_width) / 2
  level = 1
  lives = 3
  score = 0
  game_over = false
  load_level()
  reset_ball()
  initialized = true
end

local function start_game()
  if game_over then
    reset(world_width, world_height)
  elseif not launched then
    local speed = 62 + (level - 1) * 3
    ball_vx = speed * 0.34
    ball_vy = -speed * 0.94
    launched = true
  end
end

local function brick_rect(index)
  local row = math.floor((index - 1) / BRICK_COLUMNS)
  local column = (index - 1) % BRICK_COLUMNS
  local gap = BRICK_GAP
  local side = 3
  local width = (world_width - side * 2 - gap * (BRICK_COLUMNS - 1)) / BRICK_COLUMNS
  return side + column * (width + gap), BRICK_TOP + row * (BRICK_HEIGHT + gap), width
end

local function update_bricks()
  local any = false
  for index = 1, BRICK_ROWS * BRICK_COLUMNS do
    if bricks[index] > 0 then any = true end
  end
  if not any then
    level = level + 1
    load_level()
    reset_ball()
  end
end

local function hit_brick(index, x, y, previous_x, previous_y)
  local brick_x, brick_y, brick_width = brick_rect(index)
  local closest_x = math.max(brick_x, math.min(x, brick_x + brick_width))
  local closest_y = math.max(brick_y, math.min(y, brick_y + BRICK_HEIGHT))
  local dx = x - closest_x
  local dy = y - closest_y
  if dx * dx + dy * dy >= BALL_RADIUS * BALL_RADIUS then return false end

  bricks[index] = bricks[index] - 1
  if bricks[index] == 0 then score = score + 100 end

  if previous_y + BALL_RADIUS <= brick_y or previous_y - BALL_RADIUS >= brick_y + BRICK_HEIGHT then
    ball_vy = -ball_vy
    if previous_y < brick_y then
      ball_y = brick_y - BALL_RADIUS
    else
      ball_y = brick_y + BRICK_HEIGHT + BALL_RADIUS
    end
  else
    ball_vx = -ball_vx
    if previous_x < brick_x then
      ball_x = brick_x - BALL_RADIUS
    else
      ball_x = brick_x + brick_width + BALL_RADIUS
    end
  end
  return true
end

local function lose_ball()
  lives = lives - 1
  if lives <= 0 then
    lives = 0
    game_over = true
  else
    reset_ball()
  end
end

local function step()
  local previous_x = ball_x
  local previous_y = ball_y
  ball_x = ball_x + ball_vx * STEP
  ball_y = ball_y + ball_vy * STEP

  if ball_x < BALL_RADIUS then
    ball_x = BALL_RADIUS
    ball_vx = math.abs(ball_vx)
  elseif ball_x > world_width - BALL_RADIUS then
    ball_x = world_width - BALL_RADIUS
    ball_vx = -math.abs(ball_vx)
  end
  if ball_y < BALL_RADIUS then
    ball_y = BALL_RADIUS
    ball_vy = math.abs(ball_vy)
  end

  for index = 1, BRICK_ROWS * BRICK_COLUMNS do
    if bricks[index] > 0 and hit_brick(index, ball_x, ball_y, previous_x, previous_y) then
      update_bricks()
      break
    end
  end

  local py = paddle_y()
  if ball_vy > 0 and ball_y + BALL_RADIUS >= py and ball_y - BALL_RADIUS <= py + PADDLE_HEIGHT and
     ball_x + BALL_RADIUS >= paddle_x and ball_x - BALL_RADIUS <= paddle_x + paddle_width then
    local offset = math.max(-1, math.min(1,
      (ball_x - (paddle_x + paddle_width / 2)) / (paddle_width / 2)))
    local speed = math.sqrt(ball_vx * ball_vx + ball_vy * ball_vy)
    local angle = offset * 1.0471975512
    ball_vx = speed * math.sin(angle)
    ball_vy = -speed * math.cos(angle)
    ball_y = py - BALL_RADIUS
  end

  if ball_y - BALL_RADIUS > world_height then lose_ball() end
end

local function draw()
  tg.rect(0, 0, world_width, world_height, 1)
  tg.rect(0, 9, world_width, 1, 5)
  tg.text(3, 2, "SCORE " .. tostring(score), 7)
  tg.text(world_width - 48, 2, "L" .. tostring(level) .. "  " .. tostring(lives) .. "L", 7)

  local gap = BRICK_GAP
  local side = 3
  local brick_width = (world_width - side * 2 - gap * (BRICK_COLUMNS - 1)) / BRICK_COLUMNS
  for index = 1, BRICK_ROWS * BRICK_COLUMNS do
    local hits = bricks[index]
    if hits > 0 then
      local x, y = brick_rect(index)
      local color = hits == 2 and 6 or (math.floor((index - 1) / BRICK_COLUMNS) % 4 + 8)
      tg.rect(x, y, brick_width, BRICK_HEIGHT, color)
    end
  end

  tg.rect(paddle_x, paddle_y(), paddle_width, PADDLE_HEIGHT, 12)
  tg.rect(ball_x - BALL_RADIUS, ball_y - BALL_RADIUS, BALL_RADIUS * 2, BALL_RADIUS * 2, 7)

  if game_over then
    tg.text((world_width - 72) / 2, world_height / 2 - 7, "GAME OVER", 10)
    tg.text((world_width - 88) / 2, world_height / 2 + 3, "CLICK TO RESTART", 7)
  elseif not launched then
    tg.text(3, world_height - 10, "CLICK TO LAUNCH", 7)
  end
end

function update(dt, input)
  if not initialized or input.screen.width ~= world_width or input.screen.height ~= world_height then
    reset(input.screen.width, input.screen.height)
  end

  paddle_x = math.max(0, math.min(world_width - paddle_width,
    paddle_x + input.main.delta * (world_width / 120)))
  if not launched then
    ball_x = paddle_x + paddle_width / 2
    ball_y = paddle_y() - BALL_RADIUS - 1
  end
  if input.click then start_game() end

  if launched and not game_over then
    accumulator = accumulator + math.min(dt, 0.1)
    while accumulator >= STEP do
      step()
      accumulator = accumulator - STEP
      if not launched or game_over then break end
    end
  end
  draw()
end
