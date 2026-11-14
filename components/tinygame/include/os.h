#pragma once
// The OS shell: Welcome, cartridge gallery, game card, settings, the pause menu, and the game and
// error states around the vm. Portable; the platform calls os_frame() once per frame at os_fps()
// and presents the framebuffer afterwards.
//
// Controls: the scroll knob (right by default, left when Settings > HANDS is LEFT) turns to browse
// and adjust; a click on either knob selects. Back is the "<" on the gallery and game card and the
// BACK row in Settings. In a game, clicking both knobs together pauses and holding both for 3 s
// leaves the game.
#include <stdint.h>
#include <stddef.h>
#include "vm.h"

typedef struct {
    uint8_t sound;    // 0-10 (no audio hardware yet; stored for Phase 8)
    uint8_t light;    // 1-10, backlight in tens of percent
    uint8_t left_handed;
    uint8_t version;
} os_settings_t;

typedef struct {
    vm_input_t in;
    uint8_t chord_pause, chord_exit;
} os_input_t;

typedef enum { OS_WELCOME, OS_GALLERY, OS_CARD, OS_SETTINGS, OS_LEVEL, OS_GAME, OS_PAUSED, OS_ERROR } os_mode_t;

void os_init(const vm_opts_t *opts, int safe_mode);
int  os_add_cart(const char *src, size_t len, const char *fallback_name);   // returns its index
void os_frame(const os_input_t *in);
int  os_fps(void);            // 30 in the shell, the cart's fps in a game
os_mode_t os_mode(void);
const char *os_game_name(void);

// Developer hooks for the serial console.
void os_run_source(const char *src, size_t len);   // replace the "Uploaded" cart and play it
void os_restart_game(void);                        // relaunch the last game
void os_hash_game(int frames, uint32_t seed);      // relaunch deterministically for a hash run
int  os_hash_frames_left(void);                    // >0 while a hash run is active
