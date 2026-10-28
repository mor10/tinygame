#include <cassert>
#include "tg_random.h"

int main() {
    std::uint32_t seed = 1;
    assert(tg::randomNext(seed) == 270369);
    assert(tg::randomNext(seed) == 67634689);
    assert(tg::randomNext(seed) == 2647435461U);
    seed = 0;
    assert(tg::randomNext(seed) == 270369);
}
