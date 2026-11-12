#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

#include "lua_runtime.h"

using tg::engine::LuaRuntime;

struct ModuleCalls {
    unsigned helper = 0;
};

bool resolveHelper(void* context, const char* name, std::size_t name_size,
                   tg::engine::LuaModuleSource& module) {
    if (name_size != 6 || std::strcmp(name, "helper") != 0) return false;
    static constexpr char source[] =
        "module_loads = (module_loads or 0) + 1\nreturn { value = 17 }";
    ++static_cast<ModuleCalls*>(context)->helper;
    module = {source, sizeof(source) - 1};
    return true;
}

void runsOnlyTheAuditedSandboxLibraries() {
    LuaRuntime runtime;
    const char source[] =
        "assert(io == nil and os == nil and debug == nil and package == nil)\n"
        "assert(require == nil and load == nil and dofile == nil)\n"
        "assert(pcall == nil and xpcall == nil and pairs == nil and next == nil)\n"
        "assert(math.randomseed == nil and string.find == nil and string.format == nil)\n"
        "assert(string.dump == nil and string.pack == nil and string.unpack == nil "
        "and string.packsize == nil and table.sort == nil)\n"
        "function update(dt) assert(dt == 0.5) end\n";
    assert(runtime.load(source, sizeof(source) - 1));
    assert(runtime.call("update", 0.5f));
    assert(runtime.heapUsed() <= LuaRuntime::MAX_HEAP_BYTES);
}

void bundleModulesAreCachedAndPathRestricted() {
    ModuleCalls calls;
    LuaRuntime runtime;
    const char source[] =
        "local first = require('helper')\n"
        "local second = require('helper')\n"
        "function update() assert(first == second and first.value == 17 and module_loads == 1) end";
    assert(runtime.load(source, sizeof(source) - 1, "rules.lua", resolveHelper, &calls));
    assert(runtime.call("update"));
    assert(calls.helper == 1);

    LuaRuntime traversal;
    const char escaping[] = "require('../rules')";
    assert(!traversal.load(escaping, sizeof(escaping) - 1,
                           "rules.lua", resolveHelper, &calls));
    assert(std::strstr(traversal.error(), "invalid bundle module name") != nullptr);

    LuaRuntime missing;
    const char absent[] = "require('os')";
    assert(!missing.load(absent, sizeof(absent) - 1,
                         "rules.lua", resolveHelper, &calls));
    assert(std::strstr(missing.error(), "bundle module not found") != nullptr);
}

void callbackInstructionBudgetFaultsTheRuntime() {
    LuaRuntime runtime;
    const char source[] = "function update(dt) while true do end end";
    assert(runtime.load(source, sizeof(source) - 1));
    assert(!runtime.call("update", 1.0f / 60.0f));
    assert(std::strstr(runtime.error(), "instruction limit") != nullptr);
    assert(!runtime.call("update", 1.0f / 60.0f));
    assert(std::strstr(runtime.error(), "instruction limit") != nullptr);
}

void randomRejectsRangesWiderThanItsDeterministicGenerator() {
    LuaRuntime runtime;
    const char source[] =
        "function update(dt) math.random(-4294967296, 4294967296) end";
    assert(runtime.load(source, sizeof(source) - 1));
    assert(!runtime.call("update"));
    assert(std::strstr(runtime.error(), "generator range") != nullptr);
}

void randomSequencesArePerRuntimeAndResetWithTheSeed() {
    const char source[] =
        "function update() "
        "assert(math.random(0, 4294967295) == 270369) "
        "assert(math.random(0, 4294967295) == 67634689) "
        "assert(math.random(0, 4294967295) == 2647435461) end";
    LuaRuntime first(1), second(1);
    assert(first.load(source, sizeof(source) - 1));
    assert(second.load(source, sizeof(source) - 1));
    assert(first.call("update"));
    assert(second.call("update"));
    assert(first.load(source, sizeof(source) - 1));
    assert(first.call("update"));
}

void cosmeticRandomDoesNotAdvanceGameplayRandom() {
    const char source[] =
        "function update() "
        "local cosmetic = tg.cosmetic_random(0, 4294967295) "
        "assert(cosmetic == 1359504952) "
        "assert(math.random(0, 4294967295) == 270369) "
        "for i = 1, 100 do tg.cosmetic_random() end "
        "assert(math.random(0, 4294967295) == 67634689) "
        "assert(math.random(0, 4294967295) == 2647435461) end";
    LuaRuntime first(1), second(1);
    assert(first.load(source, sizeof(source) - 1));
    assert(second.load(source, sizeof(source) - 1));
    assert(first.call("update"));
    assert(second.call("update"));
    assert(first.load(source, sizeof(source) - 1));
    assert(first.call("update"));

    const char edge_seed[] =
        "function update() assert(tg.cosmetic_random(0, 4294967295) == 270369) end";
    LuaRuntime zero_cosmetic_seed(0x9e3779b9U);
    assert(zero_cosmetic_seed.load(edge_seed, sizeof(edge_seed) - 1));
    assert(zero_cosmetic_seed.call("update"));

    const char invalid[] =
        "function update() tg.cosmetic_random(-4294967296, 4294967296) end";
    LuaRuntime invalid_range;
    assert(invalid_range.load(invalid, sizeof(invalid) - 1));
    assert(!invalid_range.call("update"));
    assert(std::strstr(invalid_range.error(), "generator range") != nullptr);
}

void heapAndSourceLimitsAreEnforced() {
    LuaRuntime runtime;
    const char source[] = "local text = string.rep(string.rep('x', 1024), 1024)";
    assert(!runtime.load(source, sizeof(source) - 1));
    assert(runtime.heapUsed() <= LuaRuntime::MAX_HEAP_BYTES);

    std::string too_large(LuaRuntime::MAX_SOURCE_BYTES + 1, ' ');
    assert(!runtime.load(too_large.data(), too_large.size()));
}

void nativeLoopWorkIsBounded() {
    LuaRuntime valid;
    const char small[] =
        "assert(string.rep('ab', 3, '-') == 'ab-ab-ab')\n"
        "assert(string.rep('x', -1) == '')\n"
        "assert(string.lower('AB') == 'ab' and string.upper('ab') == 'AB')\n"
        "assert(string.reverse('abc') == 'cba' and string.sub('abc', -2) == 'bc')\n"
        "assert(tonumber('12.5') == 12.5 and tonumber('ff', 16) == 255)\n"
        "assert(tonumber(12) == 12 and tonumber('not a number') == nil)\n"
        "local a,b = string.byte('AB', 1, 2); assert(a == 65 and b == 66)\n"
        "assert(string.char(a,b) == 'AB')\n"
        "local packed = table.pack(a,b); local x,y = table.unpack(packed); "
        "assert(packed.n == 2 and x == a and y == b)\n"
        "local src = { 1, 2, 3 }; local dst = {}\n"
        "assert(table.move(src, 1, 3, 1, dst) == dst)\n"
        "assert(dst[1] == 1 and dst[3] == 3)\n"
        "table.insert(dst, 2, 9); assert(table.remove(dst, 2) == 9)\n"
        "assert(table.concat(dst, '-') == '1-2-3')\n"
        "assert(table.concat({'a', 2, 'c'}, '-', 2, 3) == '2-c')\n"
        "assert(table.concat({}, '-', 2, 1) == '')\n"
        "assert(table.remove({}) == nil)\n"
        "function update() end";
    assert(valid.load(small, sizeof(small) - 1));

    LuaRuntime bounded;
    const char large_valid[] =
        "local s=string.rep('x', 1024); assert(#string.rep(s, 64) == 65536)";
    assert(bounded.load(large_valid, sizeof(large_valid) - 1));

    for (const char* source : {
             "function update() string.rep('', 2000000000) end",
             "function update() table.move({}, 1, 2000000000, 1, {}) end",
             "function update() table.move({}, -9223372036854775807, 9223372036854775807, 1, {}) end",
             "function update() table.concat({}, '', -9223372036854775807-1, 9223372036854775807) end",
             "function update() for i=1,100 do string.rep('', 4096) end end",
             "function update() local s=string.rep('x', 1024); string.rep(s, 65) end",
             "function update() local s=string.rep('x', 1024); string.rep('', 66, s) end",
             "function update() local s=string.rep('x', 1024); "
             "for i=1,100 do string.rep(s, 1) end end",
             "function update() local s=string.rep('x', 1024); "
             "for i=1,100 do table.concat({s}) end end",
             "function update() local s=string.rep('x', 1024); "
             "for i=1,100 do string.upper(s) end end",
             "function update() local s=string.rep('9', 1024); "
             "for i=1,100 do tonumber(s) end end",
             "local s=string.rep(string.rep('9', 1024), 64)..'9'; "
             "function update() tonumber('1', s) end",
             "function update() table.unpack({}, -9223372036854775807-1, 9223372036854775807) end",
             "function update() local s=string.rep('x', 1024); "
             "local t={} for i=1,66 do t[i]=s end table.concat(t) end",
             "function update() local s=string.rep('x', 1024); "
             "local t={} for i=1,66 do t[i]='' end table.concat(t, s) end",
             "function update() local t={} for i=1,4100 do t[i]=i end table.insert(t, 1, 1) end",
             "function update() local t={} for i=1,4100 do t[i]=i end table.remove(t, 1) end",
             "function update() local t={} for i=1,4100 do t[i]='' end table.concat(t) end",
         }) {
        LuaRuntime runtime;
        assert(runtime.load(source, std::strlen(source)));
        assert(!runtime.callUpdate(1.0f / 60.0f, tg::Input{}));
        assert(std::strstr(runtime.error(), "native work limit") != nullptr ||
               std::strstr(runtime.error(), "native byte limit") != nullptr ||
               std::strstr(runtime.error(), "native argument work limit") != nullptr);
        assert(!runtime.ready());
        assert(runtime.frame().size() == 0);
    }
}

void drawCommandLimitFaultsAndClearsPartialOutput() {
    LuaRuntime runtime;
    const char source[] =
        "function update(dt, input)\n"
        "  for i = 1, 129 do tg.rect(0, 0, 1, 1, 0) end\n"
        "end\n";
    assert(runtime.load(source, sizeof(source) - 1));
    assert(!runtime.callUpdate(1.0f / 60.0f, tg::Input{}));
    assert(std::strstr(runtime.error(), "draw limit") != nullptr);
    assert(runtime.frame().size() == 0);
}

void drawRectangleRejectsOutOfRangeEdges() {
    LuaRuntime runtime;
    const char source[] = "function update() tg.rect(999999, 0, 2, 1, 0) end";
    assert(runtime.load(source, sizeof(source) - 1));
    assert(!runtime.callUpdate(1.0f / 60.0f, tg::Input{}));
    assert(std::strstr(runtime.error(), "tg.rect command is invalid") != nullptr);

    LuaRuntime bad_palette;
    const char invalid_palette[] = "function update() tg.rect(0, 0, 1, 1, 16) end";
    assert(bad_palette.load(invalid_palette, sizeof(invalid_palette) - 1));
    assert(!bad_palette.callUpdate(1.0f / 60.0f, tg::Input{}));
    assert(std::strstr(bad_palette.error(), "tg.rect command is invalid") != nullptr);
}

void invalidUpdateTimeFaultsAndClearsOutput() {
    LuaRuntime runtime;
    const char source[] =
        "function update() tg.rect(0, 0, 1, 1, 1) end";
    assert(runtime.load(source, sizeof(source) - 1));
    assert(runtime.callUpdate(1.0f / 60.0f, tg::Input{}));
    assert(runtime.frame().size() == 1);
    assert(!runtime.callUpdate(-1.0f, tg::Input{}));
    assert(!runtime.ready());
    assert(runtime.frame().size() == 0);
}

void inputAllocationFailureIsContained() {
    LuaRuntime runtime;
    const char source[] =
        "local held = {}\n"
        "for i = 1, 4096 do held[i] = false end\n"
        "local slot = 1\n"
        "function update(dt, input)\n"
        "  held[slot] = input\n"
        "  slot = slot + 1\n"
        "end\n";
    assert(runtime.load(source, sizeof(source) - 1));
    bool saw_failure = false;
    for (int i = 0; i < 4096; ++i) {
        if (!runtime.callUpdate(1.0f / 60.0f, tg::Input{})) {
            assert(!runtime.ready());
            assert(runtime.frame().size() == 0);
            assert(std::strstr(runtime.error(), "memory") != nullptr);
            saw_failure = true;
            break;
        }
    }
    assert(saw_failure);
}

int main() {
    runsOnlyTheAuditedSandboxLibraries();
    bundleModulesAreCachedAndPathRestricted();
    callbackInstructionBudgetFaultsTheRuntime();
    randomRejectsRangesWiderThanItsDeterministicGenerator();
    randomSequencesArePerRuntimeAndResetWithTheSeed();
    cosmeticRandomDoesNotAdvanceGameplayRandom();
    heapAndSourceLimitsAreEnforced();
    nativeLoopWorkIsBounded();
    drawCommandLimitFaultsAndClearsPartialOutput();
    drawRectangleRejectsOutOfRangeEdges();
    invalidUpdateTimeFaultsAndClearsOutput();
    inputAllocationFailureIsContained();
    std::puts("lua runtime tests passed");
}
