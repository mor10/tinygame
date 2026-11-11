// Runs the scrolling demo headlessly, far faster than real time, and writes
// each golden-case frame as raw RGB565. Prints: <name> <width> <height> <hash>.

#include <cinttypes>
#include <cstdio>
#include <string>
#include <vector>

#include "../core/hash.h"
#include "../demo/scroll_cases.h"
#include "../demo/scroll_demo.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: demo_host <output-directory>\n");
        return 2;
    }
    const std::string out = argv[1];
    for (const tg::demo::ScrollCase& c : tg::demo::SCROLL_CASES) {
        std::vector<tg::gfx::Color> pixels(static_cast<std::size_t>(c.width) * c.height);
        tg::gfx::Canvas canvas({pixels.data(), c.width, c.height, c.width});
        tg::demo::ScrollDemo demo;
        if (!demo.init(c.width, c.height, c.view, 1)) {
            std::fprintf(stderr, "%s: demo failed to start\n", c.name);
            return 1;
        }
        for (int i = 0; i < c.tick; ++i) demo.tick();
        demo.draw(canvas);
        const std::string path = out + "/" + c.name + ".rgb565";
        std::FILE* file = std::fopen(path.c_str(), "wb");
        if (file == nullptr) {
            std::fprintf(stderr, "cannot write %s\n", path.c_str());
            return 1;
        }
        const std::size_t written = std::fwrite(pixels.data(), sizeof(tg::gfx::Color), pixels.size(), file);
        if (std::fclose(file) != 0 || written != pixels.size()) {
            std::fprintf(stderr, "short write to %s\n", path.c_str());
            return 1;
        }
        std::printf("%s %d %d %016" PRIx64 "\n", c.name, c.width, c.height,
                    tg::hashPixels(pixels.data(), pixels.size()));
    }
    return 0;
}
