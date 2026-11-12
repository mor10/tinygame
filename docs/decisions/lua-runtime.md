# Lua runtime foundation

**Status:** implementation spike, not Gate R evidence or an API/resource freeze.

## Source pin

Builds fetch the Lua development mirror's `v5.4.9` tag and require the exact
commit `312b9efaa1061c2c4cad08554dbc1351c3270eef`. The source is cached outside
the checkout under `~/.cache/tinygame-sim/lua-5.4.9`; build tools reject a cache
whose commit does not match. TinyGame does not vendor or modify the source.
Preserve the [Lua license notice](../third-party/lua-5.4.9.md) with any
distribution containing the compiled runtime.

The numeric configuration is still unresolved. Current builds use Lua's
default 64-bit integer and floating-point types on native, ESP32, and WASM
targets. The specification's 32-bit numeric build remains a candidate; cross-
target deterministic traces and representative gameplay measurements must
decide whether to adopt it.

## Current host limits and exposed APIs

The C++ wrapper currently enforces:

| Resource | Current candidate |
|---|---:|
| Lua heap per runtime | 1 MiB |
| Source per load | 64 KiB |
| Lua VM instructions per protected call | 100,000 |
| Items per bounded native loop | 4,096 |
| Output bytes per `string.rep` or `table.concat` call | 65,536 |

It loads text only. The exposed libraries are base, math, string, and table;
filesystem, OS, debug, package loading, bytecode, and game-controlled protected
calls are unavailable. Pattern-based string functions, `string.format`,
`string.dump`, binary `string.pack`/`unpack`/`packsize`, and `table.sort` are
removed pending audit. `math.random` uses a per-runtime
xorshift32 stream and `math.randomseed` is unavailable. These restrictions and
limits are a first host-testable boundary, not a proven sandbox or accepted
content API.

The candidate `tg.cosmetic_random` takes the same arguments as `math.random`
but uses a separate xorshift32 stream. Both streams reset when the runtime
reloads. The cosmetic seed is the gameplay seed XOR `0x9e3779b9`; the generator
normalizes a zero state to 1. Cosmetic calls cannot advance gameplay randomness.
This does not yet enforce a separate render callback: game code must use the
cosmetic stream for visual-only variation, and the API remains provisional.

`string.rep`, `table.move`, `table.insert`, `table.remove` and `table.concat`
charge loop iterations against the same per-call budget. Their per-call item
limit also bounds a single native loop, including repetitions of an empty
string, moves of absent table entries, and table shifts. Other native library
functions and copied-byte costs have not yet been comprehensively bounded.
`string.rep` and `table.concat` also charge output bytes, including separators,
against the same budget and have a per-call byte cap; repeated copies cannot
bypass the budget just by using a small repetition count. The concat check
scans the bounded table range before invoking Lua's library function. String
`byte`, `lower`, `reverse`, `sub` and `upper` conservatively charge the entire
input length, so even a short slice of a large string can exceed the candidate
limit. `string.char` and `table.pack` charge their argument counts;
`table.unpack` caps and charges its result count. This does not close Gate R.
`tonumber` also charges both string arguments before parsing them, so repeated
numeric conversions or a large string base cannot escape that budget.
The host syntax checker applies one 20-second deadline to all Lua files in a
bundle, not a separate 20-second allowance for each module.

The candidate `update(dt, input)` callback receives main/left/right encoder
edges and holds, action-button state, tilt, acceleration, and gyro values.
`tg.rect` and `tg.text` append bounded world-space commands to a fixed frame
buffer using authored palette indices. A command overflow or invalid command
faults the runtime and discards partial frame output. These bindings are
prototypes, not frozen public signatures.
The presenter keeps preallocated copies of displayed text so later simulation
ticks cannot mutate LVGL labels before the next presentation.
Palette index 0 is transparent for both rectangle and text commands.
The presenter clips text and rectangles at the selected viewport without
moving partially visible text to the viewport edge.

## Evidence and remaining work

`engine/test/test_lua_runtime.cpp` exercises library removal, callback
instruction exhaustion, heap exhaustion, oversized source rejection, and
unsupported random ranges, plus bounded draw output and fault handling.
`engine/test/test_bundle_script.cpp` verifies TGB structure -> Lua startup ->
input -> update and render-command output on a small internal fixture.
`engine/test/test_lua_dodge.cpp` drives the editable `games/knob_dodge` bundle
through steering, collision, and click-to-reset scenarios.
The WASM integration test installs adversarial bundles using guarded
native functions, confirms the game faults once, and exits to the launcher.
`python3 tools/tg.py test`, the standard 1.47B firmware build, and the WASM
build have passed with the pinned sources. The firmware build only compiles
this runtime; it was not flashed, and no physical runtime behavior is verified.

The bundle loader and installed-game adapter are connected to the registry,
presenter and staged store, but manifest validation remains partial and the
device transfer/rollback path is not yet physically verified. Native/WASM traces match for a fixed-step `knob_dodge` fixture on two candidate
viewport sizes, including movement, collision, reset and scoring. This does
not establish general Lua/game numeric determinism, browser worker timeouts,
C-library worst-case cost, device recovery, or performance on either board.
Gate R stays open; the device's heap, asset, pool, DMA, LVGL, and stack budgets
must still be measured and coordinated.
