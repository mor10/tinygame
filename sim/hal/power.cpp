#include "power.h"

#include "sim_hal.h"

namespace {
bool asleep = false;
}
namespace sim {
bool buttonsPressed();
bool sleeping() { return asleep; }
}  // namespace sim

namespace power {
bool initialize() {
    asleep = false;
    return true;
}
bool prepareWake() { return true; }
const char* wakeName() { return "SIMULATOR"; }
bool buttonsPressed() { return sim::buttonsPressed(); }
void enterSleep() {
    if (!buttonsPressed()) asleep = true;
}
}  // namespace power
