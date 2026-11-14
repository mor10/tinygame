#pragma once
// Input driver: two EC11 encoders on PCNT (4x quadrature, 1 us glitch filter) plus debounced
// clicks and the OS chords. Everything is sampled by one 1 kHz task; consumers take a snapshot.
#include <stdint.h>
#include "esp_err.h"

enum { KNOB_LEFT = 0, KNOB_RIGHT = 1 };

typedef struct {
    int32_t pos[2];          // fine steps (quadrature transitions), signed, clockwise positive
    int32_t delta[2];        // transitions since the previous snapshot
    float   speed[2];        // transitions per second, smoothed
    uint8_t down[2];         // debounced click state right now
    uint8_t pressed[2];      // events since the previous snapshot (each is a count of 0 or 1)
    uint8_t released[2];
    uint8_t long_press[2];   // click held for INPUT_LONG_MS
    uint8_t chord_pause;     // both clicks pressed within INPUT_CHORD_MS of each other
    uint8_t chord_exit;      // both clicks held for INPUT_EXIT_MS
} input_state_t;

#define INPUT_TRANSITIONS_PER_TURN 80
#define INPUT_DEBOUNCE_MS 5
#define INPUT_LONG_MS     600
#define INPUT_CHORD_MS    150
#define INPUT_EXIT_MS     3000

esp_err_t input_init(void);
void input_snapshot(input_state_t *out);   // consumes the event fields; pos/down/speed are state
void input_zero(int side);                 // set pos[side] to 0
