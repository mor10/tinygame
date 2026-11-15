-- Saves across runs: tinygame-sim console/carts/tests/save_basic.lua --runs 3 --frames 5
-- Expect logs "run 1", "run 2", "run 3" with every check passing, and no error.
cart = { title = "Save basic", api = 1 }

local runs = save.get("runs", 0) + 1
local n = 0
local function check(ok, what) if not ok then error("save check failed: " .. what, 2) end n = n + 1 end

if runs > 1 then
  check(save.get("name") == "Mia & Dad\tTab\nNew line", "string with tab and newline")
  check(save.get("best") == 12345 and math.type(save.get("best")) == "integer", "integer")
  check(math.abs(save.get("speed") - 1.25) < 1e-6, "float")
  check(save.get("sound") == false, "boolean false")
  check(save.get("gone") == nil and save.get("gone", "dflt") == "dflt", "removed key gives the default")
end
save.set("runs", runs)
save.set("name", "Mia & Dad\tTab\nNew line")
save.set("best", 12345)
save.set("speed", 1.25)
save.set("sound", false)
save.set("gone", 1)
save.set("gone", nil)
check(not pcall(save.set, "big", string.rep("x", 5000)), "a value over 4 KB is refused")
check(save.get("big") == nil, "the refused value isn't kept")
check(not pcall(save.set, "f", function() end), "functions can't be saved")
check(not pcall(save.set, string.rep("k", 40), 1), "keys over 32 characters are refused")
sys.log("run " .. runs .. ": " .. n .. " checks passed")
function draw() gfx.clear(1) end
