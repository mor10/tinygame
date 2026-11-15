-- Power-cut test for saves. Every frame it bumps a counter and stores a ~3 KB string derived
-- from it; saves are written about once a second. At start it checks that the stored pair still
-- matches. Cut power (or reset) at random moments, then start it again: every start must log
-- "torture ok", never "torture BAD", and the save must never be reported as damaged.
cart = { title = "Save torture", api = 1 }

local function sig(n)
  local parts = {}
  for i = 1, 60 do parts[i] = string.format("%06x", (n * 40503 + i * 977) & 0xffffff) end
  return table.concat(parts, ":", 1, 60) .. string.rep("=", 2400)
end

local n = save.get("n", 0)
local started = n
local good = n == 0 or save.get("check") == sig(n)
sys.log((good and "torture ok n=" or "torture BAD n=") .. n)

function update()
  n = n + 1
  save.set("n", n)
  save.set("check", sig(n))
end

function draw()
  gfx.clear(good and 1 or 7)
  gfx.text(good and "SAVE OK" or "SAVE DAMAGED", 8, 8, 5, true)
  gfx.text("Started from n=" .. started, 8, 36, 5)
  gfx.text("Now n=" .. n, 8, 50, 5)
  gfx.text("Unplug the power any time, then", 8, 80, 12)
  gfx.text("start this game again.", 8, 92, 12)
end
