#include <cassert>
#include <cstdio>
#include <cstring>

#include "registry.h"

tg::Game* factory() { return nullptr; }

int main() {
    assert(!registry::add({nullptr, "NAME", 0, factory}));
    assert(!registry::add({"Upper", "NAME", 0, factory}));
    assert(!registry::add({"valid", " ", 0, factory}));
    assert(!registry::add({"valid", "NAME", 0, nullptr}));
    assert(registry::add({"first", "ZED", 0, factory}));
    assert(!registry::add({"first", "ALIAS", 0, factory}));
    char ids[registry::MAX_GAMES][16];
    for (int i = 1; i < registry::MAX_GAMES; ++i) {
        std::snprintf(ids[i], sizeof(ids[i]), "game%d", i);
        assert(registry::add({ids[i], "ALPHA", 0, factory}));
    }
    assert(!registry::add({"overflow", "OVERFLOW", 0, factory}));
    assert(registry::count() == registry::MAX_GAMES);
    assert(registry::errorCount() == 6);
    assert(std::strcmp(registry::error(5).reason, "32-game capacity reached") == 0);
    registry::sortByName();
    assert(std::strcmp(registry::get(0).name, "ALPHA") == 0);
    assert(registry::find("first") == registry::MAX_GAMES - 1);
}
