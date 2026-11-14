-- Helpers every cart gets (engine v1). Runs in the cart's sandbox before the cart itself.
-- Errors use level 2 so the message points at the cart line that called the helper.

local function need_box(v, what)
  if type(v) ~= "table" or type(v.x) ~= "number" or type(v.y) ~= "number"
     or type(v.w) ~= "number" or type(v.h) ~= "number" then
    error(what .. " needs a table with x, y, w and h", 3)
  end
end

function hit(a, b)
  need_box(a, "hit(a, b): a")
  need_box(b, "hit(a, b): b")
  return a.x < b.x + b.w and b.x < a.x + a.w and a.y < b.y + b.h and b.y < a.y + a.h
end

function clamp(v, lo, hi)
  if v < lo then return lo elseif v > hi then return hi else return v end
end

function lerp(a, b, t) return a + (b - a) * t end

function dist(x1, y1, x2, y2)
  local dx, dy = x2 - x1, y2 - y1
  return math.sqrt(dx * dx + dy * dy)
end

function rnd(n) return math.random() * (n or 1) end
function rnd_int(a, b) return math.random(a, b) end

function choose(list)
  if type(list) ~= "table" or #list == 0 then error("choose(list) needs a list with something in it", 2) end
  return list[math.random(#list)]
end

-- group(): a list of objects that is safe to change while you loop over it.
local Group = {}
Group.__index = Group

function group()
  return setmetatable({ items = {}, looping = 0, holes = false }, Group)
end

function Group:add(t)
  if type(t) ~= "table" then error("group:add needs a table", 2) end
  self.items[#self.items + 1] = t
  return t
end

function Group:remove(t)
  local items = self.items
  for i = 1, #items do
    if items[i] == t then
      if self.looping > 0 then items[i] = false; self.holes = true
      else table.remove(items, i) end
      return
    end
  end
end

function Group:each(fn)
  local items = self.items
  self.looping = self.looping + 1
  for i = 1, #items do
    local t = items[i]
    if t then fn(t) end
  end
  self.looping = self.looping - 1
  if self.looping == 0 and self.holes then
    local j = 1
    for i = 1, #items do
      if items[i] then items[j] = items[i]; j = j + 1 end
    end
    for i = j, #items do items[i] = nil end
    self.holes = false
  end
end

function Group:count()
  local n = 0
  for i = 1, #self.items do if self.items[i] then n = n + 1 end end
  return n
end
