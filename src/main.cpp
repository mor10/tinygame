#include <Arduino.h>

#if defined(TG_TEST_CARD)
#include "diagnostics/test_card.h"
#else
#include "os.h"
#endif

namespace {

#if defined(TG_TEST_CARD)
bool diagnostic_ready = false;
#else
bool os_ready = false;
#endif

}  // namespace

void setup() {
#if defined(TG_TEST_CARD)
    diagnostic_ready = diagnostics::initialize();
#else
    os_ready = os::initialize();
#endif
}

void loop() {
#if defined(TG_TEST_CARD)
    if (diagnostic_ready) {
        diagnostics::update();
    } else {
        yield();
    }
#else
    if (os_ready) {
        os::update();
    } else {
        yield();
    }
#endif
}
