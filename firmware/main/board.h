// Board profile "147b" — mirrors hardware.yaml (Waveshare ESP32-S3-LCD-1.47B).
// Keep in sync by hand for now; a generator from hardware.yaml comes later.
#pragma once

#define BOARD_NAME            "147b"

// Logical (game) screen after the 90-degree clockwise mounting
#define LOGICAL_W             320
#define LOGICAL_H             172

// Display SPI
#define BOARD_LCD_HOST        SPI2_HOST
#define BOARD_LCD_MOSI        45
#define BOARD_LCD_SCLK        40
#define BOARD_LCD_CS          42
#define BOARD_LCD_DC          41
#define BOARD_LCD_RST         39
#define BOARD_LCD_BL          46
#define BOARD_LCD_SPI_MODE    0
#define BOARD_LCD_INVERT      1

// Mode A: controller rotation (what the earlier experiments used)
#define BOARD_HW_MADCTL       0x70
#define BOARD_HW_COL_OFF      0
#define BOARD_HW_ROW_OFF      34

// Mode B: native portrait scan, rotation done in software
#define BOARD_SW_MADCTL       0x00
#define BOARD_SW_COL_OFF      34
#define BOARD_SW_ROW_OFF      0

// Backlight PWM: 20 kHz so phone slow-motion video doesn't band
#define BOARD_VENDOR_INIT     1   // 0 = minimal init (power-on panel defaults)
#define BOARD_BL_PWM_HZ       20000

// EC11 encoders (internal pull-ups; A, B and click switch to GND)
#define BOARD_ENC_L_A         9
#define BOARD_ENC_L_B         8
#define BOARD_ENC_L_CLICK     7
#define BOARD_ENC_R_A         4
#define BOARD_ENC_R_B         5
#define BOARD_ENC_R_CLICK     6
#define BOARD_ENC_L_DIR       1   // +1 = clockwise counts up; flip if a knob reads backwards
#define BOARD_ENC_R_DIR       1

// BOOT button doubles as "next page" in the lab
#define BOARD_BOOT_BTN        0
