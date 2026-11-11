// New in v2. The Arduino entry points. All the work is in os/main_loop.cpp;
// v1's src/main.cpp had the same shape (setup() initialises, loop() updates).

#include <Arduino.h>

#include "main_loop.h"

void setup() {
    main_loop::setup();
}

void loop() {
    main_loop::loop();
}
