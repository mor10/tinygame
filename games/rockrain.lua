-- Rock Rain. Right knob flies the ship. Left click fires, right click drops the bomb.
-- Each level: a tunnel to fly through, then rock rain until you've dodged or blasted enough,
-- then an alien boss that needs 15 hits.
cart = { title = "Rock Rain", tagline = "Dodge, blast and beat the alien", api = 1, fps = 30,
  icon = [[
    ....33..........
    ...3333...22....
    ...3333..2222...
    ....33...2222...
    ..........22....
    .....3333.......
    ....333333......
    ....333333......
    .....3333.......
    ................
    ................
    .......4........
    ......444.......
    .....44d44......
    ....4444444.....
    ....7.999.7.....
  ]] }

sprites = {
  ship = [[
    .......i.......
    ......444......
    ......4m4......
    .....44m44.....
    .....44m44.....
    ....4444444....
    ...444444444...
    ..g444444444g..
    ..g444444444g..
    .gg444444444gg.
    .gg.44...44.gg.
    .g...hh.hh...g.
  ]],
  alien = [[
    .....i..........i.....
    .....i..........i.....
    .......kkkkkkkk.......
    .....kkkkkkkkkkkk.....
    ....kkkkkkkkkkkkkk....
    ...kkkkkkkkkkkkkkkk...
    ...kk44kkkkkkkk44kk...
    ...kk40kkkkkkkk04kk...
    ...kkkkkkkkkkkkkkkk...
    ....kkkk4444444kkk....
    .....kkkkkkkkkkkk.....
    ...k.kk..kkkk..kk.k...
    ..k..k..........k..k..
  ]],
}
sprites.alien_hit = (sprites.alien:gsub("k", "4"))

local W, H = 320, 172
local PY, RH = 146, 8                 -- ship top, tunnel row height
local fl, sin, cos, abs = math.floor, math.sin, math.cos, math.abs
local NAMES = { "Cave Run", "Rubble Alley", "Crystal Depths", "Space Station", "Reactor Row",
                "Docking Bay", "Mine Shaft", "The Hangar", "The Gauntlet" }
local CAPS = { a = { 22, "+5" }, u = { 17, "A" }, l = { 16, "" }, b = { 25, "B" } }

local lv, lives, score, best, state, timer, lt, newbest
local prog, quota, ammo, autot, bombs, inv, cool, flash, noammo
local px, target, vel, last_pos, mv, act, mv_was
local rocks, shots, caps, parts, SX, SY, SV, boss
local spawnt, capt
local s, speed, nrows, TL, TR, obs

-- Memory: the console's Lua heap is small and its collector lets short-lived tables pile up, so the
-- things that come and go (rocks, shots, capsules, sparks, messages) are reused, never re-created.
-- An entry with .dead = true is a free slot.
local function take(list, max, make)
  for i = 1, #list do
    if list[i].dead then return list[i] end
  end
  if #list < max then
    local o = make()
    list[#list + 1] = o
    return o
  end
end

local C_SHIP, C_ROCK, C_HURT, C_BOMB, C_ZAP, C_BOSS = { 4, 17, 16, 18 }, { 3, 4, 15, 30 }, { 3, 4, 15 }, { 4, 18, 22 }, { 20, 4, 18 }, { 20, 4, 18, 17 }
local BANNER, TOAST = { "", 0, 4 }, { text = "", c = 18, t = 0 }

local function say(text, c) TOAST.text, TOAST.c, TOAST.t = text, c or 18, 50 end
local function ban(text, t, c) BANNER[1], BANNER[2], BANNER[3] = text, t, c end

local function boom(x, y, n, cols)
  local k = 0
  for i = 1, #parts do
    local q = parts[i]
    if k < n and q[5] <= 0 then
      q[1], q[2], q[3], q[4], q[5], q[6] = x, y, rnd(5) - 2.5, rnd(5) - 2.5, rnd_int(10, 22), choose(cols)
      k = k + 1
    end
  end
end

-- six shared rock shapes (angle and size of three bumps), so every rock stays a small table
local SHAPES = {}
for i = 1, 6 do
  SHAPES[i] = { rnd(6.28), 0.5 + rnd(0.25), rnd(6.28), 0.5 + rnd(0.25), rnd(6.28), 0.5 + rnd(0.25) }
end

local function setrock(o, x, y, r)
  o.x, o.y, o.vx, o.vy, o.r, o.big, o.a, o.spin = x, y, 0, 0, r, r > 9, rnd(6), rnd(0.2) - 0.1
  o.sh, o.cv, o.dead = choose(SHAPES), rnd_int(1, 2), false
  return o
end
local function newrock() return setrock({ x = 0, y = 0, vx = 0, vy = 0, r = 0, big = false, a = 0, spin = 0, sh = false, cv = 1, dead = true }, 0, 0, 5) end
-- a rock from the pool (nil if there are too many), for the rain
local function mkrock(x, y, r)
  local o = take(rocks, 22, newrock)
  if o then setrock(o, x, y, r) end
  return o
end

local function draw_rock(o)
  local x, y, r = fl(o.x), fl(o.y), o.r
  local c, hi = 3, 4
  if o.cv == 2 then c, hi = 15, 30 end
  gfx.fill_circle(x, y, r, c)
  local sh = o.sh
  for i = 1, 5, 2 do
    local a = sh[i] + o.a
    gfx.fill_circle(fl(x + cos(a) * r * 0.6), fl(y + sin(a) * r * 0.6), fl(r * sh[i + 1]), c)
  end
  gfx.fill_circle(x - r // 3, y - r // 3, math.max(1, r // 4), hi)
  gfx.fill_circle(x + r // 4, y + r // 4, math.max(1, r // 4), 2)
end

-- ---------------------------------------------------------------- tunnels
local function style_at(k)
  if lv >= 7 then return (k // 30) % 2 == 0 and "rock" or "station" end
  return lv >= 4 and "station" or "rock"
end

local function rowat(k)
  if k < 1 then k = 1 end
  if k > nrows then return 0, W, "open" end
  return TL[k], TR[k], style_at(k)
end

-- Levels 1-3 are rock caves, 4-6 space stations, 7 and up switch between them every 30 rows.
local function gen_tunnel()
  nrows = math.min(70 + lv * 5, 130)
  speed = math.min(3.0 + lv * 0.2, 4.6)
  local wid = clamp(104 - lv * 8, 64, 104)
  local gap = math.max(6, 12 - lv)
  local cx, wob, flip, since, ph = W / 2, 0, 1, 0, rnd(6)
  obs = {}
  for k = 1, nrows do
    local st = style_at(k)
    local w = wid
    local ramp = clamp((k - 10) / 10, 0, 1)             -- the first rows run straight down the middle
    if st == "rock" then
      wob = clamp(wob + rnd(2.0) - 1.0, -14, 14)
      cx = W / 2 + (sin(k * 0.11 + ph) * 52 + wob) * ramp
      w = wid + sin(k * 0.23) * 12
    elseif k > 12 and k % 10 == 1 then
      cx = cx + rnd_int(-fl(wid * 0.5), fl(wid * 0.5))
    end
    cx = clamp(cx, w / 2 + 24, W - w / 2 - 24)
    local rest = nrows - k
    if rest < 8 then w = w + (8 - rest) * 24 end            -- the mouth opens up at the end
    local l, r = cx - w / 2, cx + w / 2
    if st == "rock" then l, r = l + rnd(4) - 2, r + rnd(4) - 2 end
    TL[k], TR[k] = fl(l), fl(r)

    since = since + 1
    if k >= 14 and since >= gap and rest > 10 then
      since = 0
      local l2, r2 = TL[k], TR[k]
      local done = false
      if st == "station" then
        local gw = math.min(clamp(80 - lv * 3, 48, 80), r2 - l2 - 16)
        local gx = rnd_int(l2 + 8, r2 - 8 - gw)
        obs[#obs + 1] = { k = k, x = l2 - 4, w = gx - l2 + 4, h = 10, t = "gate" }
        obs[#obs + 1] = { k = k, x = gx + gw, w = r2 - gx - gw + 4, h = 10, t = "gate" }
        done = true
      elseif rnd(1) < 0.5 and r2 - l2 > 80 then
        local rr = rnd_int(8, 11)
        local x = rnd_int(l2 + rr + 6, r2 - rr - 6)
        if math.max(x - rr - l2, r2 - x - rr) >= 36 then
          local o = setrock(newrock(), x, 0, rr)
          o.k, o.t, o.spin = k, "rock", 0
          obs[#obs + 1] = o
          done = true
        end
      end
      if not done then
        flip = -flip
        local cw = math.min(rnd_int(22, 34), r2 - l2 - 40)
        obs[#obs + 1] = { k = k, x = flip < 0 and l2 - 4 or r2 - cw, w = cw + 4, h = rnd_int(14, 24), t = "crag" }
      end
    end
  end
end

-- the walls are solid: they stop the ship instead of hurting it
local function wall_block()
  local lo, hi = 0, W
  for k = fl((s - PY - 12) / RH), fl((s - PY) / RH) + 1 do
    local yk = s - k * RH
    if yk < PY + 12 and yk + RH > PY then
      local l, r, st = rowat(k)
      if st ~= "open" then lo, hi = math.max(lo, l + 7), math.min(hi, r - 7) end
    end
  end
  if lo > hi then lo = (lo + hi) / 2; hi = lo end
  px, target = clamp(px, lo, hi), clamp(target, lo, hi)
end

local function tunnel_hit()
  local x0, x1, y0, y1 = px - 5, px + 5, PY, PY + 12
  for k = fl((s - y1) / RH), fl((s - y0) / RH) + 1 do
    local yk = s - k * RH
    if yk < y1 and yk + RH > y0 then
    end
  end
  for _, o in ipairs(obs) do
    local y = s - o.k * RH
    if y < y1 + 12 and y > y0 - 30 then
      if o.t == "rock" then
        local cx, cy = clamp(o.x, x0, x1), clamp(y, y0, y1)
        if dist(cx, cy, o.x, y) < o.r * 0.9 then return true end
      elseif x1 > o.x and x0 < o.x + o.w and y1 > y and y0 < y + o.h then
        return true
      end
    end
  end
  return false
end

local function draw_tunnel()
  local t = sys.frame()
  for k = fl((s - H) / RH), fl((s + RH) / RH) do
    local l, r, st = rowat(k)
    local y = fl(s - k * RH)
    if st ~= "open" then
      local rock = st == "rock"
      gfx.fill_rect(l, y, r - l, RH, rock and 1 or 0)
      if not rock and k % 4 == 0 then gfx.fill_rect(l, y, r - l, 1, 1) end
      local base, edge = rock and 2 or 3, rock and 15 or 2
      if l > 0 then
        gfx.fill_rect(0, y, l, RH, base)
        gfx.fill_rect(math.max(0, l - 4), y, math.min(4, l), RH, edge)
      end
      if r < W then
        gfx.fill_rect(r, y, W - r, RH, base)
        gfx.fill_rect(r, y, 4, RH, edge)
      end
      if rock and (k * 13) % 7 == 0 then gfx.fill_rect(l - 14, y + 2, 4, 3, 3); gfx.fill_rect(r + 10, y + 3, 4, 3, 3) end
      if not rock and k % 3 == 0 then
        local lit = (k // 3 + t // 10) % 2 == 0 and 20 or 2
        gfx.fill_rect(l - 10, y + 2, 4, 4, lit)
        gfx.fill_rect(r + 6, y + 2, 4, 4, lit)
      end
    end
  end
  for _, o in ipairs(obs) do
    local y = fl(s - o.k * RH)
    if o.t == "rock" then
      if y > -20 and y < H + 20 then o.y = y; draw_rock(o) end
    elseif y > -30 and y < H then
      ui.plate(o.x + 2, y + 2, o.w, o.h, 2, o.t == "gate" and 2 or 3)
      ui.plate(o.x, y, o.w, o.h, 3, o.t == "gate" and 2 or 3)
      if o.t == "gate" then
        for i = 0, o.w // 8 - 1 do gfx.fill_rect(o.x + 3 + i * 8, y + 3, 4, 4, 17) end
      else
        gfx.fill_rect(o.x + 4, y + 2, math.max(1, o.w - 8), 2, 4)
      end
    end
  end
end

-- ---------------------------------------------------------------- game flow
local function start_level(n)
  lv, quota, prog = n, 20 + 8 * (n - 1), 0
  ammo = math.max(ammo, 5)
  gen_tunnel()
  s, state, lt, boss, inv = PY, "intro", 0, nil, 0
  BANNER[2], TOAST.t = 0, 0
  for _, list in ipairs({ rocks, shots, caps }) do
    for i = 1, #list do list[i].dead = true end
  end
  for i = 1, #parts do parts[i][5] = 0 end
  px, target = W / 2, W / 2
end

local function new_game()
  lives, score, ammo, autot, bombs, flash, noammo, cool = 3, 0, 5, 0, 0, 0, 0, 0
  newbest = false
  start_level(1)
end

function init()
  mv, act = ui.sides()
  mv_was = mv
  last_pos = knob.pos(mv)
  vel, px, target = 0, W / 2, W / 2
  best = save.get("best", 0)
  rocks, shots, caps, parts, TL, TR = {}, {}, {}, {}, {}, {}
  for i = 1, 36 do parts[i] = { 0, 0, 0, 0, 0, 4 } end
  SX, SY, SV = {}, {}, {}
  for i = 1, 30 do SX[i], SY[i], SV[i] = rnd_int(4, W - 4), rnd_int(0, H), 0.4 + rnd(1.2) end
  s, nrows, obs = PY, 0, {}
  new_game()
end

local function game_over()
  state, timer = "over", 0
  newbest = score > best
  if newbest then best = score; save.set("best", best) end
  boom(px, PY + 6, 30, C_SHIP)
end

local function hurt()
  if inv > 0 then return end
  lives = lives - 1
  inv = 60
  boom(px, PY + 6, 14, C_SHIP)
  if lives <= 0 then game_over()
  elseif state == "tunnel" then
    local l, r = rowat(fl((s - PY) / RH))
    px = (l + r) / 2
    target = px
  end
end

local function gain(v) prog = prog + v; score = score + v * 10 end

-- blast a rock: big ones split in two, small ones vanish
local function blast(r, split)
  if r.dead then return end
  local x, y, vy, big = r.x, r.y, r.vy, r.big
  r.dead = true
  boom(x, y, big and 10 or 6, C_ROCK)
  if state == "rain" then gain(big and 3 or 2) else score = score + (big and 30 or 20) end
  if big and split then
    for d = -1, 1, 2 do
      local c = mkrock(x + d * 8, y, rnd_int(5, 7))
      if c then c.vx, c.vy = d * (0.8 + rnd(0.6)), vy * 0.9 + 0.3 end
    end
  end
end

local function bomb()
  bombs, flash = 0, 14
  for i = 1, #rocks do blast(rocks[i], false) end
  boom(px, PY, 20, C_BOMB)
end

local function collect(k)
  score = score + 25
  if k == "a" then ammo = math.min(20, ammo + 5); say("+5 SHOTS", 22)
  elseif k == "u" then autot = 150; say("AUTO-FIRE!", 17)
  elseif k == "l" then
    if lives < 5 then lives = lives + 1; say("EXTRA LIFE", 16) else score = score + 200; say("+200", 16) end
  else bombs = 1; say("BOMB READY: click " .. mv, 25) end
end

local function pick_kind()
  local wa = 40 + (ammo < 3 and 50 or 0) + (state == "boss" and 30 or 0)
  local wl, wb = lives < 5 and 12 or 0, bombs == 0 and 16 or 0
  local pick = rnd(wa + 22 + wl + wb)
  if pick < wa then return "a" end
  pick = pick - wa
  if pick < 22 then return "u" end
  pick = pick - 22
  if pick < wl then return "l" end
  return wb > 0 and "b" or "a"
end

local function drop_rock(x, y, big, vy)
  local o = mkrock(x, y, big and rnd_int(11, 14) or rnd_int(5, 7))
  if o then
    o.vx = rnd(1.0) - 0.5
    o.vy = vy or (math.min(1.5 + lv * 0.12, 3.4) * (big and 0.8 or 1) + rnd(0.7))
  end
end

local function fire()
  local b = take(shots, 14, function() return { x = 0, y = 0, dead = true } end)
  if b then b.x, b.y, b.dead = px, PY - 4, false end
end

local function play()
  -- firing
  if cool > 0 then cool = cool - 1 end
  if noammo > 0 then noammo = noammo - 1 end
  if autot > 0 then
    autot = autot - 1
    if cool == 0 then fire(); cool = 4 end
  elseif knob.pressed(act) then
    if ammo > 0 then ammo = ammo - 1; fire() else noammo = 14 end
  end
  if knob.pressed(mv) and bombs > 0 then bomb() end
  if flash > 0 then flash = flash - 1 end

  -- rocks: spawn, fall, collide
  if state == "rain" then
    spawnt = spawnt - 1
    if spawnt <= 0 then
      spawnt = math.max(9, 30 - lv * 2) + rnd_int(0, 10)
      local x = rnd(1) < 0.2 and clamp(px + rnd_int(-30, 30), 24, W - 24) or rnd_int(24, W - 24)
      drop_rock(x, -16, rnd(1) < math.min(0.55, 0.28 + lv * 0.03))
    end
  end
  for i = 1, #rocks do
    local r = rocks[i]
    if not r.dead then
      r.x, r.y, r.a = r.x + r.vx, r.y + r.vy, r.a + r.spin
      if r.x < r.r + 8 or r.x > W - r.r - 8 then r.vx = -r.vx end
      if r.y - r.r > H then
        r.dead = true
        if state == "rain" then gain(1) end
      elseif r.y > PY - 20 and dist(clamp(r.x, px - 5, px + 5), clamp(r.y, PY, PY + 12), r.x, r.y) < r.r * 0.85 then
        boom(r.x, r.y, 8, C_HURT)
        r.dead = true
        hurt()
      end
    end
  end

  -- shots
  for i = 1, #shots do
    local b = shots[i]
    if not b.dead then b.y = b.y - 8 end
    if b.dead then
    elseif b.y < -10 then b.dead = true
    elseif boss and boss.ph == "fight" and abs(b.x - boss.x) < 24 and b.y < boss.y + 24 and b.y > boss.y - 6 then
      boss.hp, boss.flash = boss.hp - 1, 4
      score = score + 20
      boom(b.x, b.y, 4, C_ZAP)
      b.dead = true
    else
      for j = 1, #rocks do
        local r = rocks[j]
        if not r.dead and dist(b.x, b.y + 2, r.x, r.y) < r.r + 2 then b.dead = true; blast(r, true); break end
      end
    end
  end

  -- capsules
  capt = capt - 1
  if capt <= 0 and state ~= "warn" then
    local fight = state == "boss"
    capt = fight and (ammo < 3 and autot == 0 and rnd_int(30, 60) or rnd_int(90, 150)) or rnd_int(150, 270)
    if not (fight and boss.ph ~= "fight") then
      local q = take(caps, 4, function() return { x = 0, y = 0, k = "a", dead = true } end)
      if q then q.x, q.y, q.k, q.dead = fight and boss.x or rnd_int(30, W - 30), fight and boss.y + 24 or -10, pick_kind(), false end
    end
  end
  for i = 1, #caps do
    local q = caps[i]
    if not q.dead then
      q.y = q.y + 1.6
      if q.y > PY - 8 and q.y < PY + 14 and abs(q.x - px) < 16 then collect(q.k); q.dead = true
      elseif q.y > H + 10 then q.dead = true end
    end
  end

  -- the end of the rain, and the boss
  if state == "rain" and prog >= quota then state, timer = "warn", 0; ban("WATCH OUT!", 60, 16) end
  if state == "warn" then
    timer = timer + 1
    if timer >= 60 then
      state, capt = "boss", 150
      boss = { x = W / 2, y = -30, hp = 15, ph = "in", tx = W / 2, dt = 0, drop = 40, flash = 0, t = 0 }
    end
  elseif state == "boss" then
    local b = boss
    if b.flash > 0 then b.flash = b.flash - 1 end
    if b.ph == "in" then
      b.y = b.y + 1.6
      if b.y >= 36 then b.y, b.ph = 36, "fight" end
    elseif b.ph == "fight" then
      b.dt = b.dt - 1
      if b.dt <= 0 or abs(b.tx - b.x) < 2 then b.tx, b.dt = rnd_int(40, W - 40), rnd_int(30, 80) end
      local bs = math.min(1.6 + lv * 0.1, 3.5)
      b.x = b.x + clamp(b.tx - b.x, -bs, bs)
      b.drop = b.drop - 1
      if b.drop <= 0 then
        b.drop = math.max(20, 46 - lv * 2)
        drop_rock(b.x + rnd_int(-12, 12), b.y + 26, rnd(1) < 0.3, 1.8 + rnd(1.0))
      end
      if b.hp <= 0 then
        b.ph, b.t = "boom", 60
        score = score + 500
        for i = 1, #rocks do blast(rocks[i], false) end
        ban("BOSS DOWN!", 60, 20)
      end
    else
      b.t = b.t - 1
      if b.t % 4 == 0 then boom(b.x + rnd_int(-20, 20), b.y + rnd_int(0, 22), 6, C_BOSS) end
      if b.t <= 0 then state, timer, boss = "clear", 0, nil end
    end
  end
end

function update()
  mv, act = ui.sides()
  local p = knob.pos(mv)
  if mv ~= mv_was then mv_was = mv; last_pos = p end
  local d = p - last_pos
  last_pos = p
  vel = vel * 0.8 + abs(d) * 30 * 0.2
  local gain_k = clamp(0.2 + vel / 50, 0.3, 2.0)
  target = clamp(target + d * (W - 44) / 40 * gain_k, 22, W - 22)
  px = px + clamp((target - px) * 0.35, -16, 16)
  if abs(target - px) < 0.3 then px = target end

  for i = 1, #parts do
    local q = parts[i]
    if q[5] > 0 then q[1], q[2], q[4], q[5] = q[1] + q[3], q[2] + q[4], q[4] + 0.1, q[5] - 1 end
  end
  for i = 1, 30 do
    SY[i] = SY[i] + SV[i] * (state == "tunnel" and 3 or 1)
    if SY[i] > H then SY[i], SX[i] = 0, rnd_int(4, W - 4) end
  end
  if TOAST.t > 0 then TOAST.t = TOAST.t - 1 end
  if BANNER[2] > 0 then BANNER[2] = BANNER[2] - 1 end

  if state == "over" then
    timer = timer + 1
    if timer > 20 and knob.pressed(act) then new_game() end
    return
  end
  lt = lt + 1
  if inv > 0 then inv = inv - 1 end
  if state ~= "intro" and state ~= "clear" and s < PY + RH * (nrows + 40) + H then s = s + speed end

  if state == "intro" then
    s = PY
    wall_block()
    if lt >= 66 then state, inv = "tunnel", 30 end
  elseif state == "tunnel" then
    wall_block()
    if inv == 0 and tunnel_hit() then hurt() end
    if s >= PY + RH * (nrows + 3) and state == "tunnel" then
      state, spawnt, capt = "rain", 20, rnd_int(90, 150)
      ban("ROCK RAIN!", 45, 4)
    end
  elseif state == "clear" then
    timer = timer + 1
    if timer > 90 then start_level(lv + 1) end
  else
    play()
  end
end

-- ---------------------------------------------------------------- drawing
function stats()
  return { { "Level", lv }, { "Score", score }, { "Best", best }, { "Shots", autot > 0 and "auto" or ammo } }
end

local function draw_caps()
  for i = 1, #caps do
    local q = caps[i]
    if not q.dead then
      local x, y, c = fl(q.x), fl(q.y), CAPS[q.k]
      local bob = (sys.frame() // 4) % 2
      gfx.fill_rect(x - 9, y - 4, 20, 14, 1)
      gfx.fill_rect(x - 10, y - 7, 20, 14, c[1])
      gfx.fill_rect(x - 10, y - 7, 20, 2, 4)
      gfx.rect(x - 11 - bob, y - 8 - bob, 22 + bob * 2, 16 + bob * 2, 4)
      if q.k == "l" then ui.heart(x - 5, y - 4, 4)
      else gfx.label(c[2], x - gfx.label_width(c[2], 1) // 2, y - 3, q.k == "b" and 4 or 1, 1) end
    end
  end
end

local function draw_ship()
  if state == "over" or (inv > 0 and (inv // 3) % 2 == 0) then return end
  local x = fl(px)
  local f = 2 + sys.frame() % 3
  gfx.fill_rect(x - 3, PY + 12, 7, f, 17)
  gfx.fill_rect(x - 1, PY + 12, 3, f, 18)
  gfx.sprite("ship", x - 7, PY)
end

local function draw_hud()
  gfx.label("" .. score, ui.PAD, 6, 4, 2)
  for i = 1, lives do ui.heart(W - ui.PAD - i * 14 + 3, 8, 16) end
  -- level progress (tunnel, then rain) or the boss's health
  local x0 = 112
  if boss then
    for i = 1, 15 do gfx.fill_rect(x0 + (i - 1) * 6, 11, 5, 7, i <= boss.hp and 16 or 24) end
  else
    local f = state == "tunnel" and clamp((s - PY) / (RH * nrows), 0, 1) or clamp(prog / quota, 0, 1)
    gfx.fill_rect(x0, 11, 96, 7, 24)
    if state == "rain" or state == "warn" then f = clamp(prog / quota, 0, 1) end
    gfx.fill_rect(x0, 11, fl(96 * f), 7, state == "tunnel" and 22 or 18)
  end
  -- shots left
  if autot > 0 then
    gfx.fill_rect(ui.PAD, 26, fl(autot * 0.4), 6, 17 + sys.frame() % 3)
  elseif ammo == 0 then
    if (sys.frame() // 5) % 2 == 0 or noammo > 0 then gfx.label("NO SHOTS", ui.PAD, 26, 16, 1) end
  else
    for i = 1, ammo do gfx.fill_rect(ui.PAD + (i - 1) * 4, 26, 3, 7, noammo > 0 and 16 or 18) end
  end
  if bombs > 0 then
    local x = W - ui.PAD - 6
    gfx.fill_circle(x, 33, 6, 3)
    gfx.fill_circle(x - 2, 31, 2, 4)
    gfx.fill_rect(x + 3, 24, 2, 4, 15)
    if (sys.frame() // 4) % 2 == 0 then gfx.fill_rect(x + 3, 22, 2, 2, 18) end
  end
end

function draw()
  gfx.clear(0)
  for i = 1, 30 do gfx.pixel(fl(SX[i]), fl(SY[i]), SV[i] > 1 and 22 or 24) end
  if nrows > 0 and s < PY + RH * nrows + H + 20 then draw_tunnel() end
  draw_caps()
  for i = 1, #rocks do if not rocks[i].dead then draw_rock(rocks[i]) end end
  if boss then
    if not (boss.ph == "boom" and boss.t % 4 < 2) then
      gfx.sprite(boss.flash > 0 and "alien_hit" or "alien", fl(boss.x - 22), fl(boss.y), false, false, 2)
    end
  end
  for i = 1, #shots do
    if not shots[i].dead then gfx.fill_rect(fl(shots[i].x) - 1, fl(shots[i].y), 3, 9, 18) end
  end
  draw_ship()
  for i = 1, #parts do
    local q = parts[i]
    if q[5] > 0 then gfx.fill_rect(fl(q[1]), fl(q[2]), 3, 3, q[6]) end
  end
  if flash > 0 then
    for i = 0, 2 do gfx.circle(fl(px), PY, (14 - flash) * 14 - i * 3, i == 0 and 4 or 22) end
  end
  draw_hud()

  if BANNER[2] > 0 then
    local w = gfx.label_width(BANNER[1], 4)
    local y = 62 + (BANNER[2] > 40 and (BANNER[2] - 40) or 0) * 2
    gfx.label(BANNER[1], (W - w) // 2 + 3, y + 3, 1, 4)
    gfx.label(BANNER[1], (W - w) // 2, y, BANNER[3] == 4 and 18 or BANNER[3], 4)
  end
  if TOAST.t > 0 then ui.pill(TOAST.text, 100, TOAST.c) end
  if state == "intro" then
    ui.card("LEVEL " .. lv, NAMES[(lv - 1) % #NAMES + 1], nil, 25, lt)
  elseif lv == 1 and lt > 66 and lt < 220 and state == "tunnel" then
    ui.controls(100, { { "click", act, "Fire" }, { "click", mv, "Bomb" } })
    ui.controls(70, { { "turn", mv, "Fly" } })
  elseif state == "clear" then
    ui.card("LEVEL CLEAR!", "Score " .. score, nil, 20, timer)
  elseif state == "over" then
    local line = newbest and "New best: " .. score .. "!" or "Score " .. score .. "   Best " .. best
    ui.card("GAME OVER", line, timer > 20 and "Retry" or nil, 16, timer, "click")
  end
end
