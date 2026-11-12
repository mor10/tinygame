#pragma once

#ifndef TG_BOARD
#define TG_BOARD 147
#endif

#if TG_BOARD != 147 && TG_BOARD != 169
#error "Unsupported TG_BOARD (expected 147 or 169)"
#endif

namespace tg::board {

inline constexpr bool LCD_169 = TG_BOARD == 169;
inline constexpr const char* ID = LCD_169 ? "169" : "147b";
inline constexpr int WIDTH = LCD_169 ? 280 : 320;
inline constexpr int HEIGHT = LCD_169 ? 240 : 172;
inline constexpr bool HAS_RGB_LED = !LCD_169;
inline constexpr int SAFE_X = LCD_169 ? 20 : 8;
inline constexpr int SAFE_TOP = LCD_169 ? 12 : 0;
inline constexpr int SAFE_BOTTOM = LCD_169 ? 12 : 0;

}  // namespace tg::board
