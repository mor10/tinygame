#include <cassert>
#include <cstdint>
#include <cstdio>

#include "crc32.h"

int main() {
    constexpr std::uint8_t sample[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    assert(tg::engine::crc32(sample, sizeof(sample)) == 0xcbf43926U);
    assert(tg::engine::crc32(sample, 0) == 0U);
    std::puts("crc32 tests passed");
}
