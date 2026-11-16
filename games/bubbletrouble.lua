-- Bubble Trouble: an alien lab. Hold the console upright and tilt it so the gas
-- bubbles float up through the vents into the flask that matches their colour.
-- Miss, and they pile up on the ceiling: flip the console upside down to float
-- them back down... but flipping can shake loose gas you already captured!
cart = { title = "Bubble Trouble", tagline = "Tilt to catch each gas in the right flask", api = 1, fps = 30,
  icon = [[
    ................
    ..aaa....888....
    .aaaaa..88888...
    .aaaaa..88888...
    ..aaa....888....
    ................
    444444....444444
    ..4444....4444..
    ...444....444...
    .....6666.......
    ....666666......
    ....666666......
    .....6666.......
    ................
    ................
    ................
  ]] }

local COLS = { 16, 18, 20, 27 }       -- red, lemon, green, pink
local N, R, K, DRAG = 5, 6, 0.28, 0.90 -- bubbles per bin, bubble radius, float push, drag
local CEIL, WY, DIVH = 12, 88, 34      -- ceiling, bench height, how far the flask dividers hang
local DMAX = 15                    -- how far the bench bulges above and below at the ridges
local RIM = { [16] = 7, [18] = 8, [20] = 10, [27] = 26 }   -- darker edge for each gas colour
local X0, X1, FLOOR
local lv, k, gap, state, age, runf, best, cols, binW, hold, total, homed
local segs, vents, funnels, bubbles, bag, mx, spawnt
local caught = {}
local ang, mode, ux, uy = 0, "tilt", 0, -1

local function shuffle(t)
  for i = #t, 2, -1 do local j = rnd_int(1, i); t[i], t[j] = t[j], t[i] end
end

local function setup(l)
  lv, state, age, hold = l, "intro", 0, 0
  k = ({ 1, 2, 2, 3, 3, 4 })[math.min(l, 6)]
  gap = clamp(math.floor(29 - 1.5 * l), 17, 27) + 6   -- visible opening, plus 3 px of lip each side
  local pool = { table.unpack(COLS) }
  shuffle(pool)
  cols, binW = {}, (X1 - X0) / k
  for i = 1, k do cols[i] = pool[i] end
  funnels = {}
  if k == 1 then funnels[1] = rnd_int(X0 + 50, X1 - 50)
  else  -- one funnel per colour, spread across the wall (not lined up with the bins)
    for i = 1, k do
      local cx = X0 + binW * (i - 0.5) + rnd_int(-10, 10)
      funnels[i] = clamp(math.floor(cx), X0 + 40, X1 - 40)
    end
  end
  mx = rnd_int(110, 210)
  -- The bench is a thick slab with an hourglass hole at each vent. Its underside climbs
  -- toward the nearest hole (so rising bubbles slide in) and its top side drops toward it
  -- (so bubbles floating down slide in too). There are no flat bits and no pockets.
  segs, vents = {}, {}
  for i, cx in ipairs(funnels) do
    local gl, gr = cx - gap / 2, cx + gap / 2
    local lb = i == 1 and X0 - 10 or (funnels[i - 1] + cx) / 2
    local rb = i == #funnels and X1 + 10 or (cx + funnels[i + 1]) / 2
    local v = { c = cx, gl = gl, gr = gr, lrun = gl - lb, rrun = rb - gr }
    vents[i] = v
    for _, d in ipairs({ -1, 1 }) do
      local ox, run = d < 0 and gl or gr, d < 0 and v.lrun or v.rrun
      local dep = math.min(0.6 * run, DMAX)
      segs[#segs + 1] = { ox, WY, ox + d * run, WY - dep, 3 }
      segs[#segs + 1] = { ox, WY, ox + d * run, WY + dep, 3 }
    end
  end
  for i = 1, k - 1 do
    local x = X0 + binW * i
    segs[#segs + 1] = { x, CEIL, x, CEIL + DIVH, 2 }
  end
  bag = {}
  for i = 1, k do for _ = 1, N do bag[#bag + 1] = cols[i] end end
  shuffle(bag)
  total, homed, caught, bubbles, spawnt = #bag, 0, {}, {}, 30
end

function init()
  X0, X1, FLOOR = ui.PAD, gfx.W - ui.PAD, gfx.H - 14
  best, runf = save.get("best", 0), 0
  setup(1)
end

local function binOf(x) return clamp(math.floor((x - X0) / binW) + 1, 1, k) end
local function isHome(b) return b.y < CEIL + DIVH + 2 and cols[binOf(b.x)] == b.c end

local function seg_hit(b, s)
  local x1, y1, x2, y2 = s[1], s[2], s[3], s[4]
  local dx, dy = x2 - x1, y2 - y1
  local u = clamp(((b.x - x1) * dx + (b.y - y1) * dy) / (dx * dx + dy * dy), 0, 1)
  local px, py = x1 + u * dx, y1 + u * dy
  local ex, ey = b.x - px, b.y - py
  local m, d2 = R + s[5], ex * ex + ey * ey
  if d2 < m * m then
    local d, nx, ny = math.sqrt(d2), 0, -1
    if d > 0.001 then nx, ny = ex / d, ey / d end
    b.x, b.y = px + nx * m, py + ny * m
    local vn = b.vx * nx + b.vy * ny
    if vn < 0 then b.vx, b.vy = b.vx - vn * nx * 1.3, b.vy - vn * ny * 1.3 end
  end
end

-- which way do the bubbles float? tilt, or the knobs (they spin the "up" arrow)
local function steer()
  local t = knob.turn("left") + knob.turn("right")
  local tx, ty = tilt.x(), tilt.y()
  if t ~= 0 then mode = "knob"; ang = ang + t * 0.314
  elseif mode == "knob" and math.abs(tx) + math.abs(ty) > 0.35 then mode = "tilt" end
  if mode == "knob" then ux, uy = math.sin(ang), -math.cos(ang)
  else
    ux, uy = -tx, -ty
    if math.abs(ux) < 0.1 then ux = 0 end
    if math.abs(uy) < 0.1 then uy = 0 end
    local m = math.sqrt(ux * ux + uy * uy)
    if m > 1 then ux, uy = ux / m, uy / m end
  end
end

local function spawn()
  spawnt = spawnt - 1
  if spawnt > 0 or #bag == 0 then return end
  local sy = FLOOR - R - 3
  for _, b in ipairs(bubbles) do
    if dist(b.x, b.y, mx, sy) < 2 * R + 1 then spawnt = 5; return end
  end
  bubbles[#bubbles + 1] = { x = mx, y = sy, vx = 0, vy = 0, c = table.remove(bag), ph = rnd(6), stuck = false }
  spawnt = 40
end

local function physics()
  for _, b in ipairs(bubbles) do
    if not b.stuck then
      b.ph = b.ph + 0.13
      b.vx = (b.vx + ux * K + math.sin(b.ph) * 0.03) * DRAG
      b.vy = (b.vy + uy * K + math.cos(b.ph * 0.8) * 0.03) * DRAG
      b.x, b.y = b.x + b.vx, b.y + b.vy
    end
  end
  for _ = 1, 2 do
    for _, b in ipairs(bubbles) do
      if not b.stuck then
        for _, s in ipairs(segs) do seg_hit(b, s) end
        if b.x < X0 + R then b.x, b.vx = X0 + R, 0 elseif b.x > X1 - R then b.x, b.vx = X1 - R, 0 end
        if b.y < CEIL + R then b.y, b.vy = CEIL + R, 0 elseif b.y > FLOOR - R then b.y, b.vy = FLOOR - R, 0 end
      end
    end
    for i = 1, #bubbles - 1 do
      local a = bubbles[i]
      for j = i + 1, #bubbles do
        local c = bubbles[j]
        local dx, dy = c.x - a.x, c.y - a.y
        local d2 = dx * dx + dy * dy
        if d2 < 4 * R * R then
          local d = math.sqrt(d2)
          if d < 0.01 then dx, dy, d = 1, 0, 1 end
          local o, nx, ny = 2 * R - d, dx / d, dy / d
          if a.stuck and c.stuck then
          elseif a.stuck then c.x, c.y = c.x + nx * o, c.y + ny * o
          elseif c.stuck then a.x, a.y = a.x - nx * o, a.y - ny * o
          else
            a.x, a.y = a.x - nx * o * 0.5, a.y - ny * o * 0.5
            c.x, c.y = c.x + nx * o * 0.5, c.y + ny * o * 0.5
          end
        end
      end
    end
  end
end

-- right-coloured bubbles that reach the ceiling of their bin stick there; flip the
-- console and each stuck bubble has a chance to come loose and fall back down
local function sticking()
  local loose = (0.12 + 0.04 * lv) / 30
  homed, caught = 0, {}
  for _ = 1, 2 do
    for _, b in ipairs(bubbles) do
      if b.stuck then
        if uy > 0.5 and math.random() < loose then b.stuck = false end
      elseif isHome(b) and uy < -0.3 and b.vx * b.vx + b.vy * b.vy < 0.5 then
        local held = b.y <= CEIL + R + 2
        if not held then
          for _, o in ipairs(bubbles) do
            if o.stuck and o.y < b.y and math.abs(o.x - b.x) < R + 2 and b.y - o.y < 2 * R + 3 then held = true; break end
          end
        end
        if held then b.stuck = true; b.vx, b.vy = 0, 0 end
      end
    end
  end
  for _, b in ipairs(bubbles) do
    if isHome(b) then homed = homed + 1; caught[b.c] = (caught[b.c] or 0) + 1 end
  end
end

function update()
  age = age + 1
  local click = age > 10 and (knob.pressed("left") or knob.pressed("right"))
  if state == "intro" then
    if click then
      state, age = "play", 0
    end
    return
  elseif state == "clear" then
    if click then setup(lv + 1) end
    return
  end
  runf = runf + 1
  steer()
  spawn()
  physics()
  sticking()
  hold = (homed == total) and hold + 1 or 0
  if hold > 25 then
    state, age = "clear", 0
    if lv > best then best = lv; save.set("best", best) end
  end
end

function stats()
  return { { "Level", lv }, { "Home", homed .. "/" .. total }, { "Time", runf // 30 .. "s" }, { "Best", best } }
end

local function draw_bubble(b)
  local x, y = math.floor(b.x), math.floor(b.y)
  gfx.fill_circle(x, y, R, RIM[b.c] or 2)
  gfx.fill_circle(x, y, R - 1, b.c)
  gfx.fill_rect(x - 3, y - 3, 2, 2, 4)
  gfx.pixel(x - 1, y - 4, 4)
end

-- how thick the bench is at column x (0 inside a vent hole, growing toward the ridge)
local function bench_d(x)
  local e, run = 999, 1
  for _, v in ipairs(vents) do
    local d = x < v.c and v.gl - x or x - v.gr
    if d < e then e, run = d, x < v.c and v.lrun or v.rrun end
  end
  return e, math.min(0.6 * run, DMAX) * math.max(0, e) / run
end

local function draw_lab()
  local f = sys.frame()
  gfx.clear(1)
  gfx.fill_rect(0, WY, gfx.W, gfx.H, 24)
  for x = X0 - 8, X1, 40 do
    gfx.fill_rect(x, 0, 1, WY, 24)
    gfx.fill_rect(x, WY, 1, gfx.H - WY, 1)
  end
  gfx.fill_rect(0, 128, gfx.W, 1, 1)
  for g = 1, 5 do   -- alien writing on the wall
    local n, gx, gy = (g * 73 + lv * 41) % 512, X0 + 6 + (g * 53) % 230, 112 + (g * 29) % 22
    for r = 0, 2 do for c = 0, 2 do
      if (n >> (r * 3 + c)) & 1 == 1 then gfx.fill_rect(gx + c * 3, gy + r * 3, 2, 2, 1) end
    end end
  end
  for i = 1, 8 do   -- slow fizz in the chamber
    local fy = FLOOR - 8 - (f * (0.25 + (i % 3) * 0.15) + i * 31) % (FLOOR - WY - 36)
    gfx.circle(X0 + (i * 41) % (X1 - X0) + math.floor(math.sin(f * 0.05 + i) * 3), math.floor(fy), 2, 23)
  end
  -- flasks hanging from a rail
  for i = 1, k do
    local bx, bw = math.floor(X0 + binW * (i - 1)), math.floor(binW)
    gfx.fill_rect(bx, CEIL, bw, DIVH, 0)
    for yy = CEIL + 5, CEIL + DIVH - 7, 9 do
      for xx = bx + 6 + (yy // 9 % 2) * 4, bx + bw - 6, 9 do gfx.pixel(xx, yy, cols[i]) end
    end
    gfx.fill_rect(bx + 2, CEIL + DIVH - 2, bw - 4, 2, cols[i])
  end
  gfx.fill_rect(X0 - 8, CEIL - 2, X1 - X0 + 16, 2, 12)
  gfx.fill_rect(X0 - 8, 0, X1 - X0 + 16, CEIL - 2, 11)
  for i = 1, k do   -- pips live on the rail: redraw over it
    local bx, bw = math.floor(X0 + binW * (i - 1)), math.floor(binW)
    local n, c = math.min(N, caught[cols[i]] or 0), cols[i]
    local x0 = bx + (bw - (N * 9 - 2)) // 2
    for j = 1, N do
      local lit = j <= n
      gfx.fill_rect(x0 + (j - 1) * 9, 3, 7, 5, c)
      gfx.fill_rect(x0 + (j - 1) * 9 + 1, 4, 5, 3, lit and c or 1)
      if lit then gfx.fill_rect(x0 + (j - 1) * 9 + 1, 4, 5, 1, 4) end
    end
  end
  for i = 0, k do
    local x = math.floor(X0 + binW * i)
    gfx.fill_rect(x - 2, CEIL, 4, DIVH, 21)
    gfx.fill_rect(x - 2, CEIL, 1, DIVH, 4)
  end
  -- the bench
  for x = X0 - 10, X1 + 10 do
    local e, d = bench_d(x)
    if e > -3 then
      local y1, h = math.floor(WY - d - 3), math.floor(2 * d + 6)
      gfx.fill_rect(x, y1, 1, h, 11)
      gfx.pixel(x, y1, 12)
      gfx.pixel(x, y1 + h - 1, 2)
    end
  end
  for x = X0 + 12, X1, 24 do
    if bench_d(x) > 6 then gfx.fill_rect(x, WY - 1, 2, 2, 12) end
  end
  -- floor plate with hazard stripes, and the gas emitter
  gfx.fill_rect(0, FLOOR, gfx.W, 2, 3)
  gfx.fill_rect(0, FLOOR + 2, gfx.W, gfx.H - FLOOR - 2, 0)
  for r = 0, 5 do
    for x = -12 + r, gfx.W, 16 do gfx.fill_rect(x, FLOOR + 4 + r, 8, 1, 18) end
  end
  gfx.fill_rect(mx - 12, FLOOR - 3, 24, 4, 3)
  gfx.fill_rect(mx - 7, FLOOR - 9, 14, 6, 2)
  gfx.fill_rect(mx - 7, FLOOR - 9, 14, 1, 3)
  if bag and #bag > 0 then gfx.fill_circle(mx, FLOOR - 5, 2, bag[#bag]) end
end

function draw()
  draw_lab()
  for _, b in ipairs(bubbles) do draw_bubble(b) end
  -- which way is up, and where we are
  local gx, gy = X1 - 12, FLOOR - 14
  gfx.fill_circle(gx, gy, 11, 1)
  gfx.circle(gx, gy, 11, 11)
  gfx.line(gx, gy, gx + math.floor(ux * 8), gy + math.floor(uy * 8), 4)
  gfx.fill_circle(gx + math.floor(ux * 8), gy + math.floor(uy * 8), 2, 18)
  gfx.label("LEVEL " .. lv, X0, FLOOR - 12, 4, 1)
  gfx.label(homed .. "/" .. total, X0, FLOOR - 24, 12, 1)
  if state == "intro" then
    local line = lv == 1 and "Hold it upright and tilt" or (k == 1 and "Guide the gas up the vent" or "Sort each gas into its flask")
    ui.card("LEVEL " .. lv, line, "Start", 25, age, "click")
  elseif state == "clear" then
    ui.card("GAS CAPTURED!", "Time " .. runf // 30 .. "s", "Next", 20, age, "click")
  elseif lv == 1 and runf < 150 then
    ui.pill("Gas floats to the high side", 108)
  end
end
