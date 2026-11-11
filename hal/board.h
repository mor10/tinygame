#pragma once

// Selects the board at build time. Each PlatformIO environment defines exactly
// one TG_BOARD_* macro; drivers include this file and never a board folder
// directly.
//
// Every board folder provides:
//   board_config.h    namespace board: pins, NAME, and the capability flags
//                     has_rgb_led, has_buzzer, has_touch
//   display_config.h  namespace display_config: WIDTH, HEIGHT, offsets, SPI speed
//
// To add Board B: create hal/boards/board_b/ with those two files, add an
// #elif below, and add [env:board_b] with -DTG_BOARD_B to platformio.ini.
// See docs/hardware/board-b-open-questions.md first: its pins are not known yet.

#if defined(TG_BOARD_A)
#include "boards/board_a/board_config.h"
#include "boards/board_a/display_config.h"
#else
#error "No board selected. Build with a PlatformIO environment that defines TG_BOARD_A."
#endif

// A board that leaves out a capability flag fails here, not deep in a driver.
static_assert(board::has_rgb_led || !board::has_rgb_led, "board_config.h must define has_rgb_led");
static_assert(board::has_buzzer || !board::has_buzzer, "board_config.h must define has_buzzer");
static_assert(board::has_touch || !board::has_touch, "board_config.h must define has_touch");
