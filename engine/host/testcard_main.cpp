// Renders every test-card variant for both boards to raw RGB565 files and
// prints one manifest line per file: <name> <width> <height>.
// `tools/tg.py testcard` turns them into PNGs.

#include <cstdio>
#include <string>
#include <vector>

#include "../testcard/testcard.h"

namespace {
struct Board {
    const char* id;
    int width;
    int height;
};
constexpr Board BOARDS[] = {{"a", 320, 172}, {"b", 280, 240}};
}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: testcard_host <output-directory>\n");
        return 2;
    }
    const std::string out = argv[1];
    for (const Board& board : BOARDS) {
        std::vector<tg::gfx::Color> pixels(static_cast<std::size_t>(board.width) * board.height);
        tg::gfx::Canvas canvas({pixels.data(), board.width, board.height, board.width});
        for (int pal = 0; pal < tg::testcard::paletteCount(); ++pal) {
            for (int scale = 1; scale <= 3; ++scale) {
                for (int bevel = 0; bevel <= 1; ++bevel) {
                    tg::testcard::Options options;
                    options.scale = scale;
                    options.bevel = bevel != 0;
                    options.palette = pal;
                    tg::testcard::render(canvas, options);
                    char name[64];
                    std::snprintf(name, sizeof(name), "%s_p%d_s%d_%s", board.id, pal, scale,
                                  bevel ? "bevel" : "flat");
                    const std::string path = out + "/" + name + ".rgb565";
                    std::FILE* file = std::fopen(path.c_str(), "wb");
                    if (file == nullptr) {
                        std::fprintf(stderr, "cannot write %s\n", path.c_str());
                        return 1;
                    }
                    const std::size_t written =
                        std::fwrite(pixels.data(), sizeof(tg::gfx::Color), pixels.size(), file);
                    if (std::fclose(file) != 0 || written != pixels.size()) {
                        std::fprintf(stderr, "short write to %s\n", path.c_str());
                        return 1;
                    }
                    std::printf("%s %d %d\n", name, board.width, board.height);
                }
            }
        }
    }
    return 0;
}
