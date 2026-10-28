#include "level.h"

#include "config.h"

namespace brickbreaker::level {

namespace {

using config::BRICK_COLUMNS;
using config::BRICK_ROWS;

static_assert(BRICK_ROWS == 5 && BRICK_COLUMNS == 10,
              "Level layouts assume a 5 x 10 brick grid");

struct Layout {
    // Each row is a 10-bit column mask; bit 0 is the leftmost column.
    std::uint16_t present[BRICK_ROWS];
    // Subset of `present` that needs two hits.
    std::uint16_t tough[BRICK_ROWS];
};

constexpr Layout LAYOUTS[LEVEL_COUNT] = {
    // 1: full wall
    {{0x3FF, 0x3FF, 0x3FF, 0x3FF, 0x3FF}, {0, 0, 0, 0, 0}},
    // 2: pyramid
    {{0x030, 0x078, 0x0FC, 0x1FE, 0x3FF}, {0, 0, 0, 0, 0}},
    // 3: checkerboard
    {{0x155, 0x2AA, 0x155, 0x2AA, 0x155}, {0, 0, 0, 0, 0}},
    // 4: stripes with an armoured top row
    {{0x3FF, 0x000, 0x3FF, 0x000, 0x3FF}, {0x3FF, 0, 0, 0, 0}},
    // 5: diamond with a tough core
    {{0x030, 0x0FC, 0x3FF, 0x0FC, 0x030}, {0, 0x030, 0x078, 0x030, 0}},
    // 6: fortress with tough walls
    {{0x3FF, 0x201, 0x2FD, 0x201, 0x3FF}, {0x3FF, 0x201, 0x201, 0x201, 0}},
};

constexpr bool layoutsValid() {
    for (const Layout& layout : LAYOUTS) {
        bool any = false;
        for (std::uint8_t row = 0; row < BRICK_ROWS; ++row) {
            if ((layout.tough[row] & ~layout.present[row]) != 0 ||
                layout.present[row] >= (1U << BRICK_COLUMNS)) {
                return false;
            }
            any = any || layout.present[row] != 0;
        }
        if (!any) {
            return false;
        }
    }
    return true;
}
static_assert(layoutsValid(),
              "Every layout needs bricks; tough bricks must be present bricks");

}  // namespace

std::uint8_t brickHits(std::uint32_t level_index, std::uint8_t row,
                       std::uint8_t column) {
    if (row >= BRICK_ROWS || column >= BRICK_COLUMNS) {
        return 0;
    }
    const Layout& layout = LAYOUTS[level_index % LEVEL_COUNT];
    const std::uint16_t bit = 1U << column;
    if ((layout.present[row] & bit) == 0) {
        return 0;
    }
    return (layout.tough[row] & bit) != 0 ? 2 : 1;
}

}  // namespace brickbreaker::level
