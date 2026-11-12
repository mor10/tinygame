local palette = require("palette")
local cached_palette = require("palette")
assert(palette == cached_palette)

function update()
  tg.rect(2, 3, 4, 5, palette.index)
end
