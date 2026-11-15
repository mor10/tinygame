-- Brick Breaker. Right knob moves the paddle (turn fast to dash, slowly to aim).
-- Left click launches the ball and fires the blaster when you have it.
cart = { title = "Brick Breaker", tagline = "Bounce, blast and find the pig", api = 1, fps = 30,
  icon = [[
    ................
    777.999.888.aaa.
    777.999.888.aaa.
    ................
    777.999.888.aaa.
    ................
    ................
    ................
    ................
    ................
    ......44........
    ......44........
    ................
    ................
    ....88888888....
    ................
  ]] }

sprites = {
  pig = [[
    ........mmmmmmmm........
    ......mmrssssssrmm......
    .....mmsss0ss0sssmm.....
    .....mmssrrrrrrssmm.....
    .....mmssr0rr0rssmm.....
    ......mmssssssssmm......
    ....3333333333333333....
    .4444444444444444444444.
    .3338333383333833338333.
    ..22222222222222222222..
    .....2222......2222.....
  ]],
}

-- Levels: . empty, 1 2 3 = bricks that need that many hits, # steel (never breaks).
local LEVELS = {
  { "............", "111111111111", "111111111111", "111111111111", "111111111111" },
  { "............", ".....11.....", "....1111....", "...111111...", "..11111111..", ".1111111111.", "111111111111" },
  { "............", "1.1.1.1.1.1.", ".1.1.1.1.1.1", "1.1.1.1.1.1.", ".1.1.1.1.1.1", "111111111111" },
  { "............", "222222222222", "111111111111", "111111111111", "222222222222" },
  { "............", ".1111111111.", ".1222222221.", ".1222222221.", ".1111111111." },
  { "............", "..22222222..", "..2......2..", "..2.1111.2..", "..222..222.." },
  { "............", "#..........#", "#.22222222.#", "#.21111112.#", "#.21111112.#", "#.22222222.#" },
  { "............", "11#......#11", "22#.1111.#22", "22#.1111.#22", "33#......#33", "33##....##33" },
  { "111111111111", "............", "##########..", "222222222222", "222222222222" },
  { "#..........#", "#.33333333.#", "#.3......3.#", "#.3.1111.3.#", "#.3......3.#", "#.333..333.#" },
}

local BW, BH, COLS, X0 = 26, 12, 12, 4     -- brick size, columns, left margin
local TOP = 8                              -- the ceiling
local BY = TOP + 16                        -- top of the brick area
local PY, PH, PW = 148, 8, 44              -- paddle row, height, normal width
local ROWCOL = { 16, 17, 18, 20, 23, 27 }   -- one colour per row of 1-hit bricks
local NAMES = { "First Bounce", "The Pyramid", "Zigzag", "Armour Up", "Side Lanes", "The Vault",
                "Steel Pillars", "Side Rooms", "Steel Roof", "The Fortress" }
local KINDS = { { "W", 26, 21 }, { "B", 22, 17 }, { "M", 20, 20 }, { "2", 22, 27 }, { "+", 10, 16 } }
local W, H = 320, 172
local fl, abs = math.floor, math.abs

local grid, bricks, level, lives, score, best, state, timer, intro
local px, pw, target, last_pos, vel, mv, act, mv_was
local balls, caps, bolts, parts
local stars
local newbest = false
local widet, gunt, bigt, cool, streak, pig, pig_used, moved

local function ball_speed() return math.min(3.0 + 0.18 * (level - 1), 5.0) end

local function load_level(n)
  local def = LEVELS[(n - 1) % #LEVELS + 1]
  local lap = (n - 1) // #LEVELS
  grid, bricks = {}, 0
  for r = 1, #def do
    grid[r] = {}
    local row = def[r] .. string.rep(".", COLS)
    for c = 1, COLS do
      local ch, v = row:sub(c, c), 0
      if ch == "#" then v = -1
      elseif ch >= "1" and ch <= "3" then
        v = tonumber(ch)
        if lap > 0 and (r + c) % 2 == 0 then v = math.min(3, v + 1) end
        bricks = bricks + 1
      end
      grid[r][c] = v
    end
  end
end

local function at(c, r)
  local row = grid[r + 1]
  return row and row[c + 1] or 0
end

-- first brick touching a box (pixels): returns row, column, value
local function box_hit(x0, y0, x1, y1)
  for r = fl((y0 - BY) / BH), fl((y1 - 0.01 - BY) / BH) do
    for c = fl((x0 - X0) / BW), fl((x1 - 0.01 - X0) / BW) do
      local v = at(c, r)
      if v ~= 0 then return r, c, v end
    end
  end
end

local function set_pw(n)
  px = px - (n - pw) / 2
  target = target - (n - pw) / 2
  pw = n
  px = clamp(px, 0, W - pw)
  target = clamp(target, 0, W - pw)
end

local function new_ball()
  balls:add({ x = px + pw / 2, y = PY - 3, vx = 0, vy = 0, sz = 6, sp = ball_speed(), stuck = true, off = 0 })
end

local function spark(x, y, col)
  for i = 1, 5 do
    parts:add({ x = x, y = y, vx = rnd(4) - 2, vy = rnd(3) - 2.5, t = 16, c = col })
  end
end

local function drop(x, y)
  local total, pick = 0, rnd(100)
  for _, k in ipairs(KINDS) do
    total = total + k[2]
    if pick < total then caps:add({ x = x, y = y, k = k[1], c = k[3] }); return end
  end
  caps:add({ x = x, y = y, k = "W", c = 21 })
end

local function kill(r, c, nodrop)
  grid[r + 1][c + 1] = 0
  bricks = bricks - 1
  score = score + 10
  streak = streak + 1
  local x, y = X0 + c * BW + BW / 2, BY + r * BH + BH / 2
  spark(x, y, ROWCOL[r % #ROWCOL + 1])
  if not nodrop and rnd(100) < 14 then drop(x, y) end
end

local function hurt(r, c)
  local v = grid[r + 1][c + 1]
  if v < 0 then return end
  if v <= 1 then kill(r, c) else grid[r + 1][c + 1] = v - 1; score = score + 5 end
end

local function normalise(b)
  local m = 0.3 * b.sp
  if abs(b.vy) < m then b.vy = b.vy < 0 and -m or m end
  local n = 0.12 * b.sp   -- never go dead straight up: a cleared column would trap the ball
  if abs(b.vx) < n then b.vx = b.vx < 0 and -n or n end
  local s = math.sqrt(b.vx * b.vx + b.vy * b.vy)
  b.vx, b.vy = b.vx * b.sp / s, b.vy * b.sp / s
end

local function step_ball(b)
  local h = b.sz / 2
  local n = math.ceil(b.sp / 1.8)
  for i = 1, n do
    local free = not box_hit(b.x - h, b.y - h, b.x + h, b.y + h)   -- already inside a brick? pass through
    b.x = b.x + b.vx / n
    if free then
      local r, c = box_hit(b.x - h, b.y - h, b.x + h, b.y + h)
      if r then b.x = b.x - b.vx / n; b.vx = -b.vx; hurt(r, c) end
    end
    if b.x - h < 0 then b.x = h; b.vx = abs(b.vx) end
    if b.x + h > W then b.x = W - h; b.vx = -abs(b.vx) end
    b.y = b.y + b.vy / n
    if free then
      local r, c = box_hit(b.x - h, b.y - h, b.x + h, b.y + h)
      if r then b.y = b.y - b.vy / n; b.vy = -b.vy; hurt(r, c) end
    end
    if b.y - h < TOP then b.y = TOP + h; b.vy = abs(b.vy) end
    local bottom = b.y + h
    if b.vy > 0 and bottom >= PY and bottom - b.vy / n <= PY + 1 and b.x > px - h and b.x < px + pw + h then
      local a = clamp((b.x - (px + pw / 2)) / (pw / 2 + h), -1, 1) * 1.05 + (rnd(0.12) - 0.06)
      b.vx, b.vy = b.sp * math.sin(a), -b.sp * math.cos(a)
      b.y = PY - h
      streak = 0
    end
  end
  normalise(b)
  b.tr = b.tr or {}
  table.insert(b.tr, 1, { b.x, b.y })
  if #b.tr > 4 then table.remove(b.tr) end
end

local function multiply()
  local fresh = {}
  balls:each(function(b)
    if not b.stuck then fresh[#fresh + 1] = b end
  end)
  for _, b in ipairs(fresh) do
    for _, a in ipairs({ -0.45, 0.45 }) do
      if balls:count() < 8 then
        local s, c = math.sin(a), math.cos(a)
        balls:add({ x = b.x, y = b.y, vx = b.vx * c - b.vy * s, vy = b.vx * s + b.vy * c, sz = b.sz, sp = b.sp })
      end
    end
  end
end

local function collect(k)
  score = score + 25
  if k == "+" then
    if lives < 5 then lives = lives + 1 else score = score + 200 end
  elseif k == "W" then widet = 360; set_pw(PW * 2)
  elseif k == "B" then gunt = 300
  elseif k == "M" then multiply()
  elseif k == "2" then bigt = 360 end
end

local function pig_start()
  local d = choose({ 1, -1 })
  pig = { d = d, x = d > 0 and -30 or W + 30, y = rnd_int(76, 92), tx = W / 2 + rnd_int(-50, 50),
          ph = "in", t = 0, n = rnd_int(3, 7), say = nil, zap = nil }
  pig.by = pig.y
  pig_used = true
end

local function pig_zap(p)
  local list = {}
  for r = 0, #grid - 1 do
    for c = 0, COLS - 1 do
      if at(c, r) > 0 then list[#list + 1] = { r, c } end
    end
  end
  if #list == 0 then return end
  local t = choose(list)
  kill(t[1], t[2], true)
  p.zap = { x = X0 + t[2] * BW + BW / 2, y = BY + t[1] * BH + BH / 2, t = 7 }
end

local function pig_update()
  local p = pig
  p.t = p.t + 1
  p.by = p.y + math.sin(p.t * 0.2) * 3
  if p.zap then p.zap.t = p.zap.t - 1; if p.zap.t <= 0 then p.zap = nil end end
  if p.ph == "in" then
    p.x = p.x + p.d * 3
    if (p.x - p.tx) * p.d >= 0 then p.ph, p.t, p.say = "hi", 0, "hi!" end
  elseif p.ph == "hi" then
    if p.t >= 34 then p.ph, p.t, p.say = "blast", 0, nil end
  elseif p.ph == "blast" then
    p.x = p.x + p.d * 0.4
    if p.t % 12 == 1 and p.t <= 12 * p.n then pig_zap(p) end
    if p.t >= 12 * p.n + 8 then p.ph, p.t, p.say = "bye", 0, "bye!" end
  elseif p.ph == "bye" then
    if p.t >= 34 then p.ph, p.t, p.say = "out", 0, nil end
  else
    p.x = p.x + p.d * 3
    if p.x < -30 or p.x > W + 30 then pig = nil end
  end
end

local function reset_round()
  balls, caps, bolts = group(), group(), group()
  widet, gunt, bigt, streak, cool = 0, 0, 0, 0, 0
  set_pw(PW)
  new_ball()
end

local function start_level(n)
  level = n
  load_level(n)
  pig, pig_used, intro = nil, false, 70
  reset_round()
end

local function new_game()
  lives, score, state, timer = 3, 0, "play", 0
  start_level(1)
end

function init()
  px, pw, target, vel = W / 2 - PW / 2, PW, W / 2 - PW / 2, 0
  mv, act = ui.sides()
  mv_was = mv
  last_pos = knob.pos(mv)
  best = save.get("best", 0)
  parts = group()
  moved = false
  stars = {}
  for i = 1, 36 do stars[i] = { rnd_int(4, W - 4), rnd_int(TOP + 4, H - 4), rnd_int(0, 30) } end
  new_game()
end

local function confetti()
  for i = 1, 70 do
    parts:add({ x = rnd(W), y = TOP + rnd(20), vx = rnd(2) - 1, vy = rnd(2), t = 70, c = choose({ 16, 17, 18, 20, 21, 27, 25, 4 }) })
  end
end

local function lose_ball()
  lives = lives - 1
  if lives <= 0 then
    state, timer = "over", 0
    newbest = score > best
    if newbest then best = score; save.set("best", best); confetti() end
  else
    reset_round()
  end
end

function update()
  -- The paddle: the knob moves a target and the paddle glides after it. How far a step moves it
  -- depends on how fast you are turning: slow turns are fine steps, a fast spin dashes. At a
  -- steady half turn per second the paddle crosses the whole screen.
  mv, act = ui.sides()   -- right-handed: right knob moves, left clicks; left-handed: the other way round
  local p = knob.pos(mv)
  if mv ~= mv_was then mv_was = mv; last_pos = p end
  local d = p - last_pos
  last_pos = p
  if d ~= 0 then moved = true end
  vel = vel * 0.8 + abs(d) * 30 * 0.2                       -- steps per second, smoothed
  local gain = clamp(0.2 + vel / 50, 0.3, 2.0)
  target = clamp(target + d * (W - PW) / 40 * gain, 0, W - pw)
  px = px + clamp((target - px) * 0.3, -14, 14)
  if abs(target - px) < 0.3 then px = target end

  parts:each(function(q)
    q.x, q.y, q.vy, q.t = q.x + q.vx, q.y + q.vy, q.vy + 0.15, q.t - 1
    if q.t <= 0 then parts:remove(q) end
  end)

  if state == "over" then
    timer = timer + 1
    if timer > 20 and knob.pressed(act) then new_game() end
    return
  elseif state == "clear" then
    timer = timer + 1
    if timer > 90 then state = "play"; start_level(level + 1) end
    return
  end
  if intro > 0 then intro = intro - 1 end

  local fired = false
  if knob.pressed(act) then
    balls:each(function(b)
      if b.stuck then
        b.stuck, fired, intro = false, true, 0
        local a = clamp(b.off / (pw / 2), -1, 1) * 0.5 + (rnd(0.3) - 0.15)
        b.vx, b.vy = b.sp * math.sin(a), -b.sp * math.cos(a)
      end
    end)
  end
  if cool > 0 then cool = cool - 1 end
  if gunt > 0 and not fired and knob.held(act) and cool == 0 then
    cool = 8
    bolts:add({ x = px + 5, y = PY - 10 })
    bolts:add({ x = px + pw - 8, y = PY - 10 })
  end

  bolts:each(function(b)
    b.y = b.y - 8
    local r, c = box_hit(b.x, b.y, b.x + 3, b.y + 9)
    if r then hurt(r, c); bolts:remove(b)
    elseif b.y < TOP then bolts:remove(b) end
  end)

  balls:each(function(b)
    if b.stuck then
      b.sz = 6
      b.x = clamp(px + pw / 2 + b.off, 3, W - 3)
      b.y = PY - 3
    else
      b.sz = bigt > 0 and 12 or 6
      step_ball(b)
      if b.y - b.sz / 2 > H then balls:remove(b) end
    end
  end)

  caps:each(function(q)
    q.y = q.y + 1.8
    if q.y > PY - 6 and q.y < PY + PH + 6 and q.x > px - 8 and q.x < px + pw + 8 then
      collect(q.k); caps:remove(q)
    elseif q.y > H then caps:remove(q) end
  end)

  if widet > 0 then widet = widet - 1; if widet == 0 then set_pw(PW) end end
  if gunt > 0 then gunt = gunt - 1 end
  if bigt > 0 then bigt = bigt - 1 end

  if streak >= 10 and not pig and not pig_used then pig_start(); streak = 0 end
  if pig then pig_update() end

  if balls:count() == 0 then lose_ball()
  elseif bricks <= 0 then state, timer, pig = "clear", 0, nil; confetti() end
end

local function draw_brick(r, c, v)
  local x, y = X0 + c * BW, BY + r * BH
  if v < 0 then   -- steel: chamfered, riveted, clearly bolted in place
    ui.plate(x + 1, y + 1, BW - 2, BH - 2, 2, 3)
    ui.plate(x + 2, y + 2, BW - 4, BH - 4, 3, 2)
    gfx.fill_rect(x + 5, y + 2, BW - 10, 1, 4)
    for _, o in ipairs({ { 4, 4 }, { BW - 7, 4 }, { 4, BH - 7 }, { BW - 7, BH - 7 } }) do
      gfx.fill_rect(x + o[1], y + o[2], 3, 3, 2)
      gfx.pixel(x + o[1], y + o[2], 4)
    end
  else
    local col = v == 1 and ROWCOL[r % #ROWCOL + 1] or (v == 2 and 21 or 25)
    gfx.fill_rect(x + 1, y + 1, BW - 2, BH - 2, col)
  end
end

-- only what you need mid-game sits on the board: the score and the lives
local function draw_hud()
  gfx.label("" .. score, ui.PAD, 6, 4, 2)
  for i = 1, lives do ui.heart(W - ui.PAD - i * 14 + 3, 8, 16) end
end

local function draw_pig()
  local p = pig
  if p.zap then
    gfx.line(fl(p.x), fl(p.by + 12), p.zap.x, p.zap.y, 18)
    gfx.line(fl(p.x) + 1, fl(p.by + 12), p.zap.x + 1, p.zap.y, 18)
    gfx.fill_circle(p.zap.x, p.zap.y, 6, 4)
  end
  gfx.sprite("pig", fl(p.x - 24), fl(p.by - 14), p.d < 0, false, 2)
  if p.say then
    local tw = gfx.label_width(p.say, 3)
    local bx, by = fl(clamp(p.x - tw / 2 - 8, 4, W - tw - 20)), fl(p.by - 50)
    gfx.fill_rect(bx, by, tw + 16, 28, 4)
    gfx.fill_rect(fl(p.x) - 3, by + 28, 7, 6, 4)
    gfx.label(p.say, bx + 8, by + 5, 24, 3)
  end
end

-- shown in the pause screen's sidebar
function stats()
  local list = { { "Level", level }, { "Score", score }, { "Best", best } }
  for _, t in ipairs({ { "Wide paddle", widet }, { "Blaster", gunt }, { "Big ball", bigt } }) do
    if t[2] > 0 then list[#list + 1] = { t[1], (t[2] + 29) // 30 .. " s" } end
  end
  return list
end

function draw()
  gfx.clear(0)
  for _, st in ipairs(stars) do
    local f = (sys.frame() + st[3]) % 60
    gfx.pixel(st[1], st[2], f < 6 and 22 or (f < 30 and 24 or 1))
  end
  for r = 1, #grid do
    for c = 1, COLS do
      local v = grid[r][c]
      if v ~= 0 then draw_brick(r - 1, c - 1, v) end
    end
  end
  caps:each(function(q)
    local x, y = fl(q.x), fl(q.y)
    gfx.fill_rect(x - 8, y - 7, 16, 14, q.c)
    gfx.label(q.k, x - gfx.label_width(q.k, 1) // 2, y - 4, 1, 1)
  end)
  bolts:each(function(b) gfx.fill_rect(fl(b.x), fl(b.y), 3, 9, 18) end)
  balls:each(function(b)
    local h = fl(b.sz / 2)
    if b.tr and not b.stuck then
      for i = 4, 2, -1 do
        local t = b.tr[i]
        if t then gfx.fill_circle(fl(t[1]), fl(t[2]), math.max(1, h - i + 1), i == 2 and 22 or (i == 3 and 23 or 24)) end
      end
    end
    gfx.fill_circle(fl(b.x), fl(b.y), h, 4)
  end)

  local body = gunt > 0 and 17 or 18
  gfx.fill_rect(fl(px), PY, pw, PH, body)
  if gunt > 0 then
    gfx.fill_rect(fl(px) + 3, PY - 5, 5, 5, 18)
    gfx.fill_rect(fl(px) + pw - 8, PY - 5, 5, 5, 18)
  end

  if pig then draw_pig() end
  draw_hud()

  if state == "over" then
    local line = newbest and "New best: " .. score .. "!" or "Score " .. score .. "   Best " .. best
    ui.card("GAME OVER", line, timer > 20 and "Retry" or nil, 16, timer, "click")
  elseif state == "clear" then
    ui.card("LEVEL CLEAR!", "Score " .. score, nil, 20, timer)
  else
    local waiting = false
    balls:each(function(b) if b.stuck then waiting = true end end)
    if waiting then
      if intro > 0 then ui.card("LEVEL " .. level, NAMES[(level - 1) % #NAMES + 1], nil, 25, 70 - intro)
      else
        local bob = fl(math.sin(sys.frame() * 0.2) * 1.5)
        ui.controls(112 + bob, { { "click", act, "Launch" }, { "turn", mv, "Move" } })
      end
    end
  end
  parts:each(function(q) gfx.fill_rect(fl(q.x), fl(q.y), 3, 3, q.c) end)
end
