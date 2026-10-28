#include <Arduino.h>

#include "os.h"

namespace {

bool os_ready = false;

}  // namespace

void setup() {
    os_ready = os::initialize();
}

void loop() {
    if (os_ready) {
        os::update();
    } else {
        yield();
    }
}
