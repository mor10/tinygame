#include <cassert>

#include "board_config.h"
#include "display_config.h"
#include "input_logic.h"
#include "tg_board.h"

int main() {
    static_assert(display_config::WIDTH == tg::board::WIDTH);
    static_assert(display_config::HEIGHT == tg::board::HEIGHT);
    static_assert(display_config::WIDTH == board::LCD_PHYSICAL_HEIGHT);
    static_assert(display_config::HEIGHT == board::LCD_PHYSICAL_WIDTH);

    constexpr int pins[] = {
        board::PIN_LCD_MOSI, board::PIN_LCD_SCLK, board::PIN_LCD_CS,
        board::PIN_LCD_DC, board::PIN_LCD_RESET, board::PIN_LCD_BACKLIGHT,
        board::PIN_LEFT_A, board::PIN_LEFT_B, board::PIN_LEFT_BUTTON,
        board::PIN_RIGHT_A, board::PIN_RIGHT_B, board::PIN_RIGHT_BUTTON,
        board::PIN_IMU_SDA, board::PIN_IMU_SCL,
    };
    for (unsigned i = 0; i < sizeof(pins) / sizeof(pins[0]); ++i) {
        assert(pins[i] >= 0 && pins[i] <= 48);
        for (unsigned j = 0; j < i; ++j) assert(pins[i] != pins[j]);
    }

    assert(input::gpioLevelFromRegisters(1U << 4, 1U << 11, 4));
    assert(input::gpioLevelFromRegisters(1U << 4, 1U << 11, 43));
    assert(!input::gpioLevelFromRegisters(0, 0, 43));
    assert(!input::gpioLevelFromRegisters(0, 0, 64));
    constexpr float chip_axes[3] = {2.0f, 3.0f, 5.0f};
    assert(board::axis(chip_axes, board::SCREEN_X) ==
           (tg::board::LCD_169 ? 2.0f : 3.0f));
    assert(board::axis(chip_axes, board::SCREEN_Y) ==
           (tg::board::LCD_169 ? 3.0f : 2.0f));
    assert(board::axis(chip_axes, board::SCREEN_Z) == 5.0f);

    constexpr auto full = display_config::window(
        0, 0, display_config::WIDTH - 1, display_config::HEIGHT - 1);
    assert(full.x1 == display_config::COLUMN_OFFSET);
    assert(full.y1 == display_config::ROW_OFFSET);
    assert(full.x2 - full.x1 + 1 == display_config::WIDTH);
    assert(full.y2 - full.y1 + 1 == display_config::HEIGHT);
    assert(display_config::WIDTH + display_config::COLUMN_OFFSET <= 320);
    assert(display_config::HEIGHT + display_config::ROW_OFFSET <= 240);

    if constexpr (tg::board::LCD_169) {
        assert(display_config::WIDTH == 280 && display_config::HEIGHT == 240);
        assert(board::PIN_LEFT_A == 43 && board::PIN_LEFT_B == 2);
        assert(board::PIN_LEFT_BUTTON == 3 && board::PIN_RIGHT_BUTTON == 18);
        assert(board::PIN_IMU_SDA == 11 && board::PIN_IMU_SCL == 10);
        assert(board::LEFT_DIRECTION == -1 && board::RIGHT_DIRECTION == -1);
        assert(board::PIN_RGB_LED == -1 && !tg::board::HAS_RGB_LED);
        assert(display_config::MADCTL == 0xA0);
        assert(display_config::COLUMN_OFFSET == 20 && display_config::ROW_OFFSET == 0);
    } else {
        assert(display_config::WIDTH == 320 && display_config::HEIGHT == 172);
        assert(board::PIN_LEFT_A == 9 && board::PIN_RIGHT_A == 4);
        assert(board::PIN_IMU_SDA == 48 && board::PIN_IMU_SCL == 47);
        assert(board::LEFT_DIRECTION == 1 && board::RIGHT_DIRECTION == 1);
        assert(board::PIN_RGB_LED == 38 && tg::board::HAS_RGB_LED);
        assert(display_config::MADCTL == 0x70);
        assert(display_config::COLUMN_OFFSET == 0 && display_config::ROW_OFFSET == 34);
    }
}
