#include "tilemap.h"

#include "../gfx/sprite.h"

namespace tg {

namespace {
int floorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
}  // namespace

bool TileCache::build(const TileDef* tiles, int count, const gfx::Palette& palette, int scale,
                      bool bevel) {
    block_ = count_ = 0;
    pixels_.clear();
    mask_.clear();
    opaque_.clear();
    if (tiles == nullptr || count <= 0 || scale < 1) return false;
    const int art_size = tiles[0].art.width;
    for (int i = 0; i < count; ++i) {
        const gfx::Art& art = tiles[i].art;
        if (!gfx::artValid(art) || art.width != art_size || art.height != art_size) return false;
    }
    const int block = art_size * scale;
    const std::size_t per_tile = static_cast<std::size_t>(block) * block;
    pixels_.assign(per_tile * count, 0);
    mask_.assign(per_tile * count, 0);
    opaque_.assign(count, 1);
    for (int i = 0; i < count; ++i) {
        gfx::Canvas canvas({pixels_.data() + i * per_tile, block, block, block});
        gfx::DrawOptions options;
        options.scale = scale;
        options.bevel = bevel && tiles[i].solid;
        gfx::drawArt(canvas, tiles[i].art, palette, 0, 0, options);
        for (int y = 0; y < block; ++y) {
            for (int x = 0; x < block; ++x) {
                const char c = tiles[i].art.pixels[(y / scale) * art_size + x / scale];
                const bool solid = gfx::artIndex(c) > 0;
                mask_[i * per_tile + y * block + x] = solid ? 1 : 0;
                if (!solid) opaque_[i] = 0;
            }
        }
    }
    block_ = block;
    count_ = count;
    return true;
}

bool TileCache::build(const std::vector<SpriteData>& sprites, const std::vector<TileData>& tiles,
                      const gfx::Palette& palette, int scale, bool bevel) {
    block_ = count_ = 0;
    pixels_.clear();
    mask_.clear();
    opaque_.clear();
    if (scale < 1) return false;
    const int count = static_cast<int>(tiles.size());
    if (count == 0) return true;
    int art_size = 0;
    for (int i = 0; i < count; ++i) {
        if (tiles[i].sprite < 0 || tiles[i].sprite >= static_cast<int>(sprites.size())) return false;
        const SpriteData& sprite = sprites[tiles[i].sprite];
        if (i == 0) art_size = sprite.width;
        if (!gfx::spriteValid(sprite) || sprite.width != art_size || sprite.height != art_size)
            return false;
    }
    const int block = art_size * scale;
    const std::size_t per_tile = static_cast<std::size_t>(block) * block;
    pixels_.assign(per_tile * count, 0);
    mask_.assign(per_tile * count, 0);
    opaque_.assign(count, 1);
    for (int i = 0; i < count; ++i) {
        const SpriteData& sprite = sprites[tiles[i].sprite];
        gfx::Canvas canvas({pixels_.data() + i * per_tile, block, block, block});
        gfx::DrawOptions options;
        options.scale = scale;
        options.bevel = bevel && (tiles[i].flags & TILE_SOLID) != 0;
        gfx::drawSprite(canvas, sprite, palette, 0, 0, options);
        for (int y = 0; y < block; ++y) {
            for (int x = 0; x < block; ++x) {
                const bool solid = sprite.pixels[(y / scale) * art_size + x / scale] != 0;
                mask_[i * per_tile + y * block + x] = solid ? 1 : 0;
                if (!solid) opaque_[i] = 0;
            }
        }
    }
    block_ = block;
    count_ = count;
    return true;
}

void drawLayer(gfx::Canvas& canvas, const TileCache& cache, const Layer& layer,
               const gfx::Rect& viewport, int camera_x, int camera_y) {
    if (cache.count() == 0 || layer.cells == nullptr || layer.cols <= 0 || layer.rows <= 0) return;
    const int block = cache.block();
    const int offset_x = floorDiv(camera_x * layer.parallax, 256);
    const int offset_y = floorDiv(camera_y * layer.parallax, 256);

    const gfx::Rect previous = canvas.clipRect();
    canvas.setClip(viewport);

    int first_col = floorDiv(offset_x, block);
    int last_col = floorDiv(offset_x + viewport.w - 1, block);
    int first_row = floorDiv(offset_y, block);
    int last_row = floorDiv(offset_y + viewport.h - 1, block);
    if (first_col < 0) first_col = 0;
    if (first_row < 0) first_row = 0;
    if (last_col >= layer.cols) last_col = layer.cols - 1;
    if (last_row >= layer.rows) last_row = layer.rows - 1;

    for (int row = first_row; row <= last_row; ++row) {
        for (int col = first_col; col <= last_col; ++col) {
            const int cell = layer.cells[row * layer.cols + col];
            if (cell == 0 || cell > cache.count()) continue;
            canvas.blit(cache.pixels(cell - 1), cache.mask(cell - 1), block, block,
                        viewport.x + col * block - offset_x, viewport.y + row * block - offset_y);
        }
    }
    canvas.setClip(previous);
}

}  // namespace tg
