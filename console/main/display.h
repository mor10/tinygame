#pragma once
// Display pipeline: the game draws into an 8-bit indexed back buffer; display_present() hands it
// to a flush task on core 0, which diffs it against the last frame sent, expands the palette,
// rotates into the panel's native portrait order and DMA-pushes only the changed bands.
// Panel settings are the screen-lab winners (hardware.yaml lab_result).
#include <stdint.h>
#include <stdbool.h>

#define SCREEN_W 320
#define SCREEN_H 172

void     display_init(void);
uint8_t *display_backbuffer(void);              // valid until the next display_present()
void     display_present(void);                 // waits for the previous flush, then swaps buffers
void     display_set_color(int index, uint32_t rgb888);
void     display_backlight(int percent);

typedef struct {
    uint32_t flushes;
    uint32_t flush_us_max;
    uint64_t flush_us_sum;
    uint64_t bytes_sum;
} display_stats_t;
void display_take_stats(display_stats_t *out);  // returns and resets the counters
