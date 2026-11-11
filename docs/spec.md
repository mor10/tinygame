# TinyGame v2: build specification

**Audience:** a coding agent building TinyGame v2, and the maintainer reviewing its work.
**Source:** `github.com/mor10/tinygame` at `83620e3` (referred to as v1).
**Status:** design agreed in conversation; nothing in this document has been prototyped.

## 0. How to use this spec

- Build the milestones in section 17 in order. Each has acceptance criteria; do not start the next until they pass.
- Items marked **UNVERIFIED** are beliefs, not facts. Confirm each against a schematic, datasheet or measurement before relying on it.
- Items marked **GATE** need a human decision. Stop and ask. When the decision is made, write it to `docs/decisions/` (section 15) before continuing.
- Before proposing a change to something already decided, read its decision record. Do not reopen a recorded decision without new evidence.
- Never invent or guess a pin number. Pins come from v1 `src/hal/board_config.h` (Board A) or the vendor schematic (Board B).
- Formats and API names here are normative. If prototyping shows one is wrong, propose the change and wait; do not diverge silently.
- Report results as they are. A failed build, test or flash is reported as failed, with the output.
- Do not run a toolchain migration inside this rebuild (see section 4).

## 1. Vision

TinyGame is a pocket game console that kids (around 10) and adults make games for by talking to a coding agent. v2 turns it into a proper game platform:

1. **Fun and distinctive.** A recognisable look built from simple blocky graphics a child can draw, on two very small screens, without looking low-resolution.
2. **Fast to iterate.** A change is playable in about a second in the simulator and a few seconds on the console. No firmware compile to change a game.
3. **Cheap for the agent.** Common mechanics are selected and tuned, not rewritten. Tools return compact answers. Many changes need no model at all.
4. **Safe.** A shared game cannot run native code on the family computer or damage the console.
5. **Offline-capable (stretch).** The design must allow a small local open model to do most game edits.

**The central design decision:** games are not C++ programs. A game is a folder of text files (art, levels, settings, small Lua rules) run by a C++ engine that is compiled once into the firmware and the browser simulator.

## 2. Scope

**In scope:** engine, two board targets, game format, stock mechanics, six genre starters, three ported games, tooling, tool server, simulator editors, agent guidance, CI, benchmark, release pipeline.

**Deferred:** MakeCode import and any block-based editor. Do not build these. Keeping kit concepts close to common arcade-kit vocabulary (sprite kinds, overlap events, wall tiles) is welcome where it costs nothing.

**Stretch (section 16):** a fine-tuned local model.

**Non-goals:** networking on the console, touch input in games, emulating any existing console or runtime.

## 3. Design principles

1. Kids draw coarse; the engine renders fine.
2. Games are written in blocks and world units, never in screen pixels.
3. Select and tune before writing code. Every common mechanic is a stock part.
4. One fact has one owner. The stock-parts registry (section 10) owns the kit; docs, checks and editor controls are generated from it.
5. Rules are enforced by tools, not by prose. `tg verify` is the single authoritative gate.
6. Everything is deterministic: fixed timestep, seeded randomness, scripted input.
7. Every game runs on both boards.

## 4. Repository strategy

- Tag the current `main` as `v1.0`. It must stay buildable.
- Build v2 as a fresh tree on a `v2` branch in the same repository. Start empty, with CI in the first commit.
- Copy v1 modules in one at a time, each with its tests passing.
- Keep the v1 toolchain: PlatformIO `espressif32 @ 6.12.0`, Arduino core 2.x, C++17, LVGL 9.6.0. Record "Arduino core 2.x is frozen" as an open risk; do not migrate now.

**Carry over from v1 (adapt, don't rewrite)**

| v1 source | Use in v2 |
|---|---|
| `src/hal/board_config.h`, `display_config.h`, panel init sequence in `display.cpp` | Board A config. Preserve the row-axis 34-pixel offset and the `0xB0`/`0xE8` little-endian setting; add a comment explaining each. |
| `src/hal/imu.cpp`, `imu_health.h`, `power.cpp`, `storage.cpp`, `rgb_led.cpp` | Drivers, behind the new board abstraction |
| `src/hal/input_logic.h` (debounce, quadrature) | Unchanged |
| `src/os/power_state.h`, `sleep_gesture.h`, `settings.*`, `persistence.h`, `status_light.*`, `shell_state.*` | Unchanged logic, with their tests |
| `src/os/shell_ui.cpp`, `style.cpp` | LVGL system shell only |
| `src/os/devtools.*`, `dev_line.h`, `dev_input.h`, `controller_stream.h` | Serial dev protocol; extended in section 12 |
| `sim/` HAL replacements, `sim/server.mjs`, `tools/sim_build.py` pins | Simulator base |
| `tools/tg.py` | Starting point for the CLI |

**Write new:** engine, display driver, main loop, game format and packer, Lua host, checker, headless runner, tool server, agent guidance, all games.

**Proposed layout**

```
engine/      C++ engine (portable; no Arduino or LVGL includes)
engine/parts.toml   stock-parts registry (source of truth for the kit)
hal/         board drivers; hal/boards/<board>/ for per-board config
os/          shell, power, settings, dev protocol, main loop
sim/         browser and Node hosts for the WebAssembly engine
games/       one folder per game (text only)
starters/    genre starter games
tools/       tg CLI, packer, checker, tool server
bench/       benchmark tasks
docs/        human docs; docs/generated/ is build output
docs/decisions/     one short record per human decision
.claude/skills/   agent skills (single source; synced copies for other harnesses)
```

## 5. Hardware targets

| | Board A | Board B |
|---|---|---|
| Board | Waveshare ESP32-S3-LCD-1.47B | Waveshare ESP32-S3-Touch-LCD-1.69 |
| Screen in use | 320×172 landscape | 280×240 (240×280 panel rotated) |
| Physical size (computed) | about 33 × 18 mm | about 33 × 28 mm |
| Controls | Two EC11 knobs with click, IMU | **Assumed** the same two knobs and click; IMU **UNVERIFIED** |
| Extras | RGB LED, unused SD slot, battery ADC | Touch, and **UNVERIFIED** buzzer, RTC, battery |

Requirements:
- A board abstraction selected at build time: one PlatformIO environment per board, each with its own `board_config.h`, panel config and capability flags (`has_rgb_led`, `has_buzzer`, `has_touch`).
- **GATE (before M2):** read Board B's schematic. Confirm free GPIOs for six knob lines, that two of them are RTC-capable for click wake, the panel offsets and rotation, and the IMU part and address. Report findings before writing Board B drivers.
- Touch is not exposed to games. The shell may ignore it entirely.
- Sound: if a board has a buzzer, the engine drives it; otherwise sound calls do nothing.

## 6. Architecture

```
Game folder (text)  --tg pack-->  bundle (.tgb)
                                      |
        +-----------------------------+-----------------------------+
        |                                                           |
  Firmware: os + hal + engine + Lua                 WebAssembly: sim hal + engine + Lua
```

- **Engine** is portable C++ with no Arduino, LVGL or browser includes. It takes input and time, and produces a framebuffer, LED colour and sound events. This is what makes it host-testable.
- **Shell** (launcher, pause, settings, diagnostics, sleep) stays on LVGL. Shell and engine own the screen exclusively: while a game runs LVGL does not refresh; pause freezes the game and hands the screen to the shell.
- **Main loop** has explicit phases, replacing v1's single `os::update()`: read input, dev commands, power, shell or engine tick(s), present, pace.
- **Timing:** game logic runs at a fixed 60 ticks per second. Drawing targets 30 frames per second. The loop sleeps when idle instead of spinning.
- **Fault containment:**
  - a Lua error or a script exceeding its per-tick instruction budget stops the game and shows a kid-readable error screen, then returns to the launcher;
  - a hardware watchdog covers the loop;
  - the id of the running game is stored in RTC memory, so after a crash the launcher can say which game stopped.
- **Game storage:** a LittleFS partition for installed bundles; starters are built into the firmware image. Keep the two OTA app slots unless space forces otherwise.

## 7. Display and the stage

**Units.** One block is 8 art pixels drawn at scale 2, so 16 screen pixels (about 1.7–1.9 mm). All game-facing positions, sizes and speeds are in blocks. **GATE (M1):** the block size and scale are confirmed by a test card on both boards before anything depends on them. Keep them as engine constants so the choice can change.

**The stage is 10 blocks tall and at least 17 blocks wide.**

| | Board A | Board B |
|---|---|---|
| Visible stage | 20 × 10 blocks | 17.5 × 10 blocks |
| Status area | 12-pixel strip | 80-pixel panel |
| `view = "fill"` option | no change | 17.5 × 15 blocks, status overlaid |

- The camera follows a target with a dead zone and clamps to level edges.
- Status elements (score, lives, timer, items) are declared, not positioned. The engine lays them out in the strip or panel.
- Fixed-screen games declare a playfield size in blocks; the engine centres it and fills the border.
- Levels must be completable within 17 × 10 visible blocks. `tg check` runs every game at both sizes.

**Renderer.**
- Full-frame RGB565 framebuffer (134 KB worst case) and DMA SPI transfer. Evaluate ESP-IDF `spi_master` directly and LovyanGFX; choose by measurement.
- Target: a full-frame present in 20 ms or less on both boards. Measure and report; if unreachable, report the achievable rate instead of hiding it.
- Tiles are pre-rendered at load into a 16×16 screen-pixel cache with styling baked in, so drawing a layer is plain blitting.
- Sprites and the camera move in single screen pixels for smooth motion.

## 8. Visual style

- **Authoring:** 8×8 tiles; sprites 8×8 or 16×16 art pixels; a hero is normally two blocks tall. Flat colours only.
- **Palette:** one 16-colour platform palette, index 0 transparent, stored at 4 bits per pixel. A game may override individual entries. Final colour values are chosen at the M1 test card; bright, readable on a tiny screen.
- **Engine-added detail at full resolution:** a one-pixel highlight and shade edge on solid blocks, a soft shadow under actors, crisp text.
- **Procedural motion, no drawn frames required:** flip to face direction, squash on landing, stretch on jump, idle bob, blink, hit flash, wobble. A sprite may optionally supply a second frame for walking.
- **Backgrounds:** solid or banded colour, optional parallax tile layer. Light backgrounds are allowed in games. The shell keeps v1's dark style.
- **Text:** one pixel font at screen resolution, two sizes, ASCII.
- **Identity:** front-on 2D blocks with a bevel. Do not imitate another product's look (for example an isometric voxel style).

## 9. Game package

A game is a folder of text files. `tg pack` turns it into one binary bundle; the console and simulator read only bundles, so the device needs no text parsers except Lua.

```
games/<id>/
  game.toml      id, name, author, api version, starter, view, controls, tuning, kinds, status
  sprites.txt    text-art sprites
  levels.txt     text-grid levels with a legend
  rules.lua      optional; what is unique to this game
  README.md      how to play, controls, credit
  test/*.steps   scripted input and assertions
```

**Sprite format** (one character per pixel; `.` transparent; hex digit = palette index):

```
== coin 8x8
..3333..
.3a33a3.
3a3333a3
...
```

**Level format:**

```
== level 1
legend: # = wall, ^ = spikes, P = player, s = slime, o = coin
....................
..........o.........
P.......####....s...
####################
```

**Kinds and tuning** (in `game.toml`):

```toml
api = 1
id = "cave_hop"
name = "CAVE HOP"
starter = "platformer"

[tuning]
gravity = 30        # blocks per second squared
jump_height = 3     # blocks
run_speed = 6       # blocks per second

[kind.slime]
sprite = "slime"
behaviour = "walker"
speed = 2
on_touch_player = "hurt"
```

**Rules** (Lua, only for what stock parts cannot express):

```lua
on.touch("player", "coin", function(p, c)
  c:remove()
  score.add(10)
  fx.sparkle(c.x, c.y)
end)
```

Constraints: id is `[a-z][a-z0-9_]{0,14}`; name is 1–13 printable ASCII; `api` is required and checked at load. Bundles carry Lua source, never precompiled bytecode.

## 10. Engine kit (stock parts)

**The registry.** `engine/parts.toml` is the single source of truth for everything a game can select or tune: movement kinds, behaviours, tile properties, events, effects, sound presets, status elements and tuning keys. Each entry has an id, a one-line kid-readable description, its parameters with type, unit, default and allowed range, and the API version that introduced it.

Generated from the registry, never written by hand:
- the engine's lookup tables and parameter validation (a build step emits a C++ header);
- the rules `tg check` applies to `game.toml`;
- the API sheet and tuning-key reference that agents load;
- the simulator's sliders, menus and their ranges;
- the output grammar for the offline model (section 16).

Rules:
- A stock part does not exist until it is in the registry. Adding, removing or reshaping one changes the registry in the same commit as the code.
- CI fails if the registry and the engine disagree, or if any generated file is stale.
- The registry is never loaded into an agent's context whole. Agents query it with `tg part` (section 12).

The lists below are the initial contents.

- **Actors:** fixed pool (start at 64). Fields: kind, position, velocity, size, sprite, facing, health, timers, a small table for game data.
- **Movement kinds:**
  - `platformer`: gravity, variable jump height, a short grace period after leaving a ledge, a buffered jump press;
  - `topdown`: four or eight directions, slides along walls;
  - `vehicle`: heading and speed, drag by surface;
  - `paddle`: one axis, uses v1's steer-and-glide feel;
  - `grid`: one block per step.
- **Collision with the world:** resolve X then Y against tiles. Reports `on_ground`, `hit_wall`, `hit_ceiling`. Speed is capped below one block per tick.
- **Tile properties:** solid, one-way, hazard, ladder, ice, water, door, slow, bounce.
- **Behaviours:** walker, patroller, chaser, flyer, shooter, faller, follower, turret, pickup.
- **Events:** touch (by kind pair), hit wall, land, leave view, timer, defeated, level start, level clear.
- **Spawners:** timed, wave table, level markers activated as the camera nears. Automatic despawn well outside the view.
- **Effects:** explosion, sparkle, dust, trail, screen shake, flash, freeze-frame, LED flash. Particles use their own fixed pool.
- **Damage:** health, blink and brief invulnerability by default.
- **Flow:** title, play, level clear, game over; dialog boxes; level and room transitions (cut, fade, scroll).
- **Status:** score, best, lives, timer, item counters.
- **Save:** per-game key-value store with a size cap, plus best score.
- **Sound:** named presets (`jump`, `coin`, `hit`, `boom`); silent where there is no buzzer.
- **Road module:** a scanline pseudo-3D road for driving games, separate from tile maps.
- **Raw drawing:** rectangles, lines (with a thickness), filled convex polygons, sprites and text at block or pixel coordinates, for games that fit no stock part.

**Input.** v1 rules stand: two knobs (80 transitions per turn, clockwise positive), click on either knob is the action, a long press belongs to the OS (pause), holding both for 3 seconds opens the sleep prompt, handedness selects the main knob. Games read input through named controls. Each movement kind has a default mapping, overridable in `game.toml`:

| Kind | Default |
|---|---|
| platformer | tilt or main knob to run, click to jump |
| topdown | tilt to move, click to act |
| vehicle | main knob to steer, other knob for throttle, click to brake or boost |
| paddle | main knob |

**GATE (M4):** play-test the platformer and top-down defaults with real hands before fixing them.

## 11. Lua host

- Lua 5.4, built for 32-bit numbers. Libraries: base, `math`, `string`, `table` only. No `io`, `os`, `debug`, `package`, bytecode loading or `dofile`. `require` resolves only inside the game bundle.
- Limits: memory cap per game (start at 1 MB), per-tick instruction budget, incremental garbage collection stepped each tick.
- `math.random` is replaced by the engine's seeded generator.
- The API is small enough for a one-page sheet: `on.*`, `actor.*` and actor methods, `level.*`, `camera.*`, `input.*`, `score.*`, `fx.*`, `sound.*`, `save.*`, `draw.*`, `tuning.*`, `scene.*`.
- The API sheet is generated from the Lua binding table and the registry, and is the document agents load.

## 12. Tooling

One CLI, `tg`. Every command has `--json` output and exits non-zero on failure.

| Command | Purpose | Target |
|---|---|---|
| `tg verify [id]` | **The single gate.** Runs `check`, the game's `.steps` tests at both stage sizes, and (with no id, or when engine files changed) the engine tests. One pass/fail summary. | game-only: under 5 s |
| `tg new <id> --kind <starter>` | Scaffold a playable game | |
| `tg check [id]` | Validate formats, Lua syntax, API use, references, sizes, both stage sizes, content rules. Rejects a game change that touches files outside `games/<id>/`. Errors give file, line and a fix hint. | under 300 ms |
| `tg describe <id>` | Compact summary: kinds, sprites, levels and sizes, tuning, rules outline | under 400 tokens typical |
| `tg part [name]` | Look up the registry: list parts by group, or show one part's description, parameters, ranges and an example | under 200 tokens per part |
| `tg set <id> <key> <value>` | Change a tuning value without opening files | |
| `tg run <id> --steps <file> [--board a\|b] [--probe] [--shot out.png]` | Headless run, faster than real time. `--probe` prints state as text (positions, score, flags). | 10 s of play in under 2 s |
| `tg sim [id]` | Browser simulator with hot reload on file save | reload under 1 s |
| `tg push <id>` | Install a bundle on the console over USB and launch it | under 5 s |
| `tg send`, `tg shot`, `tg monitor` | v1 device commands, kept | |
| `tg test [id]` | Engine host tests and game `.steps` files | |
| `tg build`, `tg flash --board a\|b` | Firmware only; not needed to change a game | |
| `tg doctor` | Check the setup and explain fixes in plain words | |
| `tg bench` | Run the benchmark (section 15) | |

Notes:
- Prefer `--probe` text over screenshots for logic checks; it is far cheaper for an agent to read.
- `.steps` files hold input over time plus assertions, for example `expect player.on_ground`, `expect score >= 10`.
- USB push extends the v1 serial protocol with a chunked, checksummed file transfer.

**Tool server.** An MCP server exposing `verify`, `new`, `check`, `describe`, `part`, `set`, `run`, `push` and simulator control (`launch`, `send`, `screenshot`, `get_output`) as typed tools. It must work from Claude Code, Copilot, Codex and a local-model harness. It replaces the Copilot-only canvas as the primary path; the canvas may remain as a thin wrapper.

**Simulator editors (no model needed).**
- Sprite painter: 8×8 or 16×16 grid, palette, saves to `sprites.txt`.
- Level painter: place tiles and actors, saves to `levels.txt`.
- Tuning sliders bound to `[tuning]`, applied live, saved on request.
- A board switch (A or B) and v1's keyboard, mouse and USB-controller input.

## 13. Agent package

- **`AGENTS.md` is an index of about 30 lines.** It contains only:
  - one paragraph on what TinyGame is and who uses it;
  - the single gate: "`tg verify` must pass before you say a change is done";
  - "open the page that owns the task": one line per task type linking to its skill or doc (make or change a game, draw art, tune feel, engine work, each board's hardware, releases);
  - the few rules that apply to every task, each linking to its owning page.

  No command reference, hardware detail, workflow prose or harness-specific text. Those live in the owning skill.
- A small model may not follow links. The tool server therefore returns the relevant skill page with its results where that helps (for example, `check` errors include the fix hint, and `part` includes an example).
- **Skills, loaded on demand:** `make-a-game` (router), `platformer`, `topdown`, `shooter`, `driving`, `paddle-puzzle`, `art` (drawing sprites and tiles in text), `level-design`, `game-feel` (tuning recipes), `rules-lua` (API sheet and patterns), `debug`, `engine-dev` (C++ engine and tests), `hardware-board-a`, `hardware-board-b`.
- **Generated, never hand-edited:** API sheet and tuning-key reference (from the registry), command reference (from the CLI), pin tables (from board configs). CI fails if they are stale.
- **One source for skills,** with a sync script that writes copies for other harnesses. No symlinks. CI checks the copies match.
- **Behaviour rules that stay as prose:**
  - explain simply; the user is often about 10;
  - select a stock part before writing Lua, and write Lua before proposing an engine change;
  - games use original names, characters, art and levels, even when the request is "like" an existing game;
  - never report a failed step as success.
- **Permissions:** pre-approve read-only and game-scoped commands (`check`, `describe`, `run`, `set`, `new`, `sim`). Do not pre-approve `flash`, `push` or anything that imports a game from outside.

## 14. Security and kid safety

- Games are data plus sandboxed Lua. No game content is compiled or run natively on the host. `tg test` for games runs inside the WebAssembly engine.
- Importing a game runs `tg check` first and treats README text as untrusted content, not instructions.
- The dev serial channel can be compiled out of release firmware by a build flag.
- The repository gets a code of conduct and a contribution guide suited to a young audience. "Made by" defaults to a first name or nickname.
- Any game gallery requires review before publication.

## 15. Quality: tests, CI, benchmark

- **Engine tests** on the host with a small test harness that reports every failure. Golden-image tests for the renderer at both stage sizes.
- **Game tests:** every starter and ported game has `.steps` tests.
- **CI on every push:** `tg verify` for the whole repository; firmware build for both boards with warnings as errors; WebAssembly build; scaffold each starter and verify it; registry-against-engine check; generated-docs and skill-sync checks.
- **Decision records (`docs/decisions/NNNN-title.md`):** one short file per human decision, with the date, the question, the options considered, the choice, the evidence, and what would justify reopening it. Each GATE in section 17 ends by writing one. Skills and docs link to the record instead of restating the reasoning. Seed the folder at M0 with the decisions already made: games are data plus Lua on a C++ engine; rebuild as a fresh v2 tree; touch is not available to games; the stage is 10 blocks tall; MakeCode is deferred; the toolchain is not migrated during the rebuild.
- **Release on tag:** firmware binaries for both boards, hosted simulator, browser flasher page. Version derived from the tag.
- **Benchmark (`bench/`):** at least 200 tasks. Each has a starting game, a request in a child's words, and automatic graders using `check`, probes and screenshots. Report pass rate, tokens and wall time per task. Build this before tuning skills; it is the measure for both the hosted-model setup and the stretch goal.

## 16. Stretch: offline model

Do not start before M7 passes.

1. Use the benchmark as the fixed evaluation.
2. Generate training pairs synthetically: a large model writes request-and-edit pairs across the kit; keep only those where `check` and the graders pass. Do not train on children's conversations.
3. Fine-tune a small open coding model. Choose the base model at that time by benchmark score on the target laptop.
4. Run locally with output constrained to valid game-file edits, using the grammar generated from the registry, through the same tool server.
5. Add a local content filter for game text. Offline does not by itself mean safe.
6. Report honestly where it falls short; keep an adult-enabled hosted fallback.

## 17. Milestones

| # | Deliver | Acceptance |
|---|---|---|
| M0 | `v1.0` tag, `v2` branch, CI skeleton, repository layout, index-style `AGENTS.md`, seeded `docs/decisions/` | CI runs on an empty engine; v1 tag builds; the decisions listed in section 15 are recorded |
| M1 | Test card firmware for Board A: three block densities, candidate palettes, bevel on and off, text sizes | **GATE:** maintainer picks density and palette with kids on real hardware |
| M2 | Board abstraction; Board B bring-up after its schematic GATE; DMA display driver; carried-over drivers and logic with tests | Test card on both boards; present time measured and reported; sleep, wake, knobs and IMU verified on each |
| M3 | Engine core: fixed loop, renderer, tile cache, camera, stage layout, text; headless host; simulator shows engine output at both sizes | Golden-image tests pass; a scrolling test level runs at the reported frame rate on both boards |
| M4 | Bundle format, `tg pack`, Lua host with limits, actors, movement kinds, tile collision, events, input mapping; `engine/parts.toml` with its generators and CI check; `tg check`, `tg run`, `tg part`, `tg verify`, hot reload | A hand-written platformer level is playable in the simulator and on a board; an infinite loop in Lua produces the error screen; every part implemented so far is in the registry and CI fails when they disagree; **GATE:** control defaults play-tested |
| M5 | Remaining stock parts (each added through the registry), effects, flow, status, save, sound, road module; six starters | Each starter is playable from `tg new` with no edits, passes `tg verify`, and has `.steps` tests |
| M6 | Port Brick Breaker, Rock Rain and TiltBlock | All three run from data and Lua. Any drop to C++ is reported as a kit gap, with a proposal |
| M7 | `tg push`, `describe`, `set`, `doctor`; tool server; simulator editors and sliders driven by the registry; full skill set behind the `AGENTS.md` index; benchmark with baseline results | A new game goes from request to running on a console without a firmware build; benchmark report published |
| M8 | Release pipeline, hosted simulator, browser flasher, conduct and contribution docs | A tagged release can be flashed and played with no toolchain installed |
| Stretch | Section 16 | Local model's benchmark score reported against the hosted baseline |

## 18. Assumptions and open items

**Assumed (confirm with the maintainer if any is wrong)**
- Lua is accepted for game rules.
- Touch is not available to games.
- Board B is fitted with the same two knobs and click switches.

**UNVERIFIED (confirm before relying on them)**
- Board B's buzzer, RTC, IMU part, panel offsets and free RTC-capable GPIOs.
- That a 20 ms full-frame present is achievable on both boards.
- That TiltBlock's isometric rendering can be expressed with the kit's raw drawing calls at an acceptable frame rate.
- That Lua with the stated limits holds 60 ticks per second with 64 actors on the ESP32-S3.

**Open risks**
- Arduino core 2.x is frozen; a later move to ESP-IDF or a community fork is likely.
- The block size is physically small (under 2 mm); M1 exists to catch this early.
- A two-knob-and-tilt platformer may not feel right; M4's gate exists to catch this.
