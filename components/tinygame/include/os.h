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
    uint8_t sleep;    // Sleep after: 0 = 3 min, 1 = 5 min (default), 2 = 10 min, 3 = off (version 1 stored an unused sound byte here)
    uint8_t light;    // 1-10, backlight in tens of percent
    uint8_t left_handed;
    uint8_t version;
} os_settings_t;

typedef struct {
    vm_input_t in;
    uint8_t chord_pause, chord_exit;
} os_input_t;

typedef enum { OS_WELCOME, OS_GALLERY, OS_CARD, OS_SETTINGS, OS_LEVEL, OS_GAME, OS_PAUSED, OS_ERROR, OS_COLOURCARD, OS_DIAG } os_mode_t;

void os_init(const vm_opts_t *opts, int safe_mode);
int  os_add_cart(const char *src, size_t len, const char *id);   // a built-in cart; returns its index
void os_scan_storage(void);   // adds every cart on the carts volume
void os_frame(const os_input_t *in);
int  os_fps(void);            // 30 in the shell, the cart's fps in a game
os_mode_t os_mode(void);
const char *os_game_name(void);

// Developer hooks for the serial console.
int  os_run_source(const char *src, size_t len, const char *id);   // store as <id>.lua and play it
int  os_play(const char *id);      // launch a cart by id
int  os_delete(const char *id);    // remove a stored cart
void os_list(void);                // print every gallery entry as a {"t":"shell"} line
void os_restart_game(void);                        // relaunch the last game
void os_hash_game(int frames, uint32_t seed);      // relaunch deterministically for a hash run
// Sleep. The shell asks for it (idle timeout, Sleep in the pause menu, both knobs held outside a game);
// the platform then calls os_sleep_prepare() and powers down. After the next boot it calls os_wake().
int  os_sleep_wanted(void);                        // 1 once the shell wants the platform to sleep
void os_sleep_prepare(char *resume_id, size_t n);  // stops the game (saves flush); resume_id = game to resume, or ""
void os_wake(const char *resume_id);               // after a wake: swallow the wake click, resume that game paused
int  os_hash_frames_left(void);                    // >0 while a hash run is active
