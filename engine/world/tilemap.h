#pragma once

// Tile maps. Tiles are drawn once into a cache at screen resolution, with the
// bevel baked in, so drawing a level each frame is plain copying.

#include <cstdint>
#include <vector>

#include "../game/game_data.h"
#include "../gfx/art.h"
#include "../gfx/canvas.h"
#include "../gfx/palette.h"

namespace tg {

struct TileDef {
    gfx::Art art;        // square, the same size for every tile in a set
    bool solid = false;  // solid tiles get the bevel edge
};

class TileCache {
public:
    // Renders every tile. Called at level load, not per frame. Returns false
    // (and stays empty) if the art is invalid or the tiles differ in size.
    bool build(const TileDef* tiles, int count, const gfx::Palette& palette, int scale, bool bevel);
    // The same, from a loaded game: tile i draws sprites[tiles[i].sprite] and
    // counts as solid (for the bevel) when it has TILE_SOLID. A game with no
    // tiles gives an empty cache and true. Returns false (and stays empty) if
    // a sprite index is out of range or the tile sprites differ in size.
    bool build(const std::vector<SpriteData>& sprites, const std::vector<TileData>& tiles,
               const gfx::Palette& palette, int scale, bool bevel);

    int block() const { return block_; }  // screen pixels per tile side
    int count() const { return count_; }
    const gfx::Color* pixels(int tile) const { return pixels_.data() + tile * block_ * block_; }
    // One byte per pixel, or nullptr when the tile has no transparent pixels.
    const std::uint8_t* mask(int tile) const {
        return opaque_[tile] ? nullptr : mask_.data() + tile * block_ * block_;
    }

private:
    int block_ = 0;
    int count_ = 0;
    std::vector<gfx::Color> pixels_;
    std::vector<std::uint8_t> mask_;
    std::vector<std::uint8_t> opaque_;
};

struct Layer {
    int cols = 0;
    int rows = 0;
    const std::uint8_t* cells = nullptr;  // cols * rows; 0 = empty, n = tile n-1
    int parallax = 256;                   // 256 = moves with the camera, 128 = half speed
};

// Draws the part of a layer visible in `viewport`, never outside it.
void drawLayer(gfx::Canvas& canvas, const TileCache& cache, const Layer& layer,
               const gfx::Rect& viewport, int camera_x, int camera_y);

}  // namespace tg
