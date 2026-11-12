#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "crc32.h"
#include "transfer_codec.h"

int main() {
    std::uint32_t number = 0;
    assert(tg::engine::transfer::parseNumber("4294967295", number, 10));
    assert(number == UINT32_MAX);
    assert(tg::engine::transfer::parseNumber("cbf43926", number, 16));
    assert(number == 0xcbf43926U);
    assert(!tg::engine::transfer::parseNumber("4294967296", number, 10));
    assert(!tg::engine::transfer::parseNumber("-1", number, 10));
    assert(!tg::engine::transfer::parseNumber("1x", number, 16));

    std::uint8_t decoded[tg::engine::transfer::CHUNK_BYTES] = {};
    std::size_t size = 0;
    assert(tg::engine::transfer::decodeBase64("TQ==", decoded, sizeof(decoded), size));
    assert(size == 1 && decoded[0] == 'M');
    assert(tg::engine::transfer::decodeBase64("TWE=", decoded, sizeof(decoded), size));
    assert(size == 2 && decoded[0] == 'M' && decoded[1] == 'a');
    assert(tg::engine::transfer::decodeBase64("TWFu", decoded, sizeof(decoded), size));
    assert(size == 3 && std::memcmp(decoded, "Man", 3) == 0);

    const char* invalid[] = {"", "TQ=", "TQ==AAAA", "TQ=A", "TQ== ", "!!!!"};
    for (const char* encoded : invalid) {
        assert(!tg::engine::transfer::decodeBase64(
            encoded, decoded, sizeof(decoded), size));
    }
    assert(!tg::engine::transfer::decodeBase64("TWFu", decoded, 2, size));
    assert(tg::engine::crc32(decoded, 3) == 0x51efd138U);
    std::puts("transfer codec tests passed");
}
