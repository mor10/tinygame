# TinyGame v2 implementation roadmap

This roadmap sequences the full first release in the [v2 rebuild specification](tinygame-v2-rebuild-spec.md). It optimizes for **early actionable physical evidence**, not fewer safety or acceptance gates. The release scope remains mandatory; the early diagnostic firmware, test card, and creator-loop spike are risk-retirement steps, not scope cuts.

## Operating rules

- Implement on the experimental branch only. Preserve v1 and the parked board-support branch; do not merge or tag either as part of this plan.
- Reuse v1 drivers and state machines only after reviewing their source, dependencies, and tests. The parked branch is evidence/reference only; do not check it out, modify it, or merge it. Port only individually reviewed fixes with tests.
- No physical flash or other hardware operation without explicit permission from the maintainer and an available hardware operator. This roadmap is not such permission. Simulation, compilation, and code inspection never count as a physical gate pass.
- Assign one named flash owner and one serial-port operator at a time. Stop simulator USB bridging before physical serial operations; release injected inputs and the port afterward. Non-hardware CI, documentation, format design, and host tests can proceed in parallel.
- Keep a dated evidence record for each gate: board identity/revision, firmware and profile hashes, test procedure, raw observations, pass/fail/unknown result, operator, and follow-up. Reuse valid Board B findings below rather than repeating them without cause.
- Do not guess board identity, revision-dependent pins, or electrical behavior. Inspect the actual assembly and authoritative wiring/schematic before applying power or driving a signal.

## Current execution status

- **V2 development baseline (2026-11-13 UTC):** the integrated source at
  `88748e2d172554ddb844adc5fe07db665660038d` and this baseline note form a
  committed starting point for continued work, not a finished release or a
  guarantee that the current device matches it. Both Lua game ports are
  tracked in `games/`; their source checks and the host suite passed at this
  checkpoint. A separate temporary-ID Lua Brick Breaker bundle was installed
  on the 1.47B for side-by-side testing with the native game; that temporary
  ID is not the committed bundle's solution to the native ID conflict. A
  reported Rock Rain visual issue remains unresolved. Separate uncommitted
  display diagnostics are not included in this baseline. Preserve the
  outstanding feature and hardware gates below.
- **Active priority (2026-11-13 UTC):** the user accepted practical 1.47B Brick Breaker responsiveness, two-knob Pause/resume, the three-second sleep gesture, and idle sleep followed by press-to-wake. Stop further sleep/calibration work for now and advance the game-runtime ports. Brick Breaker and Rock Rain bundle ports, including their focused gameplay-review fixes, are integrated and pass the combined software checks below. These observations do not close all control-policy, resource, presentation or release gates, and 1.69 hardware remains deferred. The dated diagnostic observations below are retained as history; see the gameplay waypoint for the later physical evidence.
- **Historical pause for physical waypoints (2026-11-12):** feature expansion stopped until the next hardware checkpoint was reviewed. At that inventory point the uncommitted working tree had 45 modified tracked files, two deleted tracked files and 105 untracked files. It included the diagnostic, board profiles, OS/input changes, engine/Lua/bundle/store foundations, simulator and tools, fixtures and tests, plus this specification and roadmap. A session-local snapshot preserved the tracked patch and untracked files; this work has since been organized into incremental commits. The host suite at that point passed 54 Python tests, 11 Node tests and all discovered C++/game test binaries; both application and WASM profiles built and passed simulator integration checks. These were software results, not physical gate signoffs.
- **Board A (1.47B):** the user reports that the diagnostic screen appears after a manual reset and that knob input updates on screen; why a manual reset was needed is not established. They confirmed the palette, text-size samples, and readouts are visible without clipping; the profile label is at the top edge, content is about 1 mm from the left, and the bottom gap is about 2 mm. These observations inform the safe-viewport review but do not approve its final inset. Approximate clockwise readings were 71 transitions for the left knob and 72 for the right; the user noted the turns were not exact, so these do not establish fitted counts or polarity. The user also reports that five separate clicks on each knob each increased its click count by five and the button returned to UP. The earlier flashed diagnostic image did not answer `info`, so the prior `tg.py send info` received no acknowledgement; a prior 3-second monitor also captured no serial data. Current source now answers `info` with its compiled profile/dimensions and input/IMU health, but this response has not been observed on hardware and requires an authorized flash. On-screen observations do not establish the flashed image/profile hash or exact counts. Per-knob counts/polarity, IMU axes, serial telemetry, sleep/wake, and auto-off checks remain open. The revised diagnostic source performs standard power initialization and emits periodic telemetry only while the USB CDC host is active; both test-card profiles and both complete firmware profiles compile. The test screen has been user-observed after reset, but its exact flashed image/profile hash is unknown.
- **Board B (1.69):** continue host/build work using the existing profile and prior partial physical observations as provisional development assumptions, as directed by the user. A connected board has not been identified; do not perform board-specific operations or flash it without confirmed identity and separate permission. Do not mark H/V/C or any other physical gate passed, and expect corrections after the outstanding revision, GPIO3, input, gyro, sleep/wake, and auto-off checks.
- **Authorized 1.47B serial probe:** a read-only monitor and `info` query returned no serial lines from the single VID:303A port. It was opened with and without DTR; the DTR-open may have reset the board. No firmware flash or storage write occurred. USB VID/PID does not establish board identity, and the empty response has no established cause.
- **Creator-loop source and runtime:** the deterministic bundle packer, source checker, golden vectors, bounded Lua runtime with candidate bundle-local `require`, `BundleScript`, adaptive `knob_dodge` prototype, and preallocated LVGL presenter are built and host-tested. `BundleView` validates binary structure, checksums, paths, text and required files; `ManifestView` validates required root metadata and agreement between manifest and store IDs, but is not a full TOML parser. The WASM build packages every valid `games/<id>` source through the shared C++ adapter/Lua runtime; the current tree contains `knob_dodge`, `brickbreaker` and `rock_rain`, and edits to game folders rebuild the preview.
- **M5 bundle-port integration (software checkpoint complete):** initial ports landed as `4df2695` (Rock Rain) and `b725256` (Brick Breaker). The test-runner conflict was resolved by retaining both Lua regression files. Review fixes landed as `d0e75d1` (Brick Breaker: per-hit score, speed-sensitive paddle movement and capped launch speed) and `1bd8764` (Rock Rain: speed-sensitive steering and an untruncated second gap). Combined source check/pack, the host suite, both firmware builds and both WASM profile builds/integration scripts pass; native/WASM engine traces match. Firmware build results are reused for the review fixes, which change only Lua and host tests. `a49b6cb` updates the simulator's expected registry and exercises both ports through action, Pause/resume and OS restart on the 1.47B simulator. These are not final M5 acceptance results or physical bundle-play evidence. Native Brick Breaker remains registered on device under `brickbreaker`, so the same-ID bundle cannot currently replace it there; the simulator suppresses that native registration to exercise the Lua port. The small Brick Breaker port still omits power-ups, lasers, multiball and best-score persistence. Those omissions remain release work, not approved scope cuts. The earlier physical Brick Breaker signoff was for the native game, not either new Lua port.
- **Device bundle store:** both firmware profiles compile with the selected custom partition table (preserving both OTA app slots and using the existing 3.375 MiB data region for LittleFS). Rollback-aware staged writes, boot recovery, free-space reporting, the versioned `install 1` protocol (ordered 768-byte base64 chunks, per-chunk and whole-bundle CRC32, 30-second timeout), and `tg.py push <id> --board 147b|169` are implemented. Push requires a physically confirmed model selection and matching firmware profile/API response before transfer; the response cannot identify the physical assembly. Boot scans installed bundles into the registry. Host tests exercise commit, failed rename, rollback, and backup recovery against a fault-injected in-memory store. Rock Rain install/launch is now observed on 1.47B as recorded below; actual LittleFS power-loss behavior, measured capacity and 1.69 install/launch remain unverified.
- **Simulator bundle install:** arbitrary `.tgb` files can be uploaded, validated and persisted in browser localStorage, then registered/launched after restart. Integration tests cover persistence, launch, invalid bundles, and quota failure preserving an existing bundle. Browser quotas vary; this is not device-install evidence. A two-fixture generator test covers multi-game descriptors and stable ordering.
- **Rock Rain device checkpoint:** the user-authorized application-only flash of integrated `f5a95d3` and supported bundle install succeeded on the identified 1.47B. The new `rock_rain` bundle launches and draws its game screen on device; the exact image hash and observations are recorded in the bundle-device waypoint below. Human gameplay acceptance remains pending, native Brick Breaker still blocks its same-ID bundle, and 1.69 hardware remains deferred.
- **Remaining creator loop:** asset/level/starter semantics and authoring editors are incomplete; Lua limits remain candidates, not an accepted API/resource freeze. Lua 5.4.9 is pinned to upstream commit `312b9efaa1061c2c4cad08554dbc1351c3270eef`. The host suite includes 54 Python tests, 11 Node tests and C++/game binaries. Both application and test-card profiles for 1.47B and 1.69 have built, as have both WASM profiles; these build results are separate from physical evidence.
- **Lua allocation failure:** callback lookup and input-table construction now run inside Lua's protected call, and module-cache initialization is protected too. A host regression retains input tables until the Lua heap is exhausted and confirms a fault with cleared drawing output rather than a process abort. Both firmware profiles and a clean WASM build compile; this does not establish hardware watchdog behavior or complete Gate R.
- **Native Lua loop limits:** `string.rep`, `table.move`, `table.insert`, `table.remove` and `table.concat` have a candidate 4,096-iteration cap and charge the protected call's instruction budget, including empty-string, absent-element and table-shift cases. String byte operations, `tonumber`, `table.unpack`, `table.pack` and `string.char` also have candidate work bounds; unaudited binary string packing is unavailable. Native and WASM regressions exercise repeated byte copies and numeric conversions. Other native library paths and device watchdog behavior remain open for Gate R.
- **Lua source checking:** `tg.py check`, `describe`, `pack` and `push` now parse rules and bundle-local modules with the pinned Lua parser in text-only mode, without executing game code. Parsing has candidate source and heap limits; the parser is cached after its first host build. Asset semantics, runtime behavior and on-device install validation are not covered by this check. On a Darwin arm64 host with Python 3.12.4, ten warm `check knob_dodge` subprocess runs (2,584 source bytes) took 55.9 ms median and 59.5 ms maximum; this small prototype is not yet a representative release workload.
- **Game descriptions:** `tg.py describe <id> [--json]` summarizes validated text-source metadata, file counts and candidate declarations with bounded previews; JSON has a schema version. Controls and part semantics await the registry and are not inferred.
- **Bundle/Lua parity:** the host packer and shared native/WASM bundle validator now reject module filenames that the sandbox's `require` cannot address, and Lua sources larger than the runtime's 64 KiB candidate cap. Checksum-correct invalid module names and oversized scripts are rejected in the WASM install path without replacing an installed game.
- **Manifest compatibility:** the C++ manifest reader accepts literal strings, quoted root keys, valid integer spellings of API version 1, printable space-only names and TOML tabs in strings; it rejects embedded NULs in IDs and views. `BundleScript` also checks this metadata before executing Lua, including when used outside the installed-game store. Alternate valid TOML spellings install in WASM; complete TOML and semantic validation are still open.
- **Control arbitration:** a host-testable button state machine now passes individual holds to games, captures both-knob chords, opens Pause on release before three continuous seconds, and opens Sleep at the three-second threshold. Chord presses/releases are suppressed and gameplay updates freeze while captured. Host tests and both application, diagnostic, and WASM builds pass; the behavior is not physically verified, and Gate C remains open on the 1.47B while 1.69 is unavailable.
- **Engine timing:** the Lua host now uses a 60 Hz fixed-step runner and a candidate approximately 30 Hz presentation cadence; legacy games retain variable-step updates. Input is buffered by observation time, delivered once on a later tick, and discarded when an OS gesture captures the controls. Backlog, queue and ambiguous-edge limits fail explicitly rather than silently dropping events. Host tests and an actual WASM Lua-game test exercise catch-up, input delivery and Pause/resume; both application profiles build. CI compares native and WASM fixed-step traces and the Lua `knob_dodge` fixture's draw-command traces across two candidate viewport sizes, including movement, collision, reset and scoring. This is not a general Lua numeric or full-game determinism proof; other representative traces, sustained hardware timing and player acceptance remain open before API freeze.
- **Random streams:** the provisional Lua `tg.cosmetic_random` uses a seeded stream separate from `math.random`, so visual-only calls do not advance gameplay randomness. Host regressions and native/WASM traces cover both streams and reset behavior. The current combined update/draw callback cannot enforce use of the cosmetic stream for rendering; a separate rendering lifecycle and complete replay contract remain open.
- **Dual-profile simulator:** `tg.py sim --board 169` runs a separate real-firmware WASM profile at 280x240 with no simulated RGB LED; the default 1.47B remains 320x172. Shell layouts and Lua viewport adapt to screen dimensions, and profile-specific engine tests and browser screenshots cover boot and Lua gameplay. Legacy games still use 320x172 geometry and are not adapted to 1.69. Neither simulator profile establishes physical display or control acceptance.
- **Presentation and preview reliability:** displayed Lua text is retained in preallocated presenter storage until the next presentation, independently of intermediate simulation ticks. Palette index 0 is transparent and viewport clipping preserves partially visible text positioning; actual WASM pixel regressions cover both profiles. The browser pauses simulated time while hidden, preventing a hidden-tab delay from becoming a fixed-step overload; readiness and failed boot reporting no longer treat a non-running firmware as ready. This is simulator behavior, not evidence of physical timing or watchdog recovery.
- **Candidate engine lifecycle primitives:** `GenerationalPool<T, Capacity>` provides caller-selected fixed capacity, explicit full/stale results, slot reuse, and deterministic slot-order handle traversal. `DeferredQueue<T, Capacity>` provides bounded FIFO ordering, explicit overflow, and batch draining so callback-enqueued work waits for the next boundary. Both are host-tested; neither is integrated into gameplay yet, and neither freezes pool size, spawn policy, collision/event semantics, or overflow policy for decorative versus gameplay work.
- **Candidate collision primitive:** `sweepAabb` computes swept AABB time-of-impact and normals for high-speed, diagonal and axis-aligned motion, while reporting starting overlap/touch separately for caller policy. Host tests cover both directions, corner contact, misses and invalid geometry. It is not integrated into a game, and does not freeze overlap phases, tile/actor ordering, collision response or numeric/API policy.
- **Candidate camera primitive:** a bounded dead-zone camera follows a world-space target, clamps to world edges and keeps origin zero when the viewport exceeds the world. Host tests cover both axes, fractional movement, boundaries and invalid configuration; a two-viewport trace is compared between native and WASM. It is not integrated into a game and does not freeze viewport geometry or API.
- **CI:** `.github/workflows/ci.yml` now runs the host suite, clean-run WASM simulator build and engine/storage integration test, plus application and isolated test-card builds for both board profiles. It has not yet been observed on a GitHub-hosted runner.
- **TiltBlock 2D concept gate:** a hidden `turn right 1237` simulator-only mode draws the existing model as a front-on 8x8 field, including touched cells, holes, walls, bonuses, the player and alien. It preserves current game rules and controls; ordinary launches still use the existing renderer. The native firmware profiles, clean WASM build, engine checks, simulator movement, and Pause/resume passed. This is a playable presentation prototype for maintainer review, not approval of the final art/progression or physical performance; M5 remains open.
- **Physical gate status:** simulator/build evidence does not substitute for either board's physical acceptance. Limit each board's next hardware session to outstanding checks; retain one flash owner and serial operator at a time.
- **Host preflight:** `python3 tools/tg.py doctor` now reports Python, PlatformIO, the host C++ compiler, Node, Git, curl, pyserial, configured firmware profiles, and Espressif-VID USB ports without opening a port or inferring board identity. The current host sees one matching USB serial port; see the separately authorized probe below.

## Next physical waypoint: 1.47B diagnostic

**2026-11-12 attempt:** the user identified the connected board as the 1.47B,
authorized this diagnostic image, and explicitly waived the flash backup; no
usable backup was made. Automatic ROM entry failed, but a 1200-baud reset
re-enumerated the same USB device in download mode. The reported security
flags showed flash encryption and secure boot disabled, and flash size was
16 MiB. Esptool wrote and verified the four approved files at the offsets
below, but the manual command mistakenly forced QIO for the bootloader.
PlatformIO resolves the board's QIO configuration to **DIO for upload**;
the original bootloader image also specified DIO. The user reported a black
screen after RESET and after a USB power cycle, and the USB port remained the
ROM/JTAG interface.

With the user's separate permission, the same approved bootloader was rewritten
at `0x0000` using DIO and verified; the partition table, OTA data and
application were not rewritten. The board then enumerated as application USB
CDC, `info` reported `profile=147b screen=320x172 input=READY imu=OK`, and
periodic diagnostic telemetry appeared. The user confirmed the test card was
visible. This supports the incorrect boot mode as the cause of the black screen;
it does not establish a full display, input, IMU or other physical gate pass.
The earlier working test card was an older diagnostic image reported visible
after a manual reset; its exact binary hash and upload command were not recorded
in the checkpoint evidence. The standard test-card flash command uses PlatformIO.

**1.47B test-card observations, 2026-11-12:** the user confirmed upright,
unclipped text and swatches, including recognizable red, orange, yellow,
green, blue, pink and white. No photograph or final viewport approval was
provided. The USB `info` reply and live telemetry reported input ready and
IMU OK; this is device status, not proof of correct sensor axes.

- From zero, one user-marked clockwise revolution per knob reported L=+79,
  R=+79, with ERR=2 and ERR=5 respectively. After RESET, one
  counterclockwise revolution per knob reported L=-79, R=-79, with ERR=5
  and ERR=6. Five clicks on each knob produced C:5 and returned to UP.
  Both buttons reported DN and HLD during holds, then UP after release.
- On a further slow clockwise revolution, L changed -63 to +13 while ERR
  increased 5 to 7; R changed -87 to -10 while ERR increased 6 to 8. The
  increased invalid-transition counts require investigation; turn counts
  and input reliability are **not accepted** from these observations.
- Level tilt was reported near zero. Lowering the right edge made T X
  positive and lowering the left made it negative; lowering the top edge
  made T Y negative and lowering the bottom made it positive. The user
  observed opposite gyro responses for opposite motions and reported G X
  positive while lowering the right edge, G Y negative while lowering the
  top edge, and G Z negative while rotating clockwise with the screen up.
  These signs are user-read transient values; quantitative axis isolation
  and agreement with the intended coordinate convention remain open.

These are partial observations, not acceptance of the full display, input
or IMU gates. Sleep/wake, auto-off, control policy, device performance and
all outstanding 1.69 checks require their separate waypoints.

**Encoder investigation, 2026-11-12:** with both knobs untouched and the
serial host open, 151 samples over 15 seconds showed stable L=14/ERR=7 and
R=-10/ERR=8. In a separate serial-host-closed trial, the user reported starting
both totals and errors at zero and turning each knob five clockwise
revolutions: L=365/ERR=17 and R=365/ERR=18. With the serial host open, the
next trial ended at L=829/ERR=25 and R=750/ERR=26. The user was unsure whether
the left knob made five or six revolutions in that second trial; exclude its
count from a five-turn comparison. For the reported five turns on the right,
the open-host change was +385/ERR+8. The closed- and open-host trials both
show errors, but their different operator-controlled speeds and small sample
size do not quantify a serial-host effect. No image was reflashed.

`ERR` counts snapshots where both quadrature bits differ from the last
sample; it is not a count of lost steps or a proof of their direction. A
two-bit jump can result from missed intermediate edges, contact bounce,
electrical noise or nearly simultaneous transitions. Ordinary one-bit bounce
can cancel in the decoder. The diagnostic displays a cumulative copy of
the same ISR counter sent over serial; the repeated idle readings rule out
continuous stationary drift during that interval, not intermittent noise.
For 1.47B both signals are sampled together from the same GPIO register.
The v1 decoder and interrupt capture use the same quadrature logic; v2 adds
a high-bank register read for the other board. The available evidence does
not isolate a regression, establish the waveform, or justify inferring and
adding missing counts. Do not mask errors, apply the button debounce to knob
signals, or change the decoder until the cause is measured.

**Practical acceptance:** the user directs that this is a toy, not a
precision instrument. Assess the actual game and menu experience: correct
direction, responsive movement and no noticeable dropped or erratic input.
Exact transitions per turn and a zero diagnostic ERR count are not product
requirements. Investigate the counter discrepancy further only if it causes
a player-visible problem; do not silently infer or mask missing transitions.

**Counter waypoint:** a 1.47B-only test-card candidate
adds a `counter` command comparing unfiltered on-chip PCNT counts against
the existing GPIO-interrupt decoder. It reserves PCNT units 0 and 1, two
channels each, and routes only the existing encoder A/B pins as pulled-up
inputs. Its startup configuration is based on the pinned ESP-IDF 4.4.7
driver, which configures those pins as inputs and routes them into the GPIO
matrix; the existing GPIO interrupt decoder is unchanged. The diagnostic
latches a PCNT limit interrupt, reports failures explicitly, and invalidates
a hardware count once its +/-30000 limit is reached. PCNT counts are not an
independent electrical reference and the software/hardware reads are not
atomic; take baseline and final snapshots while both knobs are stationary.
Confirm direction, counts, errors and limit flags with controlled marked turns
before drawing conclusions. A new diagnostic flash requires separate
authorization; its result cannot establish an input gate pass on its own.
The 1.47B application image has SHA-256
`d6def8ac6ff7ecca62820f85098fa4dc0b9bfa6fe9ad793be94e399b401826a3`;
with explicit user approval it replaced only the existing diagnostic
application at `0x10000`, without rewriting the bootloader, partition table
or OTA data. Esptool verified the write; USB `info` and `counter` responded
and the user confirmed the card was visible.

With stationary snapshots before and after each user-marked five-turn trial,
the counter changes were:

| Knob and direction | PCNT | Interrupt decoder | New invalid |
|---|---:|---:|---:|
| Left clockwise | +399 | +389 | 5 |
| Right clockwise | +403 | +395 | 4 |
| Left counterclockwise | -401 | -389 | 6 |
| Right counterclockwise | -403 | -383 | 10 |

Hardware-limit and software-overrun flags remained zero. In these trials the
count difference was twice the invalid count, consistent with the decoder
skipping two-bit transitions that the pulse counter observed. This is
comparative evidence, not proof the pulse counter saw clean contacts or an
exact 400 transitions per five turns. It does not justify filling in missing
directions or pass the input gate; compare repeated speeds and investigate
the capture path and electrical signal before selecting a correction.

**Normal-speed and reversal comparison, 2026-11-12:** with the same diagnostic
image and stationary snapshots, the user performed marked turns:

| Knob and direction | Turns | PCNT change | Interrupt change | New invalid |
|---|---:|---:|---:|---:|
| Left clockwise | 5 | +400 | +376 | 12 |
| Left counterclockwise | 5 | -400 | -362 | 19 |
| Right clockwise | 5 | +400 | +386 | 7 |
| Right counterclockwise | 6 | -481 | -435 | 23 |

For ten cycles of a clockwise half-turn followed by a counterclockwise
half-turn back to a mark, PCNT's net change was zero for each knob; the
interrupt decoder drifted -12 on the left and +4 on the right, with 34 and
26 new invalid transitions respectively. Hardware-limit and software-overrun
flags remained zero. These trials strengthen the case for evaluating PCNT
as a candidate input source, including reversal behavior, but neither proves
its contact waveform is noise-free nor exercises counter-limit rollover.
The six-turn right trial is recorded as six, not compared to a five-turn target.

The user also made about 20 small reversals at each knob. From the prior
stationary snapshots to the next snapshots, the left PCNT and interrupt
changes were both -2 with no new left invalid events. The right changes were
-2 PCNT and -6 interrupt, with 12 new right invalid events. These are net
changes, not a count of the individual reversals; the exact return phase and
intermediate paths were not independently measured.

**Deferred counter-limit experiment:** if gameplay reveals a problem and
PCNT is considered as a replacement, develop a
test-card-only extended counter using the two remaining PCNT units and the
same existing inputs, leaving units 0/1 as the reference. Exercise positive
and negative limit events with a deliberately small limit, repeated
crossings, reversals near each boundary, simultaneous knobs, reset and
initialization failure. Compare stationary deltas against units 0/1 and flag
any ambiguous, missed, coalesced or overflowed event rather than fabricating a
count. Host tests must cover the accumulator and race cases; review interrupt
ownership, GPIO routing and all four PCNT units' allocation before building.
Require explicit authorization before flashing the new test image. Passing
this experiment would establish a candidate rollover mechanism, not validate
contact noise, full 1.47B gameplay, or the deferred 1.69.

**1.47B gameplay waypoint:** the existing v2 application firmware
includes the shell, v1-style knob decoder, and built-in Brick Breaker. It is
not the complete v2 release or an installed Lua game. The prepared application
SHA-256 is `1b0bdd1ecaef4fec02b9a6795a225d7b63c52353cf9a28341a0aae80cbe433f7`.
With separate user approval, flash only this application at `0x10000`; leave
the current bootloader, partition table and OTA selection unchanged. This
replaces the test card. There is no verified flash backup. On first startup
the application can format the LittleFS partition only if it is entirely blank,
create its game directory and recover or remove staged install files; settings
and best scores can also be written during use. Existing data may be affected.

For a short hands-on check: use a knob and click to open Brick Breaker; turn
the main knob slowly and quickly in both directions and judge whether the
paddle follows naturally; click to launch and play briefly. Press both knobs
together and release before three seconds to pause, then resume. A successful
experience is useful 1.47B gameplay evidence, not proof of the full
sleep/wake sequence, auto-off, Lua installation, all v2 controls or the
deferred 1.69.

**Observed 2026-11-13 UTC:** with explicit approval, the prepared application
was written and hash-verified at `0x10000` on the confirmed 1.47B; bootloader
and partition table were not rewritten. The automatic 1200-baud ROM-entry
attempt reported a serial configuration error, but the same device appeared
in ROM mode and the upload completed. The application returned on USB and
reported Brick Breaker available. The user opened it, tried turning both ways
and launching, and reported that it plays well; they also confirmed that the
two-knob pause and resume worked. They subsequently reported: "3s press to
sleep works." This clears the practical 1.47B gameplay checkpoint, not the
full sleep/wake and control-policy gates, other remaining physical gates or
1.69 testing.

**1.47B idle sleep/wake check complete (user report):** the user subsequently
confirmed that the board slept after three minutes and woke on a press. This
records the observed interval, not a new timeout requirement or a measurement
of deep-sleep current. Together with the reported three-second gesture, it
closes this practical 1.47B sleep/wake waypoint only. Release/fresh-click, No,
USB-host versus power-only behavior and the deferred 1.69 are not established
by this report; further sleep testing is deferred at the user's direction.
Do not add a separate power-detection or charging-indicator feature: the user
will use the board's existing charging LED. Prioritize the game-runtime ports
next; broader v2 release gates remain open.

**1.47B bundle-device waypoint (2026-11-13 UTC):** on the user's request to
flash for testing, the clean integrated revision
`f5a95d3b4c2cb23ee2e80bc0d0ed831d5064671b` was built for the same identified
1.47B. The existing game store was healthy and reported API 1/profile 147b.
The supported `tg.py push rock_rain --board 147b` flow installed the reviewed
17,109-byte bundle before reboot. The verified application-only esptool path
then wrote `0x10000` through `0xe0fff`, with DIO and hash verification; it did
not rewrite the bootloader, partition table, OTA selection or game-store
partition. No full-flash erase or filesystem format was performed. The
application SHA-256 is
`d1bdfd4c53d109dff4edbf5a5dc3df0a1280cad99b6f5b8d76c7b1f94e30f725`.
USB reconnected, storage remained ready, and `info` listed `brickbreaker`,
`rockrain`, `rock_rain` and `tiltblock`. Launching `rock_rain` succeeded; a
device framebuffer capture showed its score/lives/ammo HUD and game-over
screen. This establishes bundle install and execution on 1.47B, not human
acceptance of physical controls, screen readability or sustained performance.
The new Rock Rain bundle was left open for the user to test. Native
`brickbreaker` still owns its ID; the Brick Breaker Lua bundle was not
installed. No 1.69 hardware operation occurred.

The smallest useful hardware image is the isolated `test-card-147b`, not the
full application. On 2026-11-12 it built successfully from the current worktree:
`.pio/build/test-card-147b/firmware.bin`, SHA-256
`5e897051ee5bed189b40b09bce13b60a4b26820397753fe812cec4541e5f4401`.
The session preserves copies of this image and its matching bootloader,
partition table and OTA boot data, plus the source snapshot. A later rebuild
may produce a different image; recheck all hashes and obtain approval for the
actual artifacts. The card displays 16 palette swatches, four text sizes,
per-knob totals, clicks and errors, and tilt and gyro readings. It reports
`info` and periodic telemetry over an open serial host. It does **not** run
games, Pause, sleep, wake or auto-off tests.

**Before any board operation:** the maintainer must physically identify the
connected assembly as the 1.47B, agree on the backup/recovery approach and
explicitly authorize this exact diagnostic image and its accompanying upload
files. A profile string or USB port cannot identify the assembly. The 1.69
remains deferred, and requires separate identification and authorization.
One operator owns the port; disconnect the simulator bridge first.

The PlatformIO upload writes bootloader at `0x0000`, partition table at
`0x8000`, OTA boot data at `0xe000` and the test-card application at `0x10000`.
It replaces the running application and OTA selection data; the custom table
also changes how flash storage is mapped relative to v1. Existing games,
settings or scores could become inaccessible or be damaged, and a failed
upload could leave the device unbootable. The test card does not mount or
format LittleFS, but that is not a guarantee about existing data.

Before flashing, if preserving existing contents matters, obtain separate
permission to make and verify a **private full 16 MiB flash backup**, including
the current partition table and OTA data. Check security/readback restrictions:
an encrypted or protected flash read may not produce a usable restore. Do not
assume a backup exists or that rebuilding v1 recovers saved data. If a valid
backup cannot be obtained, stop for the maintainer's explicit data-loss
decision. Recovery requires an authorized restore of the matching full backup,
or rebuilding and flashing v1 (`83620e31e61abe87269f9e69d3f932745b6bf063`)
with its original partition layout; the latter does not restore lost data.
ROM download recovery using BOOT/RESET may be needed after a failed flash.

After identification, backup decision and authorization:

1. Flash only the approved four-image set and record hashes, operator, board
   model/revision, port and date. Capture upload output; if startup requires
   RESET again, record it rather than treating automatic boot as verified.
2. Record screen photo: `PROFILE 147b 320x172`, orientation, palette and text
   visibility/insets. The label confirms the compiled profile only.
3. Use `info` and a short serial capture to check input/IMU health and telemetry.
   Record failures as failures; earlier `info` and monitor attempts had no reply.
4. For each knob, measure signed total changes for several accurately marked
   full clockwise and counterclockwise turns, click five times, and check UP,
   hold, and error readings. Record counts and direction, not approximate
   turns as an 80-transition pass.
5. Slowly tilt each way and rotate around each axis; record display/telemetry
   signs and level readings. Stop on missing IMU or unexpected electrical
   behavior. Review palette and insets with the maintainer.

This waypoint passes only its recorded display, input, serial and IMU checks
when observations match the agreed orientation and stable measured behavior.
It cannot pass H or V for both boards, nor C, R, sleep/wake, auto-off, or
performance gates. No such result exists yet.

## Subsequent waypoints

1. **1.47B application and controls:** separately authorize the application
   image and any installed-game writes. Check startup, standalone holds,
   Pause/resume/restart, three-second sleep priority, release/fresh click, No,
   wake from either knob, and host-open versus USB-power-only auto-off.
   Record physical behavior and failures before accepting the control policy.
2. **Editable game on 1.47B:** after application startup and storage safety,
   separately authorize installation; push `knob_dodge`, restart, play, edit and
   reinstall without firmware rebuild. Verify interrupted/invalid replacement
   preserves the previous working game. Full TOML/assets/levels validation,
   watchdog and independent host timeout remain open; this is a creator-loop
   experiment, not Gate R.
3. **1.69 physical regression:** when available and identified, with separate
   flash permission and revision/strap safety review, run only outstanding
   checks: exact knob counts/clicks, gyro, GPIO3 strap/wake, sleep/wake,
   auto-off and display/HUD review. Reuse earlier documented display, direction
   and tilt observations where still applicable.
4. **Engine and content:** only after relevant physical decisions, integrate
   and test the registered kit and six playable starters, then Brick Breaker,
   Rock Rain and the reviewed 2D TiltBlock. Each needs actual play, control,
   frame-time and resource evidence on both profiles; no API freeze until C,
   R, display and performance requirements pass or an approved revision exists.
5. **Creator tools and release:** complete format/reference validation,
   bounded replay/verification, worker isolation and recovery, editors, MCP,
   guidance, benchmark and release checks. Require end-to-end creator tests,
   adversarial recovery, both physical boards and the complete specified
   release scope before declaring v2 ready.

## Dependency map

```text
M0 provenance + minimal CI + recorded decisions
  └─ M1 dual-board diagnostic firmware + density test card
       ├─ H: board identity, electrical safety, input/sensor/power checks
       └─ V: screen density, palette, HUD and viewport review on both boards
            └─ M2 format/bundle smoke test + smallest editable end-to-end game
                 └─ M3 control/chord policy + presentation/resource budget
                      ├─ C: real-board control and OS gesture acceptance
                      ├─ R: bounded Lua/resource/recovery proof on WASM + hardware
                      └─ M4 engine/registry + six playable starters
                           └─ M5 Brick Breaker, Rock Rain, 2D TiltBlock
                                └─ M6 CLI, editors, MCP, guidance, benchmark
                                     └─ M7 release acceptance
                                          └─ M8 distribution
```

Dependencies are gates, not a requirement to serialize all work. Work that does not assume a gate's unresolved behavior may proceed in parallel, provided it cannot silently freeze the blocked contract.

## Milestones

| Milestone | Work and dependencies | Evidence / pass criteria | Owner and stop condition |
|---|---|---|---|
| **M0 — Provenance and safe working baseline** | Confirm experimental branch and exact v1 SHA (`83620e31e61abe87269f9e69d3f932745b6bf063`); inspect current build/test status; preserve notices, fixtures, and history; add minimal CI and record confirmed decisions. Pin candidate tool versions before adopting them. This is prerequisite to code replacement, not a reason to delay independent source review. | CI builds/tests the unchanged baseline; v1 SHA and retained evidence are recorded; no branch merge, tag, or unreviewed toolchain migration. | Maintainer + implementation owner. Stop replacement work if provenance or baseline integrity cannot be established. Tag only later with explicit authorization. |
| **M1 — Dual-board diagnostic and physical gates H/V** | First code deliverable is a very small test-only diagnostic firmware path using inspected, verified v1 drivers and explicit `147b`/`169` profiles. It must not depend on Lua, game registry, editor, or install tooling. Compile both profiles before requesting a flash window. Add one configurable test card for native landscape geometry, color swatches, text sizes, safe insets, compact HUD and candidate 8x8-at-2x art. Review it on both actual panels before selecting density or viewport rules. | On each identified assembly: native USB serial/ROM upload flow; correct selected software profile (not treated as proof of physical identity); display extent/orientation/colors/backlight; left/right rotation polarity, measured transition count per revolution, click/debounce; IMU level/tilt axes and gyro axes; button release handling; sleep confirmation/cancel/wake; USB-host versus power-only auto-off. Capture test-card photos/screenshots and measurement log. Pass H only after identity/electrical review and all required physical checks; pass V only after maintainer/user review and published safe rectangles/minimum viewport. | One authorized flash owner/operator, with maintainer for decisions. Only one flash/serial operator at a time. Stop before any flash without explicit permission; stop any revision-dependent pin work until physical revision and safe circuit are confirmed. A failed check blocks only dependent features, not independent documentation/host work. |
| **M2 — Narrow creator-loop and toolchain split** | After M0, format and host-tool work may start in parallel with M1. Build the smallest complete path: text game -> validated manifest -> deterministic packed-bundle smoke test -> bounded Lua startup/tick -> shared native/WASM rendering -> validated staged install/restart on device. Keep bundle packing/unpacking testable on host and create valid/invalid golden fixtures. Build one tiny editable game rather than the kit or all starters. Keep the M1 diagnostic binary/path independent. | Reproducible pack/unpack round trip and rejection of malformed/truncated bundle; no firmware rebuild to edit the sample; WASM preview and board play use same engine/game source; invalid Lua and interrupted/invalid install recover to a usable launcher while preserving the previous game; warm reload and transfer measurements recorded. | Engine/format owner + toolchain owner; hardware install test requires the same authorized flash/serial window policy. Stop broad engine investment if sandbox, transfer/recovery, or no-firmware-rebuild promise is infeasible; return concrete evidence for a decision. |
| **M3 — Controls, OS arbitration, presentation and resource contracts** | Immediately after the smallest loop runs, implement and playtest the proposed dual-button chord on real devices, before freezing game input APIs. A solo button remains game-owned; dual press captures/freeze; short chord opens Pause; continuous three-second chord opens Sleep; require full release and a fresh action to confirm; No restores prior state. Test the already-delivered-action tradeoff. In parallel, benchmark actual display ownership, frame completion, input latency, memory placement, Lua caps and recovery. V determines layout bounds; H determines available peripherals. | Gate C: both boards demonstrate solo press/hold, Pause timing, sleep priority, release suppression, fresh confirmation, No resume, wake and ordinary game actions with no accidental trigger. Gate R: bounded adversarial Lua, allocation, events, callbacks, draw commands and host timeouts recover safely on WASM and hardware. Measure full presentation completion (not enqueue time), render/logic/Lua cost, tail latency, memory and sustained workload; publish resource-cap table and overload policy. Gate targets in the spec pass or an explicit maintainer-approved revision is recorded. | Input/OS owner and renderer/runtime owner; maintainer accepts gesture and any budget revision. Stop API freeze and dependent content if C or R fails. Continue independent CI, parser fixtures, docs, and non-conflicting host work. |
| **M4 — Shared engine, registry, six starters** | Requires M2 creator-loop feasibility, M3 contract freeze, and H/V/C/R evidence for any hardware-specific behavior. Implement portable C++ engine and bounded Lua host; versioned formats/protocol/replay; registry-generated schemas; deterministic actors/events/collision; profile-aware rendering, save and install contracts. Add all six complete playable starters: platformer, top-down adventure, shooter, driving, paddle, grid puzzle. | Native/WASM trace conformance; golden render fixtures at 320x172 and 280x240; valid/invalid format and resource-bound tests; each starter scaffolds and plays on both profiles; firmware builds for both profiles; tested typed controls and `.steps` scenarios. | Engine/kit owners. Stop API expansion when a required mechanic lacks tested semantics or either board regresses; do not replace physical evidence with simulator results. |
| **M5 — Three required games** | Depends on M4 mechanics and accepted controls/layout. Port Brick Breaker and Rock Rain while retaining recognizable rules/controls and document/play-test presentation or unit changes. Prototype a small playable front-on 2D TiltBlock treatment first; obtain human acceptance of progression, feedback, and knob/tilt controls before full redesign. Preserve its safe-tile/hazard/layer progression core, not the legacy projection. | All three pass deterministic automated scenarios and human playability on both boards; collision, viewport/spawn fairness, control feel and performance pass; TiltBlock concept approval is recorded before full conversion. | Game owners + maintainer/player reviewers. Stop a title on missing kit behavior, performance/safety failure, or unaccepted TiltBlock treatment; file a decision rather than hiding a scope reduction. |
| **M6 — Creator tools and evaluation** | Can proceed incrementally alongside M4/M5 once schemas/protocols stabilize. Complete `tg` CLI, editors, typed MCP operations, generated guidance/skills, simulator/headless host, compact errors, and conflict-safe writes. Start a small benchmark set during M2; keep tuning tasks separate from held-out evaluation. | End-to-end edit/preview/install without firmware rebuild; bounded and authorized file/device operations; simulator input release on switch/reload; accurate native dimensions; all 200+ benchmark tasks have declared scope and deterministic graders before improvement claims. | CLI/editor/tooling owners. Stop endpoint/hardware functions on path, origin/auth, permission, or serialization failures. |
| **M7 — Full release acceptance** | Requires every prior mandatory deliverable, including all physical gates for each supported board. | Full automated matrix, both-board firmware builds and physical acceptance, upgrade/data-preservation tests, sandbox/recovery, supported host/browser matrix, 200-task benchmark report, no concealed blocker. Evidence distinguishes tested, failed, and unsupported behavior. | Maintainer/release owner. No release claim while a mandatory gate is failed/unknown. |
| **M8 — Distribution** | After M7 and separate release authorization. | Tagged artifacts/manifests/partitions, compatible bundled games, hosted simulator, browser flasher/install instructions and clean-machine upgrade verification on supported combinations. | Maintainer/release owner. No tagging, publishing, or deployment is authorized by this roadmap. |

## M1 physical test sequence and known evidence

Run the diagnostic sequence on **both** boards, recording each result independently. Confirm model/revision from the assembled hardware and documentation before connecting or selecting a profile. No physical test is performed as part of this roadmap document.

1. **Identity and safe setup:** confirm Board A is ESP32-S3-LCD-1.47B and Board B is the existing non-Touch ESP32-S3-LCD-1.69; inspect Board B hardware revision before any revision-dependent power/buzzer wiring. Confirm a known-safe power/USB setup and correct explicit profile.
2. **Build/flash/serial smoke:** compile both profiles; with permission and a single operator, exercise native USB serial discovery, 1200-baud ROM entry, automatic upload, re-enumeration, boot log, and manual recovery path. Never infer physical board from USB identity.
3. **Display/test card:** inspect full bounds, orientation, swatches/color order, backlight, font legibility, corner-safe text, compact HUD, candidate art density and gameplay area; get human review on both real screens before freezing viewport or HUD.
4. **Inputs:** measure clockwise/counterclockwise totals per full revolution and click behavior for both knobs; verify polarity, missed/invalid transitions, debounce, held/press/release and safe release suppression. Use measured counts, not nominal 80 transitions/revolution, as the fitted hardware result.
5. **Motion:** verify level reference, tilt axes/sign/scale and gyro axes/sign with documented orientation and repeatable observations. Don't infer gyro support from tilt pass.
6. **Power/gestures:** verify short two-button Pause, long two-button sleep priority, release-before-confirmation, No restoring prior screen/game, wake from either button, and no stale wake press selecting an item. Safely validate GPIO3 strap/wake behavior on Board B before enabling that wake path.
7. **Auto-off and current:** compare an open USB CDC host with USB power only; test fresh countdown after host close. Measure whole-board current only if making power claims. Treat battery startup/latching as unsupported until measured.
8. **Record and review:** retain raw values, photos/log excerpts and firmware/profile hashes; mark each item pass/fail/unknown; file a decision for deviations. Do not repeat already accepted measurements without changed hardware/software or an evidence gap.

**Previously confirmed for Board B (partial evidence only):** 280x240 landscape orientation and colors/backlight; clockwise-positive encoder direction; downhill tilt and level centering. Reuse this evidence with its provenance. Exact full-revolution counts, click feel, gyro signs, GPIO3 strap/wake behavior, sleep/wake, USB-host versus power-only auto-off, battery behavior and hardware revision remain outstanding.

**Outstanding regression checks:** Board A 1.47B physical behavior; Board B sleep/wake and strap safety; exact knob counts/clicks; gyro axes; auto-off host/power distinction; any revision-dependent power/buzzer pins. Board B has no RGB LED capability; never drive GPIO38 as one. No touch hardware or features are in scope.

## Dependency and parallel-work policy

- **May proceed before H/V:** source/provenance audit; baseline CI; build scripts and native host tests; format/TOML decisions and fixtures; test-card code; toolchain pin research. Do not freeze any hardware-derived value.
- **Must wait for H:** board-profile pin/power/wake acceptance, GPIO3 wake enablement, revision-dependent connections, device claims, and hardware-specific input/power API decisions.
- **Must wait for V:** fixed viewport, text/sprite density, HUD placement, screen-space budgets, and geometry-sensitive game tuning.
- **Must wait for C:** frozen button/hold contract, game controls that rely on dual-button arbitration, and pause/resume action boundaries.
- **Must wait for R:** API freeze, broad kit investment, untrusted game execution claims, and release-like game installation.
- **Must wait for human TiltBlock review:** full 2D redesign beyond a disposable concept prototype.
- **Hardware scheduling:** one flash owner, one serial client and one board operation at a time. Keep hardware reservations short and scoped to a written test card; batch both-board tests to avoid setup churn. Non-hardware work may continue while waiting, but cannot claim a gate.

## Decision records and deliverables

For each gate, the owner should produce: test plan/card version; exact source and firmware hashes; board model/revision and profile; setup and safety checks; raw data/screenshots/logs; acceptance table with pass/fail/unknown; observed deviations and defect links; named reviewer and date; resulting decision record under `docs/decisions/`. Keep diagnostic firmware in a small dedicated path and do not entangle it with Lua/editor features.

The [Lua runtime decision record](decisions/lua-runtime.md) records the current source pin and sandbox prototype without freezing its numeric model or limits. The roadmap does not decide other unresolved technical candidates (density, binary layout, exact resource limits, DMA strategy, or gesture tuning). It schedules the evidence that resolves them and preserves the explicit uncertainties in the normative specification.
