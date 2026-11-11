#pragma once

// Carried over from v1.0 src/hal/power.h.
// Unchanged apart from this comment.

namespace power {

bool initialize();
bool prepareWake();
const char* wakeName();
// True while either encoder switch is physically pressed.
bool buttonsPressed();
// Returns only if a wake switch became active before sleep entry.
void enterSleep();

}  // namespace power
