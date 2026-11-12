# TinyGame v2: rebuild specification

**Audience:** the maintainer and coding agents implementing the rebuild.
**Revision:** 2026-11-11, following specification review and maintainer decisions.
**Baseline:** TinyGame v1 at `83620e31e61abe87269f9e69d3f932745b6bf063`.
**Status:** ready as a gated rebuild baseline, not a claim that the proposed runtime or performance has been proven. This review does not itself authorize implementation, tags, merges, deployment, or flashing.

## 0. Authority, evidence, and change control

- **CONFIRMED** means the maintainer explicitly chose the requirement.
- **REQUIRED** means an implementation must satisfy the contract before the relevant milestone passes.
- **CANDIDATE** means an initial technical choice to measure, not a promise or established fact.
- **GATE** means dependent work stops until the stated evidence and decision exist.
- **UNVERIFIED** means no claim of success is permitted without the specified validation.

Follow the milestones in section 15. Work inside the experimental branch; leave v1 and the parked board-support branch intact. No repository reset, release tag, or hardware operation is implied by reading this document.

Use the supplied v1 source, vendor schematics, and actual measurements as evidence. Never guess a pin or infer a board model from a USB port name. Preserve failures and unknowns in reports.

Record significant decisions in `docs/decisions/NNNN-title.md`: question, options, choice, evidence, date, and what would justify reopening it. Seed only decisions actually made, not provisional values. An implementation may change a candidate within its stated experiment; changing a confirmed requirement or frozen API requires a new decision.

## 1. Product and confirmed decisions

TinyGame is a pocket console on which children around ten and adults create games by talking to a coding agent, using small editors, or tuning existing mechanics directly.

**CONFIRMED architecture:** games are text assets and settings plus sandboxed Lua 5.4 rules, executed by a portable C++ engine shared by firmware and WebAssembly. Game changes do not require recompiling firmware.

**CONFIRMED scope:**

- Support the existing Waveshare ESP32-S3-LCD-1.47B and ESP32-S3-LCD-1.69.
- Touch is entirely out of scope: no game input, shell integration, capability plumbing, editor support, or touch-specific tests.
- Redesign TiltBlock for the new front-on 2D kit while retaining its core game idea. Preserving its old perspective cube, projection, or depth rendering is not a requirement.
- Games may use individual button presses and holds. Pause moves to a two-button gesture; the three-second two-button sleep gesture remains.
- Use most of both screens for gameplay, with compact HUDs and board-adapted views. Do not reserve an 80-pixel status panel on the 1.69 or require a universal ten-block play height.
- The complete feature set remains the first-release target: engine, both boards, game format, kit, six starters, three games, CLI, tool server, simulator editors, guidance, CI, at least 200 benchmark tasks, and release pipeline.
- Prove a narrow creator loop early without treating that milestone as a reduction of release scope.

**Goals:** distinctive and readable graphics; fast warm iteration; compact, useful agent tools; bounded execution of shared games; a future path to local models.

**Non-goals:** console networking, touch, MakeCode import, a block-language editor, emulator compatibility, a game gallery, and model training during the core rebuild.

## 2. First-release content and boundaries

The six starter deliverables are:

1. Platformer.
2. Top-down adventure.
3. Shooter.
4. Driving game.
5. Paddle game.
6. Grid puzzle.

Each must scaffold as a complete playable game, not a blank demo. Their detailed mechanics are frozen through the kit/control gates, not inferred from genre labels.

The three game deliverables are Brick Breaker, Rock Rain, and the 2D redesign of TiltBlock. Brick Breaker and Rock Rain retain their recognizable rules and controls; differences caused by the new presentation or normalized units must be recorded and play-tested.

For TiltBlock, retain the core loop of moving over tiles, marking safe tiles, avoiding hazards, and progressing through layers/boards. Before implementing its full redesign, review a small playable 2D treatment covering progression, feedback, and the intended knob/tilt controls. Legacy perspective controls or individual special mechanics are not automatically binding on the redesign.

Buzzer support is conditional on a verified board revision and safe circuit. Do not add hardware or drive unidentified pins to satisfy it. The portable sound-event API must work when a board explicitly has no enabled sound output.

## 3. Repository, provenance, and toolchain

Use the existing experimental branch as the work surface. Decide its final branch name when implementation starts; branch naming must not change the baseline silently.

Before replacing the implementation:

- Record the exact v1 SHA and verify its build.
- Preserve it with an immutable release tag when the maintainer authorizes that operation. Proposed form: `v1.0.0`, consistent with semantic versioning; do not overwrite an existing tag.
- Keep license notices, useful fixtures, history, and provenance. A fresh implementation tree is not permission to discard them.
- Establish CI in the first implementation milestone; enable real gates incrementally as their tools exist.

Keep PlatformIO `espressif32 @ 6.12.0`, its pinned Arduino core 2.x, C++17, and LVGL 9.6.0. Pin Lua 5.4's exact patch release, Emscripten, and generators before using them. Do not migrate the firmware toolchain speculatively. A demonstrated blocker returns for a decision.

### Source reuse

Reuse tested v1 drivers and state machines behind new interfaces rather than rewriting them gratuitously:

- Board A pins, panel initialization, the row-axis 34-pixel offset, and the verified RGB565 byte-order setup.
- IMU health handling, encoder/debounce logic, power/wake, settings, persistence, status-light logic, and serial protocol building blocks.
- Simulator HAL concepts, the pinned WASM build machinery, and applicable CLI functionality.

Review their dependencies and integration assumptions. They are sources to adapt, not a promise that every file stays unchanged. Input ownership and display ownership explicitly change in v2.

The unmerged board-support branch through `d381f75683e805eb76d20b5eb7220982c6bd6669` is a reference for measured Board B behavior, GPIO43 handling, and TinyUSB upload fixes. Incorporate only reviewed, relevant fixes with tests. Do not merge that branch as a prerequisite.

### Layout

```text
engine/                 portable C++ engine and Lua host
engine/parts.yaml       kit declarations and parameter schemas
hal/boards/<board>/     board profiles and verified hardware configuration
hal/                    device adapters
os/                     shell, power, transport, storage orchestration, main loop
sim/                    browser and headless WASM hosts
games/<id>/             game content only
starters/               six playable templates
tools/                  CLI, packaging, validation, generators, MCP server
bench/                  tasks, graders, fixtures, result schema
docs/decisions/         human decisions with evidence
docs/formats/           normative versioned package, protocol, and replay formats
docs/generated/         generated reference output
.claude/skills/         canonical skill sources, synced to supported harnesses
```

## 4. Boards and physical acceptance

| Property | Board A | Board B |
|---|---|---|
| Model | ESP32-S3-LCD-1.47B | ESP32-S3-LCD-1.69, the existing non-Touch-named target |
| Canonical software ID | `147b` | `169` |
| Native landscape display | 320x172 | 280x240 |
| Mounting | existing verified mounting | physical top points right in use |
| Left encoder A/B/button | GPIO9 / GPIO8 / GPIO7 | GPIO43 / GPIO2 / GPIO3 |
| Right encoder A/B/button | GPIO4 / GPIO5 / GPIO6 | GPIO16 / GPIO17 / GPIO18 |
| IMU bus SDA/SCL | GPIO48 / GPIO47 | GPIO11 / GPIO10 |
| Motion sensor | QMI8658 at 0x6B | QMI8658 at 0x6B |
| RGB LED | supported on GPIO38 | not supported; do not drive GPIO38 as an LED |

These are reference configurations, not permission to omit schematic and assembled-device checks. Board profiles own pins, display setup, sensor transforms, input polarity, safe drawing insets, wake configuration, and supported capabilities.

For Board B, encoder commons remain grounded and left A uses the U0TXD/GPIO43 header. GPIO11 must remain available to onboard I2C. Native USB serial must not repurpose that encoder pin for UART output. Correct GPIO register-bank handling is required.

Board B display orientation/colors/backlight, clockwise-positive encoder direction, and downhill tilt/level centering were physically confirmed in the earlier work. Exact measured full-revolution counts, click feel, gyro signs, sleep/wake, GPIO3 strap behavior, USB-host auto-off, and battery behavior were not fully accepted. Do not relabel old partial evidence as a complete v2 hardware pass.

**GATE H:** identify the actual board revision before enabling buzzer or revision-dependent power pins. No speculative writes to SYS_EN/SYS_OUT, RTC interrupt, buzzer, or other revision-dependent connections.

**REQUIRED before hardware acceptance:** check both boards' boot, panel coverage/order/colors, brightness, physical knob polarity/count behavior/clicks, tilt axes/level reference, any exposed gyro mapping, pause/sleep release safety, No cancellation, wake from either button, and USB host versus power-only auto-off behavior.

Battery startup/latching and sleep-current claims require measurements of those states. Unverified optional battery features remain explicitly unsupported rather than being presented as tested.

## 5. Architecture and ownership

```text
Text game -> pack and validate -> versioned .tgb bundle
                                      |
                    portable engine + bounded Lua host
                         /                       \
                  device host                WASM host
               OS + HAL + display        browser / headless
```

The portable engine has no Arduino, LVGL, browser, filesystem, or network dependencies. Its host supplies normalized input, storage operations through a bounded interface, and explicit time progression. The engine produces rendering output and bounded LED/sound events.

LVGL owns the system shell. The engine owns game rendering. One HAL presentation interface arbitrates both: finish or cancel an in-flight transfer safely, switch owners, and redraw correctly on return. Do not let LVGL and the game manipulate panel transfers independently.

Pause freezes game simulation. It does not stop OS input, power management, USB transport, or the watchdog. Returning from a shell or gesture releases/suppresses stale game input until the required fresh-input boundary.

The main loop has explicit input, transport, power, simulation/shell, presentation, and pacing phases. It yields while idle.

### Timing and deterministic execution

**Targets:** fixed 60 Hz gameplay simulation and 30 fps presentation. Rendering must not determine physics speed or consume gameplay randomness.

Replays specify the exact engine/API version, bundle, seed, board profile, initial saves, and tick-indexed normalized input. Identical fixtures must produce identical gameplay state/events on repeated runs. Establish cross-native/WASM conformance before API freeze; document the selected numeric and rounding model.

Define a stable RNG, deterministic actor/event ordering, and separate cosmetic and gameplay random streams. Do not depend on unordered Lua table traversal.

Input deltas/edges are consumed once, not repeated across catch-up ticks. Specify bounded catch-up, overload reporting, and event distribution. Do not silently drop simulation time while claiming the target was met.

Pause, dialogs, loading, hit-stop, and OS sleep have explicit clock policies. Tests use controlled save fixtures; persistent best scores must not contaminate replay results.

## 6. Controls and system gestures

The hardware exposes two incremental knobs, two debounced buttons, and motion. Clockwise is positive. Treat 80 electrical transitions per revolution as the nominal EC11 profile value, not a substitute for measuring the fitted part.

Handedness selects the main knob. Games consume named controls with documented units, edges, and hold semantics, not raw GPIO values or host key events.

**CONFIRMED:** individual button presses/holds may reach games. A solo hold no longer opens Pause. Existing game controls and OS gestures therefore cannot be copied unchanged.

**CANDIDATE two-button policy for the early physical playtest:**

- A solo press/hold is available to the configured game action.
- When both buttons become down, the OS captures the chord and freezes gameplay immediately.
- Releasing before three continuous seconds with both down opens Pause; keep input captured until both buttons have been released and debounced.
- Holding both continuously for three seconds opens the sleep confirmation, default Yes.
- Sleep confirmation requires a fresh input after full release; the initiating hold cannot confirm it.
- No restores the original state from before the captured gesture, not a transient gesture screen.
- A game action already delivered before the second button was pressed is not retroactively undone. Document and play-test this tradeoff.
- The chord is reserved; a game cannot bind simultaneous button holds to an action that competes with it.

**GATE C:** play-test gesture timing and action responsiveness on both boards before freezing API 1. If this policy is awkward, revise it explicitly; do not quietly restore single-button hold-to-pause.

Initial movement-profile experiments:

| Profile | Control contract to prove |
|---|---|
| Platformer | responsive press-to-jump, held jump for variable height; compare knob and tilt horizontal control before selecting the default |
| Top-down | calibrated tilt with dead zone; action press/hold defined by the game |
| Vehicle | main knob steering, other knob throttle; candidate default held brake, with boost an explicit alternative binding |
| Paddle | main-knob steer-and-glide feel derived from v1, normalized to world travel |
| Grid | main-knob discrete selection/steps and explicit action; specify step/repeat behavior |
| Shooter | select a declared paddle/top-down/vehicle-based control profile rather than assuming a new physical input |

Freeze defaults only after real-hand trials. Preserve physical left/right access for games that deliberately use both knobs, independent of handedness.

## 7. Display, world units, and visual identity

Games use world/block units for positions, sizes, speeds, collision, and drawing. The renderer converts to screen pixels; game logic must not depend on panel pixel dimensions.

**CANDIDATE density:** 8x8 art pixels per block, rendered at scale 2. Sprites may be 8x8 or 16x16; a typical character may be two blocks tall. These are test-card candidates, not frozen stage dimensions.

**GATE V:** run a test card on both boards before dependent layout work. Compare densities, palettes, bevels, motion, small/large text, compact HUDs, and rounded-corner safety with the maintainer and intended users.

Use the native display area minus a compact, corner-safe HUD. There is no fixed ten-block height and no special 80-pixel status panel. Publish the resulting minimum usable world viewport and exact safe rectangles after Gate V.

Scrolling games share one authored world. A larger viewport may reveal more of it. Define gameplay-relevant spawn/activation/despawn regions independently of incidental camera size unless a game explicitly declares and tests a viewport-dependent rule.

Fixed-screen games declare their world bounds. Fit them without distortion or cropping required content; fill unused margins decoratively. "Use most of the screen" does not authorize stretching artwork or changing ball-loss/collision boundaries between boards.

The standard camera follows a target with a dead zone and clamps to world bounds. Status elements are declared; the engine lays them out. Test both profiles, not an imaginary single common resolution.

### Style

- Front-on 2D block art with a restrained bevel.
- One platform palette of sixteen authored indices; index 0 always transparent.
- Per-game palette overrides cannot make index 0 opaque. Engine-generated shade/highlight colors may exceed the sixteen authored colors.
- Flat-color source art; optional procedural squash, stretch, bob, flash, blink, and wobble. Visual deformation does not silently change collision shapes.
- Optional second walking frame; backgrounds may be solid, banded, or a supported parallax layer.
- Shared crisp ASCII pixel font in two approved sizes. Shell retains the existing dark identity; games may use light backgrounds.

Raw game drawing, where necessary, uses bounded world-space primitives through the engine. Pixel-addressed panel/debug functions are not part of game API 1. No legacy 3D renderer is required for TiltBlock.

### Rendering and performance

Use RGB565 rendering, pre-rendered tile caches, clipped blits, and a measured presentation backend. Evaluate the pinned stack's SPI/DMA path and LovyanGFX if needed; retain the toolchain.

The largest single framebuffer is 134,400 bytes. Account separately for double buffering, DMA-capable internal memory/staging, PSRAM, Lua, assets, LVGL, queues, and stacks. Never render into an in-flight transfer buffer.

**Target:** completed full-frame presentation within 20 ms on both boards. At 40 MHz SPI the raw transfer alone exceeds this target; at 80 MHz it may fit, but hardware and memory-path measurements are required. Enqueue time is not presentation time.

Measure logic, Lua, rendering, transfer completion, complete-frame latency, input latency, and sustained/peak resource use on representative scenes. Report median and tail behavior and the workload.

A missed budget fails the performance gate. Propose a design change or revised budget and get a decision; merely reporting a lower rate does not pass.

## 8. Game format and bundle contract

```text
games/<id>/
  game.toml
  sprites.txt
  tiles.txt
  levels.txt
  rules.lua             optional
  modules/*.lua         optional, bundle-local only
  README.md
  test/*.steps
```

All authoring inputs are text. Devices and simulators load packed bundles; only bounded Lua source is interpreted at runtime.

Game IDs match `[a-z][a-z0-9_]{0,14}` and must agree across the folder, manifest, and bundle. Names contain 1-13 printable ASCII characters. Author metadata is optional and may use a nickname; do not require a child's full name.

Separate `starter` metadata from actor `kinds`. A starter identifies the template; explicit runtime declarations own mechanics and controls. This avoids the original invalid scalar `kind` plus `[kind.slime]` TOML.

Syntactically valid manifest excerpt:

```toml
api = 1
id = "cave_hop"
name = "CAVE HOP"
starter = "platformer"
view = "scroll"

[tuning]
gravity = 30
jump_height = 3
run_speed = 6

[kinds.slime]
sprite = "slime"
behaviour = "walker"
speed = 2
on_touch_player = "hurt"
```

This excerpt requires corresponding registered parts and referenced assets; it is not a complete game. M3 must supply a complete executable format fixture. Normative examples must be tested, not pseudocode with unexplained omissions.

Sprite/tile art uses one character per authored pixel: `.` or `0` for transparent index 0, and hexadecimal digits for other indices. Reject wrong row lengths, missing rows, invalid dimensions, and duplicate identifiers. Specify frames, namespaces, level layers, legends, actor spawn anchors, and tile properties in versioned format documents before freezing the parser.

Level legends must distinguish tile references from actor spawn references. Actor anchors/collision bounds and world origin/axes must be explicit, including how a two-block character sits on a one-block grid.

### Container and compatibility

Before bundle loader implementation is accepted, publish `docs/formats/tgb-v1.md` defining magic, format version, endianness, section layout, offsets, lengths, checksums, content limits, and Lua encoding. Maintain golden valid/invalid bundles shared by packer and all loaders.

Validate every section and referenced resource before exposing it to game execution. Reject truncated/overlapping/out-of-range sections, unsupported versions, invalid encodings, oversized resources, and invalid numeric values. Do not execute native extensions or accept precompiled Lua bytecode.

Firmware SemVer, game API version, bundle format version, game content version, and save schema version are distinct. Specify compatibility and errors explicitly; never treat an unfamiliar version as the current one.

A fixed-point/float representation, resource-cap table, and binary wire layout are technical gate deliverables, not facts already established by this document.

## 9. Kit and execution semantics

`engine/parts.yaml` owns selectable part IDs, descriptions, parameter types/units/defaults/ranges, compatibility metadata, and declared bindings. Generate validation tables, editor controls, and part references from it.

Native code implements the registered behavior. CI checks the declared/implemented binding sets and schemas in both directions. Generated metadata is not proof that behavior is correct; each mechanic still needs behavior tests.

Initial kit scope:

- Actors: candidate pool of 64, kind, transform, velocity, collider, sprite, health, timers, bounded game data.
- Movement: platformer, top-down, vehicle, paddle, grid; ball/projectile behavior sufficient for the required games.
- World tiles: solid, one-way, hazard, ladder, ice, water, door, slow, bounce.
- Behaviors: walker, patroller, chaser, flyer, shooter, faller, follower, turret, pickup.
- Events: touch, wall hit, land, leave view, timer, defeated, level start/clear.
- Spawning: timed, waves, authored markers, deterministic activation/despawn policies.
- Effects: explosion, sparkle, dust, trail, shake, flash, freeze-frame, LED flash.
- Damage: health, visible feedback, bounded invulnerability.
- Flow: title, play, clear, game over, dialogs, and cut/fade/scroll transitions.
- Status: score, best, lives, timer, item counters.
- Save: bounded per-game data and best score.
- Sound: named presets; explicit no-output capability on unsupported hardware.
- Road: a generic scanline driving module, with its own acceptance game and budget.
- Bounded raw world drawing for mechanics not covered by declarative parts.

### Required semantics before API freeze

- Stable generational actor handles; no stale-handle access after reuse.
- Defined spawn/removal points, event ordering, overlap begin/stay/end, and callback lifetimes.
- Mutations during callbacks are queued or otherwise deterministic; a full queue is an explicit fault.
- Pool exhaustion policies are documented. Do not silently fail gameplay spawns; decorative dropping may be allowed under a declared policy.
- Timer units and lifetime are explicit.
- Tile collision includes shape, one-way, step/ground, and axis-order rules.
- Actor/projectile collision addresses tunneling; "less than one tile per tick" is not a universal solution.
- Paddle/ball reflection has tested behavior rather than borrowing a platformer's wall slide.
- Rendering deformation and camera shake do not alter collision state.

Lua example avoiding use after removal:

```lua
on.touch("player", "coin", function(p, c)
  local x, y = c.x, c.y
  c:remove()
  score.add(10)
  fx.sparkle(x, y)
end)
```

Freeze whether `touch` denotes overlap start or another phase; do not award the pickup repeatedly on sustained overlap.

## 10. Lua limits, containment, and recovery

Use the pinned Lua 5.4 build. A 32-bit numeric build is the initial candidate; cross-host determinism and behavior tests must establish the final numeric policy.

Allow only audited subsets of base, math, string, and table functionality. No host IO, OS, debug, package loading, native modules, arbitrary filesystem `require`, bytecode loading, or unbounded output. Replace randomness with the engine generator.

Implement a bounded bundle-local module loader. Loading, initialization, callbacks, ticks, render callbacks, native bindings, and event fan-out all receive limits. Candidate Lua heap cap: 1 MiB per game; asset/pool/DMA budgets are additional and explicitly bounded.

Instruction hooks alone do not bound work inside native library functions. Limit those operations and provide an independent watchdog/timeout appropriate to each host. Browser games run off the UI thread; headless game runs are killable. Game tests execute inside the bounded WASM runtime.

Malformed bundles, infinite loops, excessive allocation, event storms, stale handles, and renderer-limit violations must fail without executing additional game callbacks. Release game resources and return to a usable launcher with a clear, kid-readable error and source location where available.

The hardware watchdog is a last-resort recovery path, not normal script control flow. Store a bounded running-game identifier with validity/reset-reason handling, and clear it on normal exit. Do not misreport a normal reset as a game crash or automatically enter a crash loop.

**GATE R:** before API freeze, demonstrate the resource-cap table and failure behavior with adversarial and representative workloads on WASM and hardware. No resource class may remain effectively unbounded.

## 11. Storage, installing games, and USB

Use a dedicated LittleFS data partition for installed bundles and per-game saves. Preserve built-in starter/catalog content independently of writable game updates.

Measure the partition budget before freezing it. Retain the proposed two app slots if the measured layout permits, but do not build a networking/OTA feature merely because slots exist. Document what updates preserve and what rollback is actually supported.

### Installation

- Chunked, bounded, checksummed transfer with a versioned protocol.
- Confirm target identity/profile and API compatibility. Chip/USB identity alone does not distinguish these two assembled board models.
- Stage a complete bundle, validate it, and atomically replace the catalog entry.
- Preserve the old working game on interrupted transfer, failed validation, full storage, or power loss.
- Budget space for old and staged generations; define replacement, deletion, duplicate IDs, and built-in-game precedence.
- Define acknowledgments, retries, timeouts, cancellation, and serialized port ownership.
- Do not launch an incomplete or unvalidated package.

Production firmware must retain a supported game-installation path. Privileged debugging, input injection, and profiling may be disabled separately; compiling out a shared channel must not remove the core install workflow accidentally.

### Saves and hot reload

Per-game saves have namespaced keys, bounded types/size, schema versions, atomic writes, and a flash-wear policy. Define when changes commit and how reset/delete/migration works.

Initially, hot reload replaces a validated complete generation and restarts the game. It clears held input and transient runtime state, preserves the defined save data, and leaves the previous runnable generation available when a build fails. Live Lua-state migration is not required.

### Firmware flashing and host detection

Preserve native USB/TinyUSB DTR-based host-open detection so auto-off distinguishes an open computer connection from USB power alone.

Support the pinned core's 1200-baud entry to the ROM downloader, track re-enumerated device identity/ports, and handle known detach behavior without swallowing unrelated failures. Carry forward the reviewed macOS `termios.error` correction. Test both normal auto-entry and manual recovery.

Flashing requires an explicit board target. Never infer it from the port or flash the default build to whatever is connected. Firmware-reported profile describes installed software, not proof of the physical board model.

## 12. CLI, simulator, editors, and tool server

One CLI: `tg`. Machine-readable output is versioned and bounded; failures exit nonzero. JSON modes keep diagnostics separate from result output, and streaming commands use a documented record format.

| Command | Contract |
|---|---|
| `tg new <id> --kind <starter>` | Scaffold one of the six complete playable templates inside the allowed game root |
| `tg pack [id]` | Validate and produce reproducible bundles |
| `tg check [id]` | Static syntax/schema/reference/size validation; do not execute game rules |
| `tg verify [id]` | Authoritative automated gate: static checks, packing, bounded game scenarios on both profiles, plus engine checks when required |
| `tg describe <id>` | Compact content/control/part summary |
| `tg part [name]` | Bounded registry lookup with typed parameters and example |
| `tg set <id> <key> <value>` | Typed validated edit through the same schema; atomic write |
| `tg run <id> --steps <file> --board 147b|169` | Bounded faster-than-real-time WASM run with probes and optional screenshot |
| `tg sim [id] --board 147b|169` | Actual WASM engine, validated hot reload, editor support |
| `tg push <id>` | Confirmed install and optional launch using the bounded device protocol |
| `tg send`, `tg shot`, `tg monitor` | Retained device workflows with explicit capability/version handling |
| `tg test [id]` | Engine and game test entry points |
| `tg build`, `tg flash --board 147b|169` | Firmware workflows; flashing always names its target |
| `tg doctor` | Diagnose versions, ports, configuration, and missing prerequisites without inventing hardware facts |
| `tg bench` | Execute reproducible benchmark tasks and publish structured results |

An unqualified `tg verify` runs the full automated gate. A scoped invocation must not skip engine checks when engine/shared tooling changed. Define its diff baseline; without reliable change information, run the broader gate or require an explicit mode, not a silent guess.

`.steps` has a versioned grammar: seed/save fixtures, tick-indexed input, declared probes, ordered assertions, and finite execution limits. It is not shell code or arbitrary host evaluation. Validate its grammar before executing it.

Static checks cannot prove arbitrary Lua games completable or appropriate for children. Required automated evidence and required human playtests are different gates; passing `tg verify` does not waive an outstanding physical gate.

### Warm iteration targets

- Static checks under 300 ms for a representative game.
- Warm simulator save-to-playable reload under 1 s.
- Game-only verification under 5 s.
- Ten seconds of scripted play in under 2 s headless.
- Representative game transfer/validation/launch under 5 s.
- Typical describe output under 400 tokens; individual part lookup under 200 tokens.

Name the reference host, workload/bundle size, tokenizer, warm/cold state, and measurement procedure. Dependency installation and cold firmware builds are excluded from warm-edit targets. A missed target requires an explicit disposition.

### Simulator and editors

Use the actual shared engine and Lua host. Support native board switching, v1 keyboard/mouse/USB-controller inputs, normalized tilt, screenshots with truthful native dimensions, and explicit capabilities. Switching/reloading releases all held input and stale controllers.

Editors: sprite painter, tile/level painter, and schema-driven tuning controls. Sprite/level changes save atomically to text. Tuning may preview live through the typed API and saves on request. Handle external file changes rather than overwriting them.

### MCP and permissions

Expose typed game operations and simulator launch/send/screenshot/output through one tool server usable by Claude Code, Copilot, Codex, and a local-model harness. The Copilot canvas may remain a thin optional wrapper.

Use the same validated core operations as the CLI. Constrain paths, reject traversal/symlink escapes, enforce per-task write boundaries, and serialize conflicting edits/USB access.

Browser editor/control endpoints bind to loopback with explicit origin/authentication protections. Do not expose host execution capabilities to game content.

Pre-approval belongs to the user and harness. Request only narrowly scoped permissions. Imports, hardware push, firmware flash, releases, and external services need their appropriate explicit authorization.

## 13. Agent guidance and safety

Keep `AGENTS.md` about thirty lines: identity/audience, the automated gate, task-to-owning-doc links, and a few universal rules.

Canonical skills live in one location with tested generated/synced copies for other harnesses; no symlinks. Cover game creation, each genre, art, level design, game feel, Lua rules, debugging, engine work, and both actual board profiles.

The registry owns part schemas; the declared binding metadata owns API shapes; CLI definitions own command help; board profiles own pin tables. Generate references and reject stale/contradictory output in CI. A one-page API sheet is a quick reference, not a substitute for complete typed contracts.

Tools return relevant compact guidance and fix hints without dumping the entire registry into model context.

Treat all imported game text as untrusted data. A file-scoped checker is not a sandbox and cannot grant tools permission. Games may not run native host code, spawn processes, read host files, or use networks.

Use original names, art, characters, and levels. Explain errors clearly for young creators. Do not train on children's conversations or require identifying information. Contribution/conduct guidance is part of the full release.

## 14. Tests, benchmarks, and releases

### Automated evidence

- Native engine unit tests for deterministic logic and bounds.
- Cross-host gameplay-state traces, with profile-aware rendering expectations.
- Golden renderer fixtures at both native resolutions with controlled updates.
- Valid/invalid manifest, asset, bundle, protocol, and replay fixtures.
- Sandbox exhaustion and recovery cases.
- Interrupt/full-storage/install/rollback/save tests.
- End-to-end tests for every starter and game through the actual WASM engine.
- Firmware builds for both profiles; project warnings are errors with explicit treatment of pinned vendor diagnostics.
- WASM builds, scaffold-and-verify for all six starters, registry/binding checks, generated-reference checks, and skill-sync checks.

Do not conflate visual snapshots with collision correctness, simulation speed with hardware performance, or firmware compilation with device acceptance.

### Benchmark

The first release includes at least 200 tasks with starting games, child-language requests, declared allowed files, deterministic graders, and performance/token accounting.

Begin a small evaluation set during the creator-loop prototype. Assemble and freeze the full baseline before claiming improvements from skill tuning. Keep evaluation tasks separate from tuning and any later synthetic training data.

Graders check requested behavior, not merely successful parsing. Report failures, uncertainty, token cost, and wall time. Hosted-model runs are not an implied credential, spending, or data-sharing authorization.

### Release

Version firmware using SemVer derived from release tags, with explicit development-build behavior. Keep the friendly firmware label; hashes may remain diagnostic metadata rather than the human-facing release number.

Release artifacts: both firmware images and required manifests/partitions, compatible bundled games, hosted simulator, and browser flasher/install instructions. State supported browsers/hosts and a fallback or explicit limitation when Web Serial is unavailable.

Verify clean-machine installation and data preservation across supported updates. Do not claim OTA rollback, battery behavior, or a board revision that was not actually supported and tested.

## 15. Milestones and stop conditions

All milestones contribute to the full first release. A failed gate is recorded and resolved; it is not silently removed to advance the schedule.

| Milestone | Deliverable | Acceptance / stop condition |
|---|---|---|
| M0: baseline and contracts | v1 provenance, authorized preservation tag, experimental layout, minimal CI, decision records, toolchain pins | v1 remains buildable; decisions in section 1 are recorded; no unreviewed branch merge or toolchain migration |
| M1: both-board foundation | verified profiles, reused HAL/state logic, dual-board test cards, display ownership prototype | Gate H and Gate V; physical controls/sleep/wake evidence for both boards; measured memory/display budgets; unsupported peripherals explicitly disabled |
| M2: creator-loop spike | smallest text game, pack/load format, bounded Lua, native/WASM rendering, minimal validated USB transfer and launch | change a game without firmware rebuild; working preview and hardware play; bad Lua and failed install recover; Gate R feasibility demonstrated before broad kit investment |
| M3: core contract freeze | versioned formats, registry/bindings, engine lifecycle, replay semantics, controls, `pack/check/run/part/verify` | complete executable fixtures; cross-host traces; bounded events/collision cases; Gate C; limits and overload policy published; performance gate passes or an approved revision exists |
| M4: complete kit and six starters | movement, behaviors, world, effects, flow, status, saves, sound abstraction, road module | each starter is playable without edits on both profiles; `.steps` tests; representative resource/performance measurements; every implemented part is registered |
| M5: three games | Brick Breaker, Rock Rain, 2D TiltBlock | playable 2D TiltBlock concept accepted before full conversion; each game uses data/Lua and passes both-profile automated and human checks; any native kit gap is proposed explicitly |
| M6: creator tools | robust install UX, CLI completion, editors/sliders, MCP, optional canvas, generated guidance | file scope and conflicts handled; host integration smoke tests; game request-to-console loop works through supported harnesses without rebuilding firmware |
| M7: full release acceptance | 200-task baseline, completed skills, full automated/physical matrix, failure/upgrade tests | all mandatory scope present; benchmark report honest; no significant open hardware/resource/safety blocker hidden by automated success |
| M8: distribution | tagged artifacts, hosted simulator, browser flasher, contribution/conduct docs | clean-machine install/upgrade succeeds on supported combinations; published limitations match evidence |

## 16. Remaining gates and known uncertainty

These are deliberately scheduled experiments, not architectural decisions the coding agent should guess:

- Physical density, palette, font readability, HUD insets, and resulting minimum viewport.
- Exact gesture timing and control feel on real knobs.
- DMA memory/presentation strategy and sustained whole-frame performance on the frozen toolchain.
- Lua numeric model, limits, and native/WASM deterministic conformance.
- Final container/protocol/replay encodings and complete fixtures.
- Practical game/content sizes, partition budgets, install timing, and save durability.
- Board-specific wake/strap/gyro/USB/power behavior and optional safe sound capability.
- The playable treatment of 2D TiltBlock.

Every gate names an owner, evidence, decision, and dependent milestone in its decision record. A gate can resolve to a smaller technical limit, changed implementation, or explicit product revision; it cannot resolve to an undocumented success-shaped fallback.

## 17. Stretch: local model

Do not begin until M7 passes and the maintainer separately authorizes it.

Use the held-out benchmark to compare suitable open models on the actual target laptop. Synthetic training examples must be separate from evaluation tasks and pass behavioral graders; do not train on children's conversations.

Constrain edits through typed game operations and the same tool server. Registry-derived grammars can constrain declared schemas, not prove arbitrary Lua semantics. Keep runtime validation/sandboxing and local text safeguards.

Report limitations honestly. Any hosted fallback is explicitly adult-enabled, not an automatic upload of local content.

## 18. Change summary from the supplied proposal

- Confirmed the data/Lua/C++ architecture and full first-release scope.
- Replaced the old 3D-port obligation with an explicit 2D TiltBlock redesign.
- Corrected Board B identity and separated measured facts from outstanding tests.
- Removed touch entirely.
- Freed single-button holds for games and introduced a gated two-button OS gesture policy.
- Replaced the fixed ten-block/large-status-panel layout with compact HUDs and native adapted views.
- Fixed the TOML namespace collision; added explicit tile/module and format-contract requirements.
- Brought packaging, sandbox, device installation, and runtime/performance risks into the early creator-loop spike.
- Added lifecycle, determinism, collision, storage, permissions, and release/install boundaries.
- Preserved the parked branch and v1, while retaining their useful measured evidence.
