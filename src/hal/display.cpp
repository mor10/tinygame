#include "display.h"

#include <Arduino.h>
#include <SPI.h>
#include <lvgl.h>
#include "driver/gpio.h"

#include "board_config.h"
#include "display_config.h"
#include "tg_board.h"

namespace {

constexpr std::uint32_t BACKLIGHT_PWM_FREQUENCY_HZ = 1000;
constexpr std::uint8_t BACKLIGHT_PWM_RESOLUTION_BITS = 10;
constexpr std::uint8_t BACKLIGHT_PWM_CHANNEL = 0;
constexpr std::uint8_t BOOT_BRIGHTNESS_PERCENT = 90;

SPIClass display_spi(FSPI);
lv_display_t* lv_display = nullptr;
alignas(4) std::uint8_t draw_buffer[display_config::WIDTH *
                                    display_config::DRAW_BUFFER_LINES *
                                    sizeof(std::uint16_t)];

void writeCommand(std::uint8_t command, const std::uint8_t* data = nullptr,
                  std::size_t data_size = 0) {
    display_spi.beginTransaction(
        SPISettings(display_config::SPI_FREQUENCY_HZ, MSBFIRST,
                    tg::board::LCD_169 ? SPI_MODE3 : SPI_MODE0));
    digitalWrite(board::PIN_LCD_CS, LOW);
    digitalWrite(board::PIN_LCD_DC, LOW);
    display_spi.transfer(command);

    if (data_size != 0) {
        digitalWrite(board::PIN_LCD_DC, HIGH);
        display_spi.writeBytes(const_cast<std::uint8_t*>(data), data_size);
    }

    digitalWrite(board::PIN_LCD_CS, HIGH);
    display_spi.endTransaction();
}

void resetPanel() {
    digitalWrite(board::PIN_LCD_CS, LOW);
    delay(50);
    digitalWrite(board::PIN_LCD_RESET, LOW);
    delay(50);
    digitalWrite(board::PIN_LCD_RESET, HIGH);
    delay(50);
    digitalWrite(board::PIN_LCD_CS, HIGH);
}

void initializePanel() {
    writeCommand(0x11);
    delay(120);

    const std::uint8_t memory_access_control[] = {display_config::MADCTL};
    writeCommand(0x36, memory_access_control, sizeof(memory_access_control));

    const std::uint8_t pixel_format[] = {tg::board::LCD_169 ? std::uint8_t{0x55}
                                                            : std::uint8_t{0x05}};
    writeCommand(0x3A, pixel_format, sizeof(pixel_format));

    const std::uint8_t b0[] = {0x00, tg::board::LCD_169 ? std::uint8_t{0xF8}
                                                        : std::uint8_t{0xE8}};
    writeCommand(0xB0, b0, sizeof(b0));

    const std::uint8_t b2[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
    writeCommand(0xB2, b2, sizeof(b2));

    const std::uint8_t b7[] = {0x35};
    writeCommand(0xB7, b7, sizeof(b7));

    const std::uint8_t bb[] = {tg::board::LCD_169 ? std::uint8_t{0x19}
                                                  : std::uint8_t{0x35}};
    writeCommand(0xBB, bb, sizeof(bb));

    const std::uint8_t c0[] = {0x2C};
    writeCommand(0xC0, c0, sizeof(c0));

    const std::uint8_t c2[] = {0x01};
    writeCommand(0xC2, c2, sizeof(c2));

    const std::uint8_t c3[] = {tg::board::LCD_169 ? std::uint8_t{0x12}
                                                  : std::uint8_t{0x13}};
    writeCommand(0xC3, c3, sizeof(c3));

    const std::uint8_t c4[] = {0x20};
    writeCommand(0xC4, c4, sizeof(c4));

    const std::uint8_t c6[] = {0x0F};
    writeCommand(0xC6, c6, sizeof(c6));

    const std::uint8_t d0[] = {0xA4, 0xA1};
    writeCommand(0xD0, d0, sizeof(d0));

    const std::uint8_t d6[] = {0xA1};
    if constexpr (!tg::board::LCD_169) writeCommand(0xD6, d6, sizeof(d6));

    const std::uint8_t e0_147[] = {0xF0, 0x00, 0x04, 0x04, 0x04, 0x05, 0x29,
                                   0x33, 0x3E, 0x38, 0x12, 0x12, 0x28, 0x30};
    const std::uint8_t e0_169[] = {0xF0, 0x09, 0x13, 0x12, 0x12, 0x2B, 0x3C,
                                   0x44, 0x4B, 0x1B, 0x18, 0x17, 0x1D, 0x21};
    writeCommand(0xE0, tg::board::LCD_169 ? e0_169 : e0_147, sizeof(e0_147));

    const std::uint8_t e1_147[] = {0xF0, 0x07, 0x0A, 0x0D, 0x0B, 0x07, 0x28,
                                   0x33, 0x3E, 0x36, 0x14, 0x14, 0x29, 0x32};
    const std::uint8_t e1_169[] = {0xF0, 0x09, 0x13, 0x0C, 0x0D, 0x27, 0x3B,
                                   0x44, 0x4D, 0x0B, 0x17, 0x17, 0x1D, 0x21};
    writeCommand(0xE1, tg::board::LCD_169 ? e1_169 : e1_147, sizeof(e1_147));

    writeCommand(0x21);
    if constexpr (tg::board::LCD_169) {
        writeCommand(0x13);
        delay(10);
    }
    writeCommand(0x11);
    delay(120);
    writeCommand(0x29);
}

void setAddressWindow(const lv_area_t* area) {
    const auto window = display_config::window(area->x1, area->y1, area->x2, area->y2);

    const std::uint8_t column_address[] = {
        static_cast<std::uint8_t>(window.x1 >> 8), static_cast<std::uint8_t>(window.x1),
        static_cast<std::uint8_t>(window.x2 >> 8), static_cast<std::uint8_t>(window.x2)};
    writeCommand(0x2A, column_address, sizeof(column_address));

    const std::uint8_t row_address[] = {
        static_cast<std::uint8_t>(window.y1 >> 8), static_cast<std::uint8_t>(window.y1),
        static_cast<std::uint8_t>(window.y2 >> 8), static_cast<std::uint8_t>(window.y2)};
    writeCommand(0x2B, row_address, sizeof(row_address));
    writeCommand(0x2C);
}

void flush(lv_display_t* display, const lv_area_t* area, std::uint8_t* pixels) {
    setAddressWindow(area);

    const std::uint32_t width =
        static_cast<std::uint32_t>(area->x2 - area->x1 + 1);
    const std::uint32_t height =
        static_cast<std::uint32_t>(area->y2 - area->y1 + 1);
    const std::uint32_t pixel_bytes =
        width * height * sizeof(std::uint16_t);

    display_spi.beginTransaction(
        SPISettings(display_config::SPI_FREQUENCY_HZ, MSBFIRST,
                    tg::board::LCD_169 ? SPI_MODE3 : SPI_MODE0));
    digitalWrite(board::PIN_LCD_CS, LOW);
    digitalWrite(board::PIN_LCD_DC, HIGH);
    const auto stride = lv_draw_buf_width_to_stride(width, LV_COLOR_FORMAT_RGB565);
    if (stride == width * sizeof(std::uint16_t)) {
        display_spi.writeBytes(pixels, pixel_bytes);
    } else {
        for (std::uint32_t y = 0; y < height; ++y) {
            display_spi.writeBytes(pixels + y * stride, width * sizeof(std::uint16_t));
        }
    }
    digitalWrite(board::PIN_LCD_CS, HIGH);
    display_spi.endTransaction();

    lv_display_flush_ready(display);
}

std::uint32_t getTickMilliseconds() {
    return millis();
}

// Platform espressif32 6.x ships Arduino core 2.x, which uses LEDC channels.
// Waveshare demo convention: duty = level (0-100) * 10 on a 10-bit channel.
bool writeBacklight(std::uint8_t percent) {
    if (percent > 100) {
        percent = 100;
    }
    const std::uint16_t duty = static_cast<std::uint16_t>(percent) * 10U;
    ledcWrite(BACKLIGHT_PWM_CHANNEL, duty);
    return true;
}

bool initializeBacklight() {
    if (ledcSetup(BACKLIGHT_PWM_CHANNEL, BACKLIGHT_PWM_FREQUENCY_HZ,
                  BACKLIGHT_PWM_RESOLUTION_BITS) == 0) {
        Serial.println("[LCD] Failed to initialize backlight PWM");
        return false;
    }
    ledcAttachPin(board::PIN_LCD_BACKLIGHT, BACKLIGHT_PWM_CHANNEL);
    return writeBacklight(BOOT_BRIGHTNESS_PERCENT);
}

}  // namespace

namespace display {

bool prepareSleep() {
    writeBacklight(0);
    ledcDetachPin(board::PIN_LCD_BACKLIGHT);
    pinMode(board::PIN_LCD_BACKLIGHT, OUTPUT);
    digitalWrite(board::PIN_LCD_BACKLIGHT, LOW);
    const auto result = gpio_hold_en(static_cast<gpio_num_t>(board::PIN_LCD_BACKLIGHT));
    if (result != ESP_OK) {
        Serial.printf("[LCD] backlight hold failed: %s\n", esp_err_to_name(result));
        ledcAttachPin(board::PIN_LCD_BACKLIGHT, BACKLIGHT_PWM_CHANNEL);
        return false;
    }
    writeCommand(0x28);
    writeCommand(0x10);
    return true;
}

void setBrightness(std::uint8_t percent) {
    writeBacklight(percent);
}

bool initialize() {
    pinMode(board::PIN_LCD_CS, OUTPUT);
    pinMode(board::PIN_LCD_DC, OUTPUT);
    pinMode(board::PIN_LCD_RESET, OUTPUT);

    if (!initializeBacklight()) {
        return false;
    }

    display_spi.begin(board::PIN_LCD_SCLK, -1, board::PIN_LCD_MOSI);
    resetPanel();
    initializePanel();

    lv_init();
    lv_tick_set_cb(getTickMilliseconds);

    lv_display = lv_display_create(display_config::WIDTH, display_config::HEIGHT);
    if (lv_display == nullptr) {
        Serial.println("[LVGL] Failed to create display");
        return false;
    }

    lv_display_set_color_format(lv_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(lv_display, flush);
    lv_display_set_buffers(lv_display, draw_buffer, nullptr, sizeof(draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    Serial.println("[LCD] Waveshare ST7789 initialized");
    return true;
}

lv_display_t* lvDisplay() {
    return lv_display;
}

}  // namespace display
