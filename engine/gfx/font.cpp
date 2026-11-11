#include "font.h"

namespace tg::gfx {

namespace {
constexpr std::uint16_t bits(int a, int b, int c, int d, int e) {
    return static_cast<std::uint16_t>(a | (b << 3) | (c << 6) | (d << 9) | (e << 12));
}
}  // namespace

std::uint16_t glyph(char c) {
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    switch (c) {
        case 'A': return bits(2, 5, 7, 5, 5);
        case 'B': return bits(6, 5, 6, 5, 6);
        case 'C': return bits(3, 4, 4, 4, 3);
        case 'D': return bits(6, 5, 5, 5, 6);
        case 'E': return bits(7, 4, 6, 4, 7);
        case 'F': return bits(7, 4, 6, 4, 4);
        case 'G': return bits(3, 4, 5, 5, 3);
        case 'H': return bits(5, 5, 7, 5, 5);
        case 'I': return bits(7, 2, 2, 2, 7);
        case 'J': return bits(1, 1, 1, 5, 2);
        case 'K': return bits(5, 5, 6, 5, 5);
        case 'L': return bits(4, 4, 4, 4, 7);
        case 'M': return bits(5, 7, 7, 5, 5);
        case 'N': return bits(5, 7, 7, 7, 5);
        case 'O': return bits(2, 5, 5, 5, 2);
        case 'P': return bits(6, 5, 6, 4, 4);
        case 'Q': return bits(2, 5, 5, 3, 1);
        case 'R': return bits(6, 5, 6, 5, 5);
        case 'S': return bits(3, 4, 2, 1, 6);
        case 'T': return bits(7, 2, 2, 2, 2);
        case 'U': return bits(5, 5, 5, 5, 7);
        case 'V': return bits(5, 5, 5, 5, 2);
        case 'W': return bits(5, 5, 7, 7, 5);
        case 'X': return bits(5, 5, 2, 5, 5);
        case 'Y': return bits(5, 5, 2, 2, 2);
        case 'Z': return bits(7, 1, 2, 4, 7);
        case '0': return bits(7, 5, 5, 5, 7);
        case '1': return bits(2, 6, 2, 2, 7);
        case '2': return bits(6, 1, 2, 4, 7);
        case '3': return bits(6, 1, 2, 1, 6);
        case '4': return bits(5, 5, 7, 1, 1);
        case '5': return bits(7, 4, 6, 1, 6);
        case '6': return bits(3, 4, 7, 5, 7);
        case '7': return bits(7, 1, 2, 2, 2);
        case '8': return bits(7, 5, 7, 5, 7);
        case '9': return bits(7, 5, 7, 1, 6);
        case ':': return bits(0, 2, 0, 2, 0);
        case '!': return bits(2, 2, 2, 0, 2);
        case '.': return bits(0, 0, 0, 0, 2);
        case '-': return bits(0, 0, 7, 0, 0);
        case '/': return bits(1, 1, 2, 4, 4);
        case '+': return bits(0, 2, 7, 2, 0);
        default: return 0;
    }
}

int textWidth(const char* text, int scale) {
    int count = 0;
    while (text[count] != '\0') ++count;
    return count == 0 ? 0 : (count * GLYPH_ADVANCE - 1) * scale;
}

void drawText(Canvas& canvas, int x, int y, const char* text, Color color, int scale) {
    if (scale < 1) scale = 1;
    for (int i = 0; text[i] != '\0'; ++i) {
        const std::uint16_t g = glyph(text[i]);
        for (int row = 0; row < GLYPH_H; ++row) {
            for (int col = 0; col < GLYPH_W; ++col) {
                if (g & (1U << (row * 3 + 2 - col))) {
                    canvas.rect(x + (i * GLYPH_ADVANCE + col) * scale, y + row * scale, scale, scale,
                                color);
                }
            }
        }
    }
}

}  // namespace tg::gfx
