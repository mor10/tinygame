#include "display.h"

#include <Arduino.h>
#include <SPI.h>
#include <lvgl.h>
#include "driver/gpio.h"

#include "board_config.h"
#include "display_config.h"

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
        SPISettings(display_config::SPI_FREQUENCY_HZ, MSBFIRST, SPI_MODE0));
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

    const std::uint8_t memory_access_control[] = {
        display_config::HORIZONTAL ? 0x00 : 0x70};
    writeCommand(0x36, memory_access_control, sizeof(memory_access_control));

    const std::uint8_t pixel_format[] = {0x05};
    writeCommand(0x3A, pixel_format, sizeof(pixel_format));

    const std::uint8_t b0[] = {0x00, 0xE8};
    writeCommand(0xB0, b0, sizeof(b0));

    const std::uint8_t b2[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
    writeCommand(0xB2, b2, sizeof(b2));

    const std::uint8_t b7[] = {0x35};
    writeCommand(0xB7, b7, sizeof(b7));

    const std::uint8_t bb[] = {0x35};
    writeCommand(0xBB, bb, sizeof(bb));

    const std::uint8_t c0[] = {0x2C};
    writeCommand(0xC0, c0, sizeof(c0));

    const std::uint8_t c2[] = {0x01};
    writeCommand(0xC2, c2, sizeof(c2));

    const std::uint8_t c3[] = {0x13};
    writeCommand(0xC3, c3, sizeof(c3));

    const std::uint8_t c4[] = {0x20};
    writeCommand(0xC4, c4, sizeof(c4));

    const std::uint8_t c6[] = {0x0F};
    writeCommand(0xC6, c6, sizeof(c6));

    const std::uint8_t d0[] = {0xA4, 0xA1};
    writeCommand(0xD0, d0, sizeof(d0));

    const std::uint8_t d6[] = {0xA1};
    writeCommand(0xD6, d6, sizeof(d6));

    const std::uint8_t e0[] = {0xF0, 0x00, 0x04, 0x04, 0x04, 0x05, 0x29,
                               0x33, 0x3E, 0x38, 0x12, 0x12, 0x28, 0x30};
    writeCommand(0xE0, e0, sizeof(e0));

    const std::uint8_t e1[] = {0xF0, 0x07, 0x0A, 0x0D, 0x0B, 0x07, 0x28,
                               0x33, 0x3E, 0x36, 0x14, 0x14, 0x29, 0x32};
    writeCommand(0xE1, e1, sizeof(e1));

    writeCommand(0x21);
    writeCommand(0x11);
    delay(120);
    writeCommand(0x29);
}

void setAddressWindow(const lv_area_t* area) {
    // The panel's 34 px offset lies on the physical 172 px axis. In landscape,
    // MADCTL MV=1 already swaps axes in the controller, so CASET takes logical
    // x and RASET takes logical y; the offset therefore moves to the row axis.
    // (The Waveshare demo's unused !HORIZONTAL path swaps x/y a second time.)
    const std::uint16_t column_offset = display_config::HORIZONTAL
                                            ? display_config::OFFSET_X
                                            : display_config::OFFSET_Y;
    const std::uint16_t row_offset = display_config::HORIZONTAL
                                         ? display_config::OFFSET_Y
                                         : display_config::OFFSET_X;
    const std::uint16_t x1 = static_cast<std::uint16_t>(area->x1) + column_offset;
    const std::uint16_t x2 = static_cast<std::uint16_t>(area->x2) + column_offset;
    const std::uint16_t y1 = static_cast<std::uint16_t>(area->y1) + row_offset;
    const std::uint16_t y2 = static_cast<std::uint16_t>(area->y2) + row_offset;

    const std::uint8_t column_address[] = {
        static_cast<std::uint8_t>(x1 >> 8), static_cast<std::uint8_t>(x1),
        static_cast<std::uint8_t>(x2 >> 8), static_cast<std::uint8_t>(x2)};
    writeCommand(0x2A, column_address, sizeof(column_address));

    const std::uint8_t row_address[] = {
        static_cast<std::uint8_t>(y1 >> 8), static_cast<std::uint8_t>(y1),
        static_cast<std::uint8_t>(y2 >> 8), static_cast<std::uint8_t>(y2)};
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
        SPISettings(display_config::SPI_FREQUENCY_HZ, MSBFIRST, SPI_MODE0));
    digitalWrite(board::PIN_LCD_CS, LOW);
    digitalWrite(board::PIN_LCD_DC, HIGH);
    display_spi.writeBytes(pixels, pixel_bytes);
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
