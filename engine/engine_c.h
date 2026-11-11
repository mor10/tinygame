#pragma once

// Flat C interface over tg::Engine for WebAssembly and other non-C++ hosts.
// One engine instance. Implemented in engine/engine_c.cpp.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 1 on success, 0 on failure (see tg_engine_error).
int tg_engine_load(const uint8_t* bundle, uint32_t size, int screen_w, int screen_h);
const char* tg_engine_error(void);
// One tick. buttons: bit 0 = left knob held, bit 1 = right knob held. The
// press, release and click edges are worked out inside.
void tg_engine_tick(int32_t left_delta, int32_t right_delta, int buttons, float tilt_x,
                    float tilt_y, int left_handed);
// Draws and returns the frame: screen_w * screen_h RGB565 pixels.
const uint16_t* tg_engine_draw(void);
// Probe text, valid until the next call.
const char* tg_engine_probe(void);

#ifdef __cplusplus
}
#endif
