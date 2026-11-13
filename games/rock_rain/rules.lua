local BRICK_LIMIT = 32
local BOLT_LIMIT = 8
local CAPSULE_LIMIT = 3
local STARTING_LIVES = 3
local STARTING_AMMO = 5
local MAX_LIVES = 5
local MAX_AMMO = 20
local ROWS_PER_WAVE = 6
local SMALL_BRICK_WIDTH = 16
local SMALL_BRICK_HEIGHT = 8
local BIG_BRICK_WIDTH = 30
local BIG_BRICK_HEIGHT = 12

local world_width = 152
local world_height = 86
local scale = 0.475
local hud_height = 14 * scale
local robot_width = 14 * scale
local robot_height = 16 * scale
local robot_x = 0
local robot_y = 0
local bricks = {}
local bolts = {}
local capsules = {}
for i = 1, BRICK_LIMIT do
  bricks[i] = {active = false, big = false, hits = 0, x = 0, y = 0,
               w = 0, h = 0, speed = 0}
end
for i = 1, BOLT_LIMIT do
  bolts[i] = {active = false, x = 0, y = 0}
end
for i = 1, CAPSULE_LIMIT do
  capsules[i] = {active = false, kind = 1, x = 0, y = 0}
end
local boss = {active = false, hits = 0, x = 0, y = 0, vx = 0, drop_timer = 0}
local lives = STARTING_LIVES
local ammo = STARTING_AMMO
local score = 0
local elapsed = 0
local survive_timer = 0
local fire_cooldown = 0
local rapid_seconds = 0
local invulnerable_seconds = 0
local spawn_timer = 1
local capsule_timer = 4
local boss_timer = 20
local capsules_since_life = 0
local wave_rows_left = ROWS_PER_WAVE
local wave_timer = 0.5
local waves_done = 0
local game_over = false
local steer_speed = 0

local function overlaps(ax, ay, aw, ah, bx, by, bw, bh)
  return ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah
end

local function reset(width, height)
  world_width = width
  world_height = height
  scale = math.min(world_width / 320, world_height / 172)
  hud_height = 14 * scale
  robot_width = 14 * scale
  robot_height = 16 * scale
  robot_x = (world_width - robot_width) / 2
  robot_y = world_height - robot_height - 2 * scale
  lives = STARTING_LIVES
  ammo = STARTING_AMMO
  score = 0
  elapsed = 0
  survive_timer = 0
  fire_cooldown = 0
  rapid_seconds = 0
  invulnerable_seconds = 0
  spawn_timer = 1
  capsule_timer = 4
  boss_timer = math.random(15, 25)
  capsules_since_life = 0
  wave_rows_left = ROWS_PER_WAVE
  wave_timer = 0.5
  waves_done = 0
  game_over = false
  steer_speed = 0
  boss.active = false
  for i = 1, BRICK_LIMIT do bricks[i].active = false end
  for i = 1, BOLT_LIMIT do bolts[i].active = false end
  for i = 1, CAPSULE_LIMIT do capsules[i].active = false end
end

local function fall_speed()
  return math.min(45 + elapsed * 1.5, 140) * scale
end

local function wave_speed()
  return math.min(55 + waves_done * 10, 110) * scale
end

local function spawn_brick_at(centre_x, top, big, speed, width)
  for i = 1, BRICK_LIMIT do
    local brick = bricks[i]
    if not brick.active then
      brick.active = true
      brick.big = big
      brick.hits = big and 2 or 1
      brick.w = width or (big and BIG_BRICK_WIDTH or SMALL_BRICK_WIDTH) * scale
      brick.h = (big and BIG_BRICK_HEIGHT or SMALL_BRICK_HEIGHT) * scale
      brick.x = math.max(0, math.min(world_width - brick.w, centre_x - brick.w / 2))
      brick.y = top
      brick.speed = speed or fall_speed() * (0.85 + math.random() * 0.3)
      return true
    end
  end
  return false
end

local function start_wave(first_row_delay)
  wave_rows_left = ROWS_PER_WAVE
  wave_timer = first_row_delay
end

local function spawn_wave_row()
  local slots = math.floor(world_width / (SMALL_BRICK_WIDTH * scale) + 0.5)
  local first_start = 0
  local first_width = 0
  local second_start = slots
  local second_width = 0
  if math.random(1, 100) <= 50 then
    local half = math.floor(slots / 2)
    first_width = 2
    second_width = 2
    first_start = math.random(0, half - first_width)
    second_start = half + 1 + math.random(0, half - second_width - 1)
  else
    first_width = 3
    first_start = math.random(0, slots - first_width)
  end

  local run_start = -1
  for slot = 0, slots do
    local open = slot == slots or
      (slot >= first_start and slot < first_start + first_width) or
      (slot >= second_start and slot < second_start + second_width)
    if not open and run_start < 0 then
      run_start = slot
    elseif open and run_start >= 0 then
      spawn_brick_at((run_start + slot) * SMALL_BRICK_WIDTH * scale / 2,
                     hud_height - SMALL_BRICK_HEIGHT * scale, false, wave_speed(),
                     (slot - run_start) * SMALL_BRICK_WIDTH * scale)
      run_start = -1
    end
  end
end

local function spawn_brick()
  local big = math.random(1, 100) <= 30
  local width = (big and BIG_BRICK_WIDTH or SMALL_BRICK_WIDTH) * scale
  spawn_brick_at(width / 2 + math.random() * (world_width - width),
                 hud_height - (big and BIG_BRICK_HEIGHT or SMALL_BRICK_HEIGHT) * scale,
                 big, nil)
end

local function spawn_capsule()
  for i = 1, CAPSULE_LIMIT do
    local capsule = capsules[i]
    if not capsule.active then
      local roll = math.random(1, 100)
      if boss.active then
        capsule.kind = roll <= 50 and 2 or 1
      else
        if capsules_since_life >= 6 or roll <= 12 then
          capsule.kind = 3
          capsules_since_life = 0
        elseif roll <= 44 then
          capsule.kind = 2
          capsules_since_life = capsules_since_life + 1
        else
          capsule.kind = 1
          capsules_since_life = capsules_since_life + 1
        end
      end
      capsule.active = true
      capsule.x = math.random() * (world_width - 12 * scale)
      capsule.y = hud_height - 12 * scale
      return
    end
  end
end

local function can_shoot()
  return not game_over and (rapid_seconds > 0 or ammo > 0)
end

local function fire()
  if not can_shoot() or fire_cooldown > 0 then return end
  for i = 1, BOLT_LIMIT do
    local bolt = bolts[i]
    if not bolt.active then
      bolt.active = true
      bolt.x = robot_x + (robot_width - 2 * scale) / 2
      bolt.y = robot_y - 6 * scale
      if rapid_seconds > 0 then
        fire_cooldown = 0.08
      else
        ammo = ammo - 1
        fire_cooldown = 0.15
      end
      return
    end
  end
end

local function break_thing(brick, points)
  score = score + points
  brick.active = false
end

local function knock_out_piece(brick, hit_x)
  local piece = SMALL_BRICK_WIDTH * scale
  local pieces = math.floor(brick.w / piece + 0.5)
  local index = math.floor((hit_x - brick.x) / piece)
  index = math.max(0, math.min(pieces - 1, index))
  local hit_x1 = brick.x + index * piece
  local right_width = (pieces - index - 1) * piece
  local left_width = index * piece
  local right = nil
  if left_width > 0 and right_width > 0 then
    for i = 1, BRICK_LIMIT do
      if not bricks[i].active then
        right = bricks[i]
        break
      end
    end
    if right == nil then return false end
  end
  if right ~= nil then
    right.active = true
    right.big = false
    right.hits = 1
    right.x = hit_x1 + piece
    right.y = brick.y
    right.w = right_width
    right.h = brick.h
    right.speed = brick.speed
  end
  if left_width > 0 then
    brick.w = left_width
  elseif right_width > 0 then
    brick.x = hit_x1 + piece
    brick.w = right_width
  else
    brick.active = false
  end
  score = score + 50
  return true
end

local function step_bolts(dt)
  for i = 1, BOLT_LIMIT do
    local bolt = bolts[i]
    if bolt.active then
      bolt.y = bolt.y - 240 * scale * dt
      if bolt.y + 6 * scale < hud_height then
        bolt.active = false
      else
        for j = 1, BRICK_LIMIT do
          local brick = bricks[j]
          if brick.active and overlaps(bolt.x, bolt.y, 2 * scale, 6 * scale,
                                       brick.x, brick.y, brick.w, brick.h) then
            bolt.active = false
            if not brick.big and brick.w > SMALL_BRICK_WIDTH * scale then
              knock_out_piece(brick, bolt.x + scale)
            else
              brick.hits = brick.hits - 1
              if brick.hits <= 0 then
                break_thing(brick, brick.big and 150 or 50)
              end
            end
            break
          end
        end
        if bolt.active and boss.active and
           overlaps(bolt.x, bolt.y, 2 * scale, 6 * scale,
                    boss.x, boss.y, 32 * scale, 32 * scale) then
          bolt.active = false
          boss.hits = boss.hits - 1
          if boss.hits <= 0 then
            boss.active = false
            score = score + 500
            start_wave(1.5)
            spawn_timer = 1.5
          end
        end
      end
    end
  end
end

local function hurt_robot()
  lives = lives - 1
  invulnerable_seconds = 1.5
  if lives <= 0 then
    lives = 0
    game_over = true
  end
end

local function step_bricks(dt)
  local robot_hit_x = robot_x + 2 * scale
  local robot_hit_y = robot_y + 2 * scale
  local robot_hit_w = robot_width - 4 * scale
  local robot_hit_h = robot_height - 2 * scale
  for i = 1, BRICK_LIMIT do
    local brick = bricks[i]
    if brick.active then
      brick.y = brick.y + brick.speed * dt
      if brick.y > world_height then
        brick.active = false
      elseif invulnerable_seconds <= 0 and
             overlaps(brick.x, brick.y, brick.w, brick.h,
                      robot_hit_x, robot_hit_y, robot_hit_w, robot_hit_h) then
        brick.active = false
        hurt_robot()
        if game_over then return end
      end
    end
  end
end

local function step_capsules(dt)
  for i = 1, CAPSULE_LIMIT do
    local capsule = capsules[i]
    if capsule.active then
      capsule.y = capsule.y + 50 * scale * dt
      if capsule.y > world_height then
        capsule.active = false
      elseif overlaps(capsule.x, capsule.y, 12 * scale, 12 * scale,
                      robot_x + 2 * scale, robot_y + 2 * scale,
                      robot_width - 4 * scale, robot_height - 2 * scale) then
        capsule.active = false
        if capsule.kind == 3 then
          if lives < MAX_LIVES then lives = lives + 1 end
        elseif capsule.kind == 2 then
          rapid_seconds = 5
        else
          ammo = math.min(MAX_AMMO, ammo + 5)
        end
      end
    end
  end
end

local function spawn_boss()
  boss.active = true
  boss.hits = 10
  boss.x = math.random() * (world_width - 32 * scale)
  boss.y = hud_height - 32 * scale
  boss.vx = math.random(0, 1) == 0 and 45 * scale or -45 * scale
  boss.drop_timer = 0.7
  if capsule_timer > 1 then capsule_timer = 1 end
end

local function step_boss(dt)
  if not boss.active then return end
  local hover_y = 64 * scale
  if boss.y < hover_y then
    boss.y = math.min(hover_y, boss.y + 30 * scale * dt)
  else
    boss.x = boss.x + boss.vx * dt
    if boss.x < 0 then
      boss.x = 0
      boss.vx = 45 * scale
    elseif boss.x > world_width - 32 * scale then
      boss.x = world_width - 32 * scale
      boss.vx = -45 * scale
    end
  end
  boss.drop_timer = boss.drop_timer - dt
  if boss.drop_timer <= 0 then
    boss.drop_timer = boss.drop_timer + 0.7
    spawn_brick_at(boss.x + math.random() * 32 * scale,
                   boss.y + 32 * scale, false, nil)
  end
end

local function draw()
  tg.rect(0, 0, world_width, world_height, 1)
  tg.rect(0, 0, world_width, hud_height, 5)
  local status = "S " .. tostring(score) .. " L " .. tostring(lives) ..
                 " A " .. tostring(ammo)
  if rapid_seconds > 0 then status = status .. " R" end
  tg.text(scale, scale, status, 7)
  if boss.active then
    tg.text(world_width - 40 * scale, scale, "B " .. tostring(boss.hits), 10)
  end

  if boss.active then
    tg.rect(boss.x, boss.y, 32 * scale, 32 * scale, 2)
    tg.rect(boss.x + 7 * scale, boss.y + 9 * scale,
            5 * scale, 3 * scale, 7)
    tg.rect(boss.x + 20 * scale, boss.y + 9 * scale,
            5 * scale, 3 * scale, 7)
    tg.rect(boss.x + 9 * scale, boss.y + 21 * scale,
            14 * scale, 3 * scale, 8)
  end

  for i = 1, BRICK_LIMIT do
    local brick = bricks[i]
    if brick.active then
      local color = brick.big and (brick.hits == 1 and 15 or 8) or 9
      tg.rect(brick.x, brick.y, brick.w, brick.h, color)
    end
  end

  for i = 1, CAPSULE_LIMIT do
    local capsule = capsules[i]
    if capsule.active then
      local color = capsule.kind == 3 and 11 or (capsule.kind == 2 and 14 or 12)
      local glyph = capsule.kind == 3 and "+" or (capsule.kind == 2 and "R" or "A")
      tg.rect(capsule.x, capsule.y, 12 * scale, 12 * scale, color)
      tg.text(capsule.x + 2 * scale, capsule.y + 2 * scale, glyph, 7)
    end
  end

  for i = 1, BOLT_LIMIT do
    local bolt = bolts[i]
    if bolt.active then tg.rect(bolt.x, bolt.y, 2 * scale, 6 * scale, 12) end
  end

  if invulnerable_seconds <= 0 or math.floor(elapsed * 12) % 2 == 0 then
    tg.rect(robot_x + 2 * scale, robot_y + 5 * scale,
            10 * scale, 11 * scale, 11)
    tg.rect(robot_x + 4 * scale, robot_y,
            6 * scale, 7 * scale, 13)
    tg.rect(robot_x + 6 * scale, robot_y - 3 * scale,
            2 * scale, 4 * scale, 10)
  end

  if game_over then
    tg.rect(world_width * 0.12, world_height * 0.35,
            world_width * 0.76, world_height * 0.3, 5)
    tg.text(world_width * 0.27, world_height * 0.4, "GAME OVER", 8)
    tg.text(world_width * 0.2, world_height * 0.53, "CLICK TO PLAY", 7)
  end
end

local function update_game(dt, input)
  local smoothing = dt > 0 and math.min(dt / 0.06, 1) or 0
  local transitions_per_second = dt > 0 and math.abs(input.main.delta) / dt or 0
  steer_speed = steer_speed + (transitions_per_second - steer_speed) * smoothing
  local fast_turn = math.max(0, math.min(1, (steer_speed - 20) / 140))
  local pixels_per_transition = 2.5 + fast_turn * 5.5
  robot_x = math.max(0, math.min(world_width - robot_width,
                                 robot_x + input.main.delta * pixels_per_transition * scale))
  elapsed = elapsed + dt
  survive_timer = survive_timer + dt
  if survive_timer >= 1 then
    survive_timer = survive_timer - 1
    score = score + 10
  end
  fire_cooldown = math.max(0, fire_cooldown - dt)
  rapid_seconds = math.max(0, rapid_seconds - dt)
  invulnerable_seconds = math.max(0, invulnerable_seconds - dt)
  if input.pressed then fire() end

  if wave_rows_left > 0 then
    wave_timer = wave_timer - dt
    if wave_timer <= 0 then
      spawn_wave_row()
      wave_rows_left = wave_rows_left - 1
      wave_timer = wave_timer + 85 * scale / wave_speed()
      if wave_rows_left == 0 then
        waves_done = waves_done + 1
        spawn_timer = 1.5
      end
    end
  else
    spawn_timer = spawn_timer - dt
    if spawn_timer <= 0 then
      spawn_brick()
      spawn_timer = math.max(0.3, 1.1 - elapsed * 0.01)
    end
  end

  capsule_timer = capsule_timer - dt
  if capsule_timer <= 0 then
    spawn_capsule()
    capsule_timer = boss.active and math.random(2, 4) or math.random(6, 10)
  end

  if not boss.active and wave_rows_left == 0 then
    boss_timer = boss_timer - dt
    if boss_timer <= 0 then
      spawn_boss()
      boss_timer = math.random(20, 40)
    end
  end

  step_bolts(dt)
  step_bricks(dt)
  if game_over then return end
  step_capsules(dt)
  step_boss(dt)
end

function update(dt, input)
  if input.screen.width ~= world_width or input.screen.height ~= world_height then
    reset(input.screen.width, input.screen.height)
  end
  if game_over then
    if input.click then reset(world_width, world_height) end
    draw()
    return
  end
  update_game(dt, input)
  draw()
end

reset(world_width, world_height)
