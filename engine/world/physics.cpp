#include "physics.h"

#include <cmath>

#include "../game/game_data.h"

namespace tg {

namespace {

constexpr float COORD_LIMIT = 100000.0f;
constexpr float DT = 1.0f / 60.0f;

int floorInt(float v) { return static_cast<int>(std::floor(v)); }
int ceilInt(float v) { return static_cast<int>(std::ceil(v)); }

float clampTo(float v, float limit) {
    if (v != v) return 0.0f;
    return v < -limit ? -limit : (v > limit ? limit : v);
}

void sanitize(Actor& a) {
    a.x = clampTo(a.x, COORD_LIMIT);
    a.y = clampTo(a.y, COORD_LIMIT);
    a.vx = clampTo(a.vx, MAX_SPEED);
    a.vy = clampTo(a.vy, MAX_SPEED);
}

void moveX(Actor& a, const TileGrid& g, float dx) {
    if (dx == 0.0f) return;
    float nx = a.x + dx;
    const int r0 = floorInt(a.y), r1 = ceilInt(a.y + a.h) - 1;
    const int c0 = floorInt(nx), c1 = ceilInt(nx + a.w) - 1;
    bool blocked = false;
    if (dx > 0) {
        for (int c = c0; c <= c1 && !blocked; ++c)
            for (int r = r0; r <= r1 && !blocked; ++r)
                if (g.flagsAt(c, r) & TILE_SOLID) {
                    nx = static_cast<float>(c) - a.w;
                    if (nx < a.x) nx = a.x;
                    blocked = true;
                }
    } else {
        for (int c = c1; c >= c0 && !blocked; --c)
            for (int r = r0; r <= r1 && !blocked; ++r)
                if (g.flagsAt(c, r) & TILE_SOLID) {
                    nx = static_cast<float>(c + 1);
                    if (nx > a.x) nx = a.x;
                    blocked = true;
                }
    }
    a.x = nx;
    if (blocked) {
        a.vx = 0;
        a.flags |= ACTOR_HIT_WALL;
    }
}

void moveY(Actor& a, const TileGrid& g, float dy, float start_bottom) {
    if (dy == 0.0f) return;
    float ny = a.y + dy;
    const int c0 = floorInt(a.x), c1 = ceilInt(a.x + a.w) - 1;
    const int r0 = floorInt(ny), r1 = ceilInt(ny + a.h) - 1;
    bool blocked = false;
    if (dy > 0) {
        for (int r = r0; r <= r1 && !blocked; ++r)
            for (int c = c0; c <= c1 && !blocked; ++c) {
                const std::uint16_t f = g.flagsAt(c, r);
                // One-way: only if the feet were at or above the tile's top when the tick began.
                if ((f & TILE_SOLID) ||
                    ((f & TILE_ONE_WAY) && start_bottom <= static_cast<float>(r))) {
                    ny = static_cast<float>(r) - a.h;
                    if (ny < a.y) ny = a.y;
                    blocked = true;
                }
            }
        if (blocked) a.flags |= ACTOR_ON_GROUND;
    } else {
        for (int r = r1; r >= r0 && !blocked; --r)
            for (int c = c0; c <= c1 && !blocked; ++c)
                if (g.flagsAt(c, r) & TILE_SOLID) {
                    ny = static_cast<float>(r + 1);
                    if (ny > a.y) ny = a.y;
                    blocked = true;
                }
    }
    a.y = ny;
    if (blocked) a.vy = 0;
}

}  // namespace

std::uint16_t TileGrid::flagsAt(int col, int row) const {
    if (col < 0 || col >= cols || row < 0) return TILE_SOLID;
    if (row >= rows || cells == nullptr) return 0;
    const int cell = cells[row * cols + col];
    if (cell == 0 || cell > tile_count) return 0;
    return tile_flags[cell - 1];
}

void stepPlatformer(Actor& a, const TileGrid& g, float gravity) {
    sanitize(a);
    a.flags &= ~static_cast<std::uint32_t>(ACTOR_ON_GROUND | ACTOR_HIT_WALL);
    const float start_bottom = a.y + a.h;
    const float vy_before = a.vy;
    a.vy = clampTo(a.vy + gravity * DT, MAX_SPEED);
    // Y moves by the mean of the speed before and after this tick's gravity
    // (exact for constant gravity), so a jump reaches jump_height.
    const float dx = clampTo(a.vx * DT, MAX_STEP);
    const float dy = clampTo((vy_before + a.vy) * 0.5f * DT, MAX_STEP);
    moveX(a, g, dx);
    moveY(a, g, dy, start_bottom);
}

bool overlapsTileFlag(const TileGrid& g, const Actor& a, std::uint16_t flag) {
    const int c0 = floorInt(a.x), c1 = ceilInt(a.x + a.w) - 1;
    const int r0 = floorInt(a.y), r1 = ceilInt(a.y + a.h) - 1;
    for (int r = r0; r <= r1; ++r)
        for (int c = c0; c <= c1; ++c)
            if (g.flagsAt(c, r) & flag) return true;
    return false;
}

bool isSupported(const TileGrid& g, const Actor& a) {
    const float bottom = a.y + a.h;
    const float row = std::floor(bottom + 0.5f);
    if (std::fabs(bottom - row) > 1e-4f) return false;
    const int c0 = floorInt(a.x), c1 = ceilInt(a.x + a.w) - 1;
    for (int c = c0; c <= c1; ++c)
        if (g.flagsAt(c, static_cast<int>(row)) & (TILE_SOLID | TILE_ONE_WAY)) return true;
    return false;
}

bool actorsOverlap(const Actor& a, const Actor& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

bool hasFallenOut(const TileGrid& g, const Actor& a) {
    return a.y > static_cast<float>(g.rows) + FALL_OUT_ROWS;
}

}  // namespace tg
