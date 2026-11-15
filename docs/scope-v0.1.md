# TinyGame Scope Document v0.1: Platform, OS, Engine and AI-Authoring Kit for the Waveshare ESP32-S3-LCD-1.47B

> **Historical planning document.** It records the early plan and is no longer a description of the product.
> Where it differs from the code, the code and `CLAUDE.md` win. In particular the OS shell (launcher, game
> card, settings, pause) was built in C in `components/tinygame/os.c`, not as a privileged Lua system cart,
> and its look is documented in `docs/ref-designs/`.


> **Status:** reference, not doctrine. Written 2026-11-13 from desk research before any bench work. Where it conflicts with `hardware.yaml` or with measurements, the yaml and the measurements win.
>
> **Updates since writing (2026-11-13 evening):**
> - **`hardware.yaml` confirms:**
>   - IMU SDA=GPIO48, SCL=GPIO47, WHO_AM_I 0x05.
>   - Encoders on GPIO4–9: left A9/B8/click7, right A4/B5/click6. These are undetented, 80 transitions per turn.
>   - The panel runs at 80 MHz, mode 0, with row offset 34 under MADCTL 0x70.
> - **Earlier experiments** showed "pixel waves" on scrolling horizontal lines under controller rotation (MADCTL 0x70). The screen lab (`firmware/`) tests software rotation in native portrait as the fix. See `CLAUDE.md`.
> - **Changes from this document:**
>   - The build is now screen-lab-first. The "software factory"/CI rig idea is dropped in favour of one board on the desk.
>   - USB-Serial-JTAG over TinyUSB CDC during development.
>   - Backlight PWM at 20 kHz.
>   - The API exposes screen width and height because the 1.69 board (280×240) comes later.

TinyGame can run smooth Lua games on the ESP32-S3-LCD-1.47B. Two conditions apply:
- A C engine does all pixel work in an 8-bit indexed framebuffer.
- The second core pushes frames to the ST7789 over SPI.

Plan around a guaranteed, locked 30 fps. 60 fps is an opt-in mode that must pass bench tests, because without a tearing-effect (TE) line a full-frame transfer is too close to the 16.7 ms budget.

Lua 5.4 is the right language, built with 32-bit numbers and heavily sandboxed. It sits behind a small namespaced API and a WASM simulator, so AI agents can test games before a kid flashes them.

## TL;DR

- **Hardware reality:**
  - An ESP32-S3R8 (8 MB PSRAM, 16 MB flash) drives a 172×320 ST7789 over plain 4-wire SPI with **no TE pin**.
  - The board also has a QMI8658 IMU, a 4-bit TF slot, a WS2812 LED, and a battery charger with ADC sensing.
  - There is **no audio**; add a MAX98357A I2S amp.
- **Architecture:**
  - ESP-IDF (C) firmware with a strict HAL.
  - Core 1 runs Lua and rasterizes into a 55 KB 8-bit framebuffer.
  - Core 0 expands palette and rotation into RGB565 strips and DMA-pushes only the changed bands.
  - Locked 30 fps by default, no LVGL, and the launcher is a privileged Lua cart.
- **Language and workflow:**
  - Lua 5.4 with `LUA_32BITS`, an allocator cap, an instruction-count watchdog and a stripped standard library.
  - Carts are single-file `.lua` with text-encoded assets, documented by a versioned one-page "API card".
  - Carts are checked by a linter and a headless WASM simulator, and uploaded over WebSerial through the board's built-in USB-Serial-JTAG.
  - Wi-Fi is off and no accounts are needed.

---

## 1. Executive Summary of Key Decisions

| Area | Decision | Alternatives considered | Why |
|---|---|---|---|
| Framework | ESP-IDF, latest stable, pure C, CMake components | Arduino-as-component; PlatformIO; Zephyr | Full control of SPI DMA, PCNT, sleep, partitions, OTA |
| Display driver | `esp_lcd` for init/commands + our own strip-flush on SPI2 DMA | LovyanGFX, TFT_eSPI, Arduino_GFX | We need a custom 8bpp→RGB565+rotate+byteswap flush, not a GFX library |
| Framebuffer | Two 8-bit indexed 320×172 buffers (2 × 55,040 B) in **internal SRAM** + small RGB565 DMA strips | RGB565 framebuffers in PSRAM | Half the memory traffic; free palette effects |
| Frame rate | **Locked 30 fps**; 60 fps opt-in per cart after bench tests | Variable fps | No TE pin; a full 110 KB frame is ~11 ms theoretical at 80 MHz |
| Rotation | Panel stays in native portrait scan order; rotate **in software** during expansion | MADCTL landscape (MV bit) | Free while touching each pixel; limits tearing to one seam (bench-verify) |
| OS UI | Launcher/settings/pause built on the engine as a system cart | LVGL | One renderer, one look, dogfoods the API |
| Language | **Lua 5.4.x (PUC), `LUA_32BITS`** | MicroPython, Luau, Berry, QuickJS, wasm3 | Small, sandboxable, LLM-fluent; S3 FPU is single-precision |
| Sandbox | No `io`/`os`/`debug`/`load`/`require`; allocator cap; `LUA_MASKCOUNT` watchdog | Killing the RTOS task | A bad cart must never hang or brick the device |
| Input | 2× EC11 on **PCNT**; clicks via 1 kHz debounce task | GPIO ISR; polling | Zero CPU cost, no missed edges |
| System chord | Both clicks → Pause menu; both held 3 s → force exit; right click at power-on → safe mode | Long-press one knob | Games keep full use of each knob |
| Audio | **MAX98357A I2S + 8 Ω speaker**; 4-voice synth at 22,050 Hz | Piezo on LEDC PWM | No onboard audio; piezo can't mix |
| Carts | **Single `.lua` text file**, text-encoded assets, 128 KB limit | Zip bundle, PNG carts | Easiest for AI to author, lint, diff, share |
| Storage | Factory recovery + 2 OTA slots + LittleFS `carts` + LittleFS `saves`; TF optional | FAT on flash | Power-loss resilient; saves isolated |
| Upload | **WebSerial** over USB-Serial-JTAG; MSC later | TinyUSB MSC; Wi-Fi AP; BLE | No driver, keeps esptool and console, no radio |
| Simulator | Same C engine + Lua compiled to **WASM**, plus headless CLI | JS reimplementation | No drift; AI agents self-verify |
| Network | Wi-Fi/BLE **off**; no network API for carts (v1) | On-device gallery | Child safety, privacy, simplicity |

---

## 2. Hardware Facts

Legend:
- **[V]** Waveshare official docs/schematic or Espressif docs.
- **[S]** Secondary (community repo/library).
- **[A]** Inference.
- **[Y]** Confirmed by `hardware.yaml`.
- **[BENCH]** Must be measured on real hardware.

### 2.1 Distinguishing the three "1.47" boards

| Board | Controller | Backlight | IMU | Charger | Touch | Header |
|---|---|---|---|---|---|---|
| **ESP32-S3-LCD-1.47B (target)** | ST7789 | **GPIO46** | QMI8658 | Yes | No | 18-pin |
| ESP32-S3-LCD-1.47 (non-B) | ST7789 | GPIO48 | No | No | No | 18-pin |
| ESP32-S3-Touch-LCD-1.47 | **JD9853** | — | No | Yes | AXS5106L | 22-pin |

Code for the non-B board is a trap. Its LCD data pins match, but its backlight is on GPIO48, which is the IMU's SDA on the B board.

### 2.2 Hardware facts table

| Item | Fact | Status |
|---|---|---|
| SoC | ESP32-S3R8, dual-core LX7 up to 240 MHz | [V] |
| Memory | 512 KB SRAM, 8 MB octal PSRAM, 16 MB flash (W25Q128) | [V] [Y] |
| Display | 1.47" IPS, 172×320, RGB565, inversion on | [V] [Y] |
| Interface | 4-wire SPI, write-only: MOSI 45, SCLK 40, CS 42, DC 41, RST 39 | [V] [Y] |
| Panel offset | 34 px: on columns in portrait, on rows in landscape (MV) | [S] [Y] |
| TE pin | **Not routed** | [V] schematic |
| Backlight | GPIO46 → MOSFET; PWM-dimmable | [V] [Y] |
| SPI clock | S3 SPI divides 80 MHz by integers: 80 or 40 MHz (60 becomes 40) | [S]; 80 MHz used in earlier experiments [Y] |
| IMU | QMI8658 at 0x6B, SDA 48 / SCL 47, WHO_AM_I 0x05 | [Y]; INT pins [BENCH] |
| TF card | CMD 15, CLK 14, D0 16, D1 18, D2 17, D3 21; 4-bit SDMMC | [V] |
| RGB LED | WS2812B on GPIO38 | [V] [Y] |
| Battery | ETA6098 charger; BAT_ADC via ×3 divider on GPIO1; no fuel gauge | [V]/[S] |
| Power switch | None; add an external switch | [A] |
| Audio | **None** | [V] |
| Buttons | BOOT (GPIO0) and RESET only | [V] |
| USB | Type-C to native USB (GPIO19/20) | [V] |
| Strapping | GPIO0, 3, 45, 46 | [V] |
| Encoders | Left A9/B8/SW7, right A4/B5/SW6, internal pull-ups, 80 transitions/turn | [Y] |

Community firmware on this exact board (Zephyr) reported "~30 Hz target (~21 Hz measured)" for naive full-frame redraws. Naive redraws do not reach 30 fps.

### 2.3 Rotation implications (device rotated 90° clockwise)

- **Display:**
  - The panel is natively portrait; the logical game screen is 320×172.
  - With MADCTL landscape (MV=1), the 34 px offset moves from columns to rows.
  - Keep MADCTL portrait and rotate in the flush. Draw an arrow and corner markers on day one to fix the direction.
- **IMU:**
  - Axes are fixed to the PCB, so screen X maps to ±IMU Y and screen Y to ±IMU X.
  - The HAL applies a sign/swap matrix from board config, confirmed by a "tilt right → x positive" test.
  - Games never see raw axes.

---

## 3. Architecture Overview

```
┌──────────────────────────────────────────────────────────────────┐
│ Tooling: web uploader (WebSerial) · WASM simulator · headless CLI│
│          linter · API card / llms.txt · example carts · AI skill │
├──────────────────────────────────────────────────────────────────┤
│ Cartridges: single-file .lua (meta + code + text assets)         │
├──────────────────────────────────────────────────────────────────┤
│ TinyGame Lua API v1 (gfx, knob, tilt, sfx, music, save, sys,     │
│   tier-2 helpers: hit, timers, tweens, groups)                   │
├──────────────────────────────────────────────────────────────────┤
│ Engine (C): 8bpp rasterizer, sprite/tile/text batching, camera,  │
│   palette, collision, particles, synth, fixed-step loop, Lua host│
├──────────────────────────────────────────────────────────────────┤
│ OS services (C): cart loader, launcher host, pause/settings,     │
│   crash screen, saves, power, battery, OTA, logging, safe mode   │
├──────────────────────────────────────────────────────────────────┤
│ Drivers: ST7789 flush, PCNT encoders, buttons, QMI8658, I2S,     │
│   LEDC backlight, WS2812, SDMMC, ADC battery, LittleFS           │
├──────────────────────────────────────────────────────────────────┤
│ HAL contract: display_present(), input_poll(), imu_read(),       │
│   audio_pull_cb, storage, time, power → ESP32-S3 / SDL2 / WASM   │
└──────────────────────────────────────────────────────────────────┘
```

Rule: everything above the HAL is portable C11 with no ESP-IDF headers. pinyol, an ESP32-S3 Lua handheld engine, does exactly this: one `hal.h` contract, and one source tree that builds for the console, desktop and browser.

---

## 4. Display Pipeline ("Screen Painting")

### 4.1 Throughput math

- A full frame is 320 × 172 × 2 B = 110,080 B.
- Theoretical wire time is 11.0 ms at 80 MHz and 22.0 ms at 40 MHz.
- At the effective rates measured on another S3 board (7.4 and 4.8 MB/s), that is about 14.9 ms and 22.9 ms.
- At 30 fps (33.3 ms per frame) even 40 MHz has headroom. That is why a locked 30 fps is the product promise.

### 4.2 Recommended pipeline

1. **Render (core 1):** Lua `draw()` calls into C, which rasterizes into an 8-bit back buffer (55,040 B, internal SRAM).
2. **Swap:** pointer swap at frame end.
3. **Diff (core 0):** compare against the last sent frame in bands and skip unchanged ones.
4. **Expand + rotate + byteswap (core 0):** a 256-entry pre-byteswapped palette writes RGB565 into DMA-capable internal strip buffers, in native portrait order.
5. **Push:** queued SPI DMA; strip N+1 is expanded while strip N is on the wire.

### 4.3 Why these choices

- **8bpp over RGB565:** half the RAM and write bandwidth, fits internal SRAM, and gives free palette swaps, fades and flashes.
- **PSRAM caveat:** PSRAM is cached and slow for random access, and DMA traffic competes with it. Keep framebuffers, strips and the hot Lua heap internal; put cart text, sheets and maps in PSRAM. Set PSRAM to 80 MHz explicitly.
- **Tearing without TE** can only be made less visible:
  - Write in native scan order, so any tear is a single seam.
  - Keep pushes short.
  - Experiment with FRCTRL2.
  - Avoid full-screen high-contrast scrolling in the house style.
- **Libraries:** use GFX libraries only for bring-up.
- **LVGL:** avoid it.

### 4.4 Fantasy-console rendering model

- **Screen:** 320×172, origin top-left, positions floored to integers.
- **Palette:** 16 game colours (0–15), with indices 16–31 reserved for the OS.
- **Sprites:** 8×8 and 16×16 from text-encoded sheets, with flip and 2× scale.
- **Tilemaps:** up to 4 layers, 8 or 16 px tiles, per-layer parallax.
- **Text:** 5×7 and 8×8 bitmap fonts.
- **Other:** camera, clip, and shape primitives, including filled triangles.

### 4.5 Per-frame budget (30 fps = 33.3 ms)

| Core 1 (game) | Budget | Core 0 (system) | Budget |
|---|---|---|---|
| Input snapshot | 0.1 ms | Flush: diff + expand + DMA (full frame, 80 MHz) | ~15–18 ms |
| Lua `update()` | ≤ 10 ms | Audio synth | ~1–2 ms |
| Lua `draw()` calls | ≤ 8 ms | Input (1 kHz) + IMU (100–200 Hz) | < 0.5 ms |
| C rasterization | ≤ 6 ms | USB, logging | as needed |
| GC step | ≤ 2 ms | | |
| **Slack** | ≥ 7 ms | | |

At 60 fps the flush nearly fills core 0, and Lua gets about 8 ms in total.

---

## 5. Lua Runtime

### 5.1 Recommendation: Lua 5.4.x (PUC-Rio)

- **`LUA_32BITS`:** int32 integers and float numbers. `sys.time()` returns integer milliseconds.
- **GC:** generational mode plus an explicit `lua_gc(L, LUA_GCSTEP, n)` after `draw()`. Measure GC pause distribution on the bench.
- **Allocator:** custom `lua_Alloc`. Small blocks come from an internal pool and larger ones from PSRAM. The proposed cap is 1 MB per cart, 256 KB of it internal. On hitting the cap, run a full GC, then fail with "Your game ran out of memory".
- **Watchdog:** a `lua_sethook` count hook. Past 250 ms in one callback, it raises "Your game got stuck (maybe a loop that never ends?) at line N".
- **Stdlib:**
  - Keep `base` without `load`, `dofile`, `loadfile` and most of `collectgarbage`.
  - Keep `string` without `dump`, plus `table`, `math` and `utf8`.
  - Remove `io`, `os`, `debug` and `package`.
- **Locked API:** the API tables are read-only.
- **Errors:** file and line plus a traceback, shown on a crash screen and sent as JSON over serial.

### 5.2 Alternatives

| Option | Verdict |
|---|---|
| Lua 5.1 / LuaJIT | LuaJIT has no Xtensa backend; 5.1 lacks integers |
| eLua / NodeMCU Lua | Borrow ideas (LFS, 32-bit builds) only |
| Luau | Large C++ VM, 5.1 semantics |
| PICO-8 dialect | LLMs leak `btn()`, `+=`, 128×128 assumptions; standard Lua lets the linter catch them |
| MicroPython | Most LLM-fluent; bigger, slower, harder GC bounds; runner-up |
| Berry / Wren / Squirrel | Too little LLM training data |
| QuickJS / Duktape / Espruino | Heavier, worse frame-time determinism |
| wasm3 | Needs a compiler in the loop |

ESP32-S3 prior art:
- Lozamded's fantasy console runs Lua 5.4.
- pinyol runs Lua at 60 fps with dual-core strip flushing.
- luanette runs single-file carts with changed-pixel flushing.

### 5.3 Speed expectations

- PICO-8 specifies 4M VM instructions per second.
- esp-box-emu reports about 1.3M Lua instructions per second on the S3, with the Lua heap in PSRAM.
- Design for about 40k Lua instructions per frame at 30 fps until the bench says otherwise.

---

## 6. Prior Art: Lessons

| Project | Copy | Avoid |
|---|---|---|
| PICO-8 | Tiny vocabulary; init/update/draw; fixed palette; text carts | Its dialect |
| TIC-80 | 240×136 (close aspect); text cart files | Many languages |
| Playdate | Crank ≈ knobs: change + acceleratedChange; simulator-first | Large SDK |
| MakeCode Arcade | Browser simulator; share links; AI goes through the adult's account | Cloud-only editor |
| Thumby / Arduboy / PicoSystem | Web IDE/emulator; afternoon-sized games | C++ toolchains |
| ESP32-S3 Lua consoles | C does sprites/tiles/physics; dual-core strip flush; `hal.h`; `AGENTS.md` | Treat numbers as directional |
| Scratch / micro:bit | No accounts; instant feedback; remix | — |

---

## 7. Firmware / OS Architecture

### 7.1 Tasks

| Task | Core | Rate | Notes |
|---|---|---|---|
| game (Lua + rasterizer) | 1 | 30/60 Hz | Only task touching Lua |
| flush | 0 | per frame | Diff, expand, DMA |
| audio | 0 | 10 ms | Never blocks |
| input | 0 | 1 kHz | PCNT, debounce, chords |
| imu | 0 | 100–200 Hz | Filter, tilt, shake |
| sys | 0 | event | USB, power, battery, logs |

### 7.2 Boot flow

1. The bootloader picks an OTA slot, with rollback.
2. Display init to black, then a backlight fade-in.
3. Holding the right knob at power-on boots safe mode.
4. Mount the LittleFS carts and saves partitions.
5. Load the launcher system cart.
6. Welcome → Cartridges carousel → Game Card → Play.

### 7.3 Launcher / system UI

- **Controls:** OS-wide, the left knob navigates and the right knob acts.
- **Pause:** a both-click chord. The game frame is dimmed by palette remap, with Continue / Settings / Exit Game.
- **Settings:** Sound, Light, Tilt calibrate, About, Back.
- **Crash screen:** "Oops! Your game hit a bug." plus a short message and line number. A JSON copy goes out over serial.

### 7.4 Game lifecycle

`load` → `init()` → loop [`update()` fixed step → `draw()`] → pause/resume → `on_exit()` → launcher.

- Carts declare `fps = 30` or `60`.
- On overrun, the engine skips at most one draw and runs no more than 2 updates per frame.
- There is no `dt`.

### 7.5 Memory budget

| Region | Item | Size (est.) |
|---|---|---|
| Internal SRAM (512 KB) | IDF + FreeRTOS + drivers + stacks | ~120–160 KB |
| | 8bpp framebuffers ×2 | 110 KB |
| | DMA strip buffers | ~22–60 KB |
| | Audio | ~8 KB |
| | Lua small-object pool | 96–160 KB |
| | Engine tables | ~16 KB |
| PSRAM (8 MB) | Lua large objects (cap) | ≤ 1 MB |
| | Decoded sheets + maps | ≤ 512 KB/cart |
| | Cart source | ≤ 128 KB |
| | Launcher art cache | ~256 KB |

### 7.6 Partition table (16 MB)

| Name | Type | Size | Purpose |
|---|---|---|---|
| nvs | data | 24 KB | Settings, tilt calibration |
| otadata / phy_init | data | 8 KB / 4 KB | — |
| factory | app | 2 MB | Recovery firmware, never OTA-updated |
| ota_0 / ota_1 | app | 3 MB each | Main firmware A/B |
| carts | LittleFS | 7.5 MB | Carts + bundled examples |
| saves | LittleFS | 256 KB | Separate so cart churn can't corrupt saves |
| coredump | data | 64 KB | Post-mortem |

### 7.7 Saves, power, battery, OTA, versioning, logging

- **Saves:**
  - Per-cart key/value store with a 4 KB quota.
  - Atomic write-then-rename, coalesced to once per second and on exit.
- **Power:**
  - Dim at 30 s idle; at 2 min, backlight off and auto-pause.
  - Light sleep at 5 min; deep sleep at 15 min, waking on a knob click.
  - The knob pins (GPIO 4–9) can all wake the board from deep sleep.
- **Battery:** ADC on GPIO1 ×3, median of 16 readings, shown as 4 bars.
- **OTA:** USB-only in v1, with rollback.
- **API versioning:** carts declare `api = 1`, and firmware supports N and N-1.
- **Logging:** JSON lines over USB, plus a RAM ring buffer.

---

## 8. Input

### 8.1 EC11 decoding

- **Decoding:** PCNT, one unit per encoder, 4× quadrature, with a glitch filter of about 1 µs.
- **Steps:** the driver converts counts to steps. The undetented parts give 80 transitions per turn; expose fine steps plus a coarse "notch" view for menus.
- **Speed:** EMA velocity, with optional Playdate-style acceleration.
- **Clicks:** 5 ms debounce, with pressed/held/released/long events. Double-click is off by default.
- **OS chords:**
  - Both pressed within 150 ms → pause.
  - Both held for 3 s → force exit, even if Lua hangs.
- **Hardware:** the internal pull-ups work today. Consider external 10 kΩ pull-ups plus 10 nF to ground if the bench shows bounce.

### 8.2 IMU

- **Settings:** QMI8658 accelerometer at ±4 g, 200 Hz.
- **Pipeline:** raw → board rotation matrix → minus calibration → low-pass (~8 Hz) → ±35° mapped to −1..1, with a 3° dead zone.
- **Shake:** a high-pass magnitude above 1.8 g.
- **Calibration:** "Hold me flat" in Settings, plus a per-game `tilt.zero()`.
- **Tilt design rules:**
  - Use tilt for gentle steering, never precision aiming.
  - Always show a tilt indicator.
  - Every tilt game needs a knob fallback.

### 8.3 Two-knob + tilt patterns

| Pattern | Mapping |
|---|---|
| Paddle | Left turn → paddle x; right click → launch |
| Etch-a-sketch | Left = X, right = Y |
| Steering | Left/tilt = steer, right = throttle |
| Aim + fire | Left = angle, right click = fire, right turn = power |
| Rolling ball | Tilt = gravity; right click = jump |
| Platformer | Left turn = walk, right click = jump, right long = special |
| Menu / text entry | Left = move or letter wheel, right click = OK |

---

## 9. TinyGame API v1 (proposed surface)

### 9.1 Principles

1. There is one way to do each thing.
2. Namespaces, not globals (`gfx.`, `knob.`, `tilt.`, `sfx.`, `music.`, `save.`, `sys.`).
3. Lowercase words with underscores; strings for sides and presets.
4. Callbacks are `init()`, `update()` and `draw()`, plus optional `on_pause()` and `on_exit()`.
5. Integers wherever possible.
6. Errors are loud and early, with hints.
7. Two tiers: a core API and helpers.

### 9.2 Compact reference (illustrative)

```
cart = { title="Brick Bop", author="Mia & Dad", api=1, fps=30,
         inputs={"knobs"}, tagline="Bounce it!", icon=[[...]], hero=[[...]] }

init()  update()  draw()  on_pause()  on_exit()

gfx.clear(c) gfx.pixel(x,y,c) gfx.line(x1,y1,x2,y2,c)
gfx.rect(x,y,w,h,c) gfx.fill_rect(x,y,w,h,c) gfx.circle(x,y,r,c) gfx.fill_circle(x,y,r,c)
gfx.fill_tri(x1,y1,x2,y2,x3,y3,c) gfx.text(str,x,y,c,[big]) gfx.text_width(str,[big])
gfx.sprite(name,x,y,[flip_x],[flip_y],[scale]) gfx.map(layer,[x],[y])
gfx.camera(x,y) gfx.ui() gfx.pal(from,to) gfx.pal_reset() gfx.transparent(c) gfx.clip(x,y,w,h)
gfx.W, gfx.H   -- always use these; the 1.69 board is 280x240

sprites = { ball=[[ ... ]] }  maps = { level1={ tiles=8, layers={[[ ... ]]} } }
sounds  = { boing="square C5 60ms down" }  songs = { theme="T120 C4 E4 G4 C5 | ..." }

knob.turn(side) knob.turn_fast(side) knob.pos(side) knob.speed(side)
knob.pressed(side) knob.held(side) knob.released(side) knob.long(side)

tilt.x() tilt.y() tilt.shake() tilt.zero()

sfx.play(name_or_preset,[channel]) sfx.tone(freq_hz,ms,[wave],[volume])
music.play(name,[loop]) music.stop()

save.set(key,value) save.get(key,default) save.clear()

sys.time() sys.frame() sys.battery() sys.fps() sys.log(str) sys.exit()

hit(a,b) map_hit(layer,x,y,w,h) map_get(layer,tx,ty) map_set(layer,tx,ty,id)
group() -> g:add(t) g:remove(t) g:each(fn) g:count()
after(s,fn) every(s,fn) tween(obj,{x=..},s,"ease_out")
rnd(n) rnd_int(a,b) choose(list) clamp(v,a,b) lerp(a,b,t) dist(x1,y1,x2,y2)
particles(x,y,count,colour,[speed])
```

### 9.3 C vs Lua split

- **C:** rasterization, batching, maps, text, collision, groups, particles, synth, and input filtering.
- **Lua:** game rules, plus a frozen prelude for tweens and timers.

### 9.4 Documenting the API for LLMs

- **API card / llms.txt:** about 3–4k tokens, including a "Don't" list (no `btn()`, `love.*`, `+=`, `!=`, 128×128 or `dt`).
- **Golden carts:** 6–8, each under 150 lines.
- **Skill file:** the loop is write → `tinygame check` → `tinygame sim --frames 300 --screenshot` → fix.
- **Linter:** undefined-global whitelist, dialect detectors, asset validation, and a 300-frame headless smoke run.

---

## 10. Cartridge Format

- **v1 is one `.lua` text file.** Limits:
  - 128 KB source
  - 256 sprites
  - 4 map layers
  - 32 sounds
  - 8 songs
- **Sprites and maps** are ASCII art in long strings, one character per pixel or tile, with a palette or legend key.
- **Icon and hero art:** 32×32 and up to 160×96. If missing, the launcher renders the cart at frame 60.
- **v2:** an optional bundle that tools convert to the single-file form.

## 11. The "Build With AI" Loop

**Target: idea to playing on the device in under 5 minutes.**

1. The parent opens a static, self-hostable web page. No account is needed.
2. The AI writes `game.lua`, which is linted and simulated in the browser.
3. "Send to TinyGame" uploads over WebSerial, and the device hot-reloads.
4. Errors show on the device and on the page, with a "Copy for AI helper" button.

**Sharing:** carts are shared as files or QR codes, with `remix_of` metadata.

**Safety:**
- No accounts and no telemetry.
- Radios are off, and carts have no network API.
- AI use goes through the parent's account.

## 12. Performance Feasibility for the Four Mock Games

| Game | Requirement | Verdict |
|---|---|---|
| Brick Breaker | Bricks as a tilemap, `map_hit`/`map_set` | Easy at 30, likely 60 |
| Falling Rocks | C-backed `group()` + broadphase in C | OK at 30 |
| Tilt-A-Block | Pre-drawn iso tiles, `fill_tri` | Easy at 30 |
| WalkWalk | Parallax maps in C; full scroll defeats dirty bands | OK at 30 |

Rule: if a game loops over more than 50 things a frame, use a `group()` or a map.

## 13. Extra Hardware

- **Encoders:** EC11-footprint with a push switch, undetented as now. Prototype a detented pair too for the menu feel.
- **Audio:** MAX98357A I2S breakout + 8 Ω speaker, using 3 free GPIOs (to be picked; 4–9 are the knobs).
- **Battery:** protected 1S LiPo, 500–1000 mAh, with an inline slide switch.
- **Enclosure:** landscape, a knob at each end, USB-C reachable, pin holes for BOOT and RESET.

## 14. Roadmap

| Phase | Scope | "Done" test |
|---|---|---|
| 0. Bench truth / screen lab | Rotation, offsets, SPI 40/80, tearing, flush times | Geometry page correct; motion artefacts understood; settings recorded in `hardware.yaml` |
| 1. Display | 8bpp FB, rotate/expand, strip DMA, dirty bands, backlight | Moving sprite locked at 30 fps for 10 min |
| 2. Input | PCNT, buttons, chords, IMU tilt/shake | 100 steps each way counted exactly; tilt right gives +x |
| 3. Lua core | Sandbox, allocator cap, watchdog, errors, `gfx` + `knob` | Infinite loop → "stuck" screen in < 0.5 s; no resets |
| 4. Engine v1 | Assets, sprites, maps, text, groups, collision | Brick Breaker at 30 fps, p99 < 33 ms |
| 5. Simulator + CLI | WASM + headless runner | Same pixel hash on device and sim |
| 6. OS shell | Launcher, Game Card, pause, settings, crash screen, safe mode | A kid navigates unaided |
| 7. Storage | Partitions, LittleFS, quotas | 50 power pulls during saves: no corruption |
| 8. Audio | I2S synth | Click-to-sound < 30 ms |
| 9. Upload | WebSerial page | Plug in → playing in < 60 s |
| 10. AI kit | API card, golden carts, linter | ≥ 80% of kid prompts pass first try |
| 11. Power + OTA | Sleep/wake, battery UI, rollback | Bad image rolls back |
| 12. Sharing | QR, gallery, remix | QR cart plays on a second device |

**First demo games, in order:**
1. Brick Breaker
2. Tilt-A-Block
3. Falling Rocks

WalkWalk follows as the scroll stress test.

## 15. Top Risks

| # | Risk | Mitigation |
|---|---|---|
| 1 | 80 MHz SPI unreliable | 30 fps works at 40 MHz |
| 2 | Visible tearing without TE | Native-order writes, short pushes, FRCTRL, art rules |
| 3 | Lua slower than budget | Internal hot heap; C batching; lint budget |
| 4 | GC hitches | Generational + explicit steps; pools |
| 5 | LLMs emit PICO-8/LÖVE dialect | Namespaces, "Don't" list, lint, examples |
| 6 | Bad cart hangs or bricks | Watchdog, cap, force-exit chord, safe mode, recovery |
| 7 | Save corruption | LittleFS, separate FS, atomic rename |
| 8 | Battery safety | Protected cells, switch |

## 16. Open Questions

- After rotation, is USB-C on the left or right?
- Which GPIOs are the IMU interrupts?
- Target age band (6–8 vs 9–12)?
- Should TF-card carts be listed in the launcher?
- File/QR sharing only, or a gallery in v1?
