#pragma once
// Board runner: the OS shell on core 1, plus requests from the serial console.
#include <stddef.h>
#include <stdint.h>

// Deep sleep (platform_esp.c). plat_deep_sleep never returns: the next wake is a reboot, and the
// click that woke it starts the boot. resume is the game to bring back paused, or "".
void plat_deep_sleep(const char *resume);
int  plat_woke_from_sleep(void);
const char *plat_wake_resume_id(void);
void plat_release_sleep_holds(void);

void game_start(int safe_mode);   // scans the built-in carts and starts the shell
// Queue an uploaded cart: it is stored as /carts/<id>.lua (replacing any cart with that id) and
// starts playing. The source is copied.
void game_load(const char *src, size_t len, const char *id);
void game_play(const char *id);     // launch a stored or built-in cart by id
void game_delete(const char *id);   // remove a stored cart
void game_list(void);               // print the gallery as {"t":"shell"} lines
void game_restart(void);   // relaunch the last game
// Relaunch the last game deterministically (fixed seed, frame-based time, no input) and print
// {"t":"hash"} lines for frame 1, every 30th frame and the last one. Compare with the simulator.
void game_hash(int frames, uint32_t seed);
