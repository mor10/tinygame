#include <cassert>
#include <cstdio>
#include "power_state.h"

int main() {
    power_state::State s;
    s.initialize(0);
    s.update(1000, false, true);
    assert(!s.acceptingInput());
    s.update(1010, false, false);
    s.update(1034, false, false);
    assert(!s.acceptingInput());
    s.update(1035, false, false);
    assert(s.acceptingInput());
    s.update(181034, false, false);
    assert(!s.pending());
    s.update(181035, false, false);
    assert(s.pending() && s.canPrepare());
    s.begin(181035);
    s.update(181154, false, false);
    assert(!s.ready());
    s.update(181155, false, false);
    assert(s.ready());

    s.initialize(0);
    s.update(0, false, false);
    s.update(25, false, false);
    s.update(170000, true, false);
    s.update(349999, false, false);
    assert(!s.pending());
    s.update(350000, false, false);
    assert(s.pending());
    s.cancel(350000);
    assert(!s.pending());
    s.request();
    s.update(350001, false, true);
    assert(!s.canPrepare());
    s.update(350010, false, false);
    s.update(350035, false, false);
    assert(s.canPrepare());
    s.begin(350035);
    s.update(350200, false, true);
    assert(!s.ready());
    s.update(350210, false, false);
    s.update(350235, false, false);
    assert(s.ready());

    s.initialize(UINT32_MAX - 100);
    s.update(UINT32_MAX - 100, false, false);
    s.update(UINT32_MAX - 75, false, false);
    s.update(179923, false, false);
    assert(!s.pending());
    s.update(179924, false, false);
    assert(s.pending());
    std::puts("power state tests passed");
}
