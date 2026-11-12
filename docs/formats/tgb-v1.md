# TGB v1 bundle format (candidate)

**Status: implementation candidate, not an accepted API freeze.** The text
packer and its golden tests define the current candidate. Limits, load-time
behavior, hardware transfer size, and the final Lua/numeric policy still need
the evidence and decisions listed in the [rebuild specification](../tinygame-v2-rebuild-spec.md#16-remaining-gates-and-known-uncertainty).
Do not treat this document as proof of a playable Lua runtime or safe device
installation.

All integers are unsigned little-endian. Bundles are uncompressed. The header
is 40 bytes; each table entry starts with a 16-byte fixed record. Offsets are
absolute from the first bundle byte. CRC32 uses the standard reflected
polynomial (`zlib.crc32`) over the indicated bytes; it detects accidental
corruption and is not an authenticity or security mechanism.

## Header

| Offset | Size | Field | Required value |
|---:|---:|---|---|
| 0 | 4 | Magic | `TGB\0` |
| 4 | 2 | Format version | `1` |
| 6 | 2 | Header size | `40` |
| 8 | 2 | File count | `1..256` |
| 10 | 2 | Flags | `0` |
| 12 | 4 | Table offset | `40` |
| 16 | 4 | Table length | Exact encoded entry-table size |
| 20 | 4 | Payload offset | Table end rounded up to a 4-byte boundary |
| 24 | 4 | Payload length | `total length - payload offset` |
| 28 | 4 | Total length | Exact bundle byte length |
| 32 | 4 | Table CRC32 | CRC32 of the entire table |
| 36 | 4 | Reserved | Four zero bytes |

Any gap between the table and payload is zero-filled. No trailing bytes are
permitted.

## File table and payload

Entries are sorted by ASCII path bytes and have no duplicates. The fixed entry
record is `<u16 path length, u16 flags, u32 data offset, u32 data length,
u32 data CRC32>`. Flags are zero. The path follows the fixed record and is
padded with zero bytes to a 4-byte boundary. Paths are ASCII, `/` separated,
relative, and contain no empty, `.` or `..` component, backslash, or absolute
prefix.

Payload files are laid out in the same path order. Each data offset is
4-byte-aligned; any gap before a file is zero-filled. Ranges must be wholly
inside the payload, non-overlapping, and in ascending order. The CRC is over
the exact unpadded file bytes. The last file ends exactly at total length.

Every file is non-empty UTF-8 text with LF line endings and no NUL. Lua files
are source only; bytecode is rejected. Accepted paths are `game.toml`,
`sprites.txt`, `tiles.txt`, `levels.txt`, `README.md`, optional `rules.lua`,
`modules/*.lua`, and `test/*.steps`. The first five are required. No other
files or nested paths are accepted.

The manifest is TOML and must have `api = 1`, a matching `id` in
`[a-z][a-z0-9_]{0,14}`, a 1-13 character printable-ASCII `name`, a non-empty
`starter`, and `view = "scroll"` or `"fixed"`. Values in `[tuning]` must be
finite numbers. Detailed asset/level grammar, kind schemas, reference
resolution, and executable Lua semantics are intentionally not inferred here;
they require their own versioned contracts before runtime acceptance.
The current C++ manifest reader recognizes basic and literal strings, quoted
root keys and valid integer spellings of API version 1; it is not yet a full
TOML parser, so broader host/device manifest parity remains open.

## Bundle-local modules (candidate)

When a bundle is loaded through `BundleScript`, `require("palette")` resolves
only to `modules/palette.lua` in that same bundle. Module names are simple
ASCII identifiers (a letter or underscore, then letters, digits, or
underscores, at most 116 bytes); dots, slashes, traversal, and host modules
are rejected. Bundle validators also reject module filenames that cannot be
addressed by this loader. Loaded
modules are cached for that Lua state, and their source is subject to the same
64 KiB source and instruction/heap limits as the main script. Standalone
`LuaRuntime` instances do not expose `require`. This mapping is a tested
candidate contract, not an accepted API freeze.

## Provisional validation limits

These are conservative prototype ceilings, **not approved device budgets**:

| Resource | Candidate maximum |
|---|---:|
| Files per bundle | 256 |
| Path length | 128 bytes |
| One file | 1 MiB |
| Lua source file | 64 KiB |
| `game.toml` | 32 KiB |
| Entire bundle | 4 MiB |

The Python packer/parser reject malformed headers, unsupported versions,
invalid paths/encodings, bad checksums, truncation, overlaps, out-of-range data,
excess files/sizes, non-zero padding, invalid TOML, and non-finite tuning
values. The C++ `tg::engine::BundleView` reader is zero-copy and host-tested
against the same vectors; it checks binary structure, paths, checksums, UTF-8
text and required filenames. A separate `ManifestView` checks the required
root fields (`api`, `id`, `name`, `starter`, `view`) and the store requires the
manifest ID to match the installed filename. This is a bounded metadata
validator, not a complete TOML parser; the Python `tomllib` validation remains
the full source check. Asset/level grammar, kind schemas, reference resolution,
and executable Lua semantics still need versioned contracts before runtime
acceptance. Neither these prototype limits nor their storage/memory costs are
accepted for either board.

## Golden vectors

`tests/fixtures/tgb_v1_valid.hex` is a valid bundle for the text-only
`tests/fixtures/games/cave_hop` packaging fixture.
`tests/fixtures/tgb_v1_invalid.hex` is its deliberately corrupted-header
counterpart. Native host tests run the C++ reader against both vectors and a
damaged payload. `tests/fixtures/tgb_v1_lua_tick.hex` is an internal smoke
fixture for structural bundle loading and bounded Lua update/input/draw output;
it is not a playable game. `tests/fixtures/tgb_v1_lua_modules.hex` verifies
bundle-local `require` resolution and caching. `games/knob_dodge` is a
text-game prototype exercised through `BundleScript` for steering, collision,
and restart. Firmware scans validated LittleFS bundles into the launcher at
boot and creates a `BundleGame` from stored bytes. The WASM build now
deterministically packages and registers every valid `games/<id>` source
folder through the same C++ adapter and Lua runtime; the current tree contains
only `knob_dodge`, and edits to a game folder rebuild the preview. The browser
simulator also accepts arbitrary `.tgb` files, validates and stores them in
browser localStorage, and registers them after restart. Engine integration
tests cover install, persistence, registration, launch and invalid-bundle
rejection. Browser quota may be lower than the simulator's virtual storage cap.
The firmware store path is build-tested but has not been flashed or exercised
on hardware.

The host entry points are `python3 tools/tg.py check <id>` and
`python3 tools/tg.py pack <id>`; they read `games/<id>/` and write the packed
bundle beneath the ignored `build/games/` directory. `python3 tools/tg.py push
<id> --board 147b|169` validates and packs the same source, then sends it over
USB serial. Choose the physically confirmed model: the firmware-reported
profile does not identify the assembly. Before starting a transfer the host
requires `install 1 capabilities` to return the matching compiled profile and
game API. Older firmware without this response is rejected; it must not be
flashed without separate authorization. The
`python3 tools/tg.py describe <id> [--json]` command summarizes source metadata,
counts and candidate declarations; JSON is versioned and long lists/names are
truncated with counts and flags. Controls and part semantics are not yet
available. The
firmware protocol is versioned (`install 1`), uses ordered 768-byte base64
chunks with per-chunk CRC32 plus a whole-bundle CRC32, and cancels an idle
transfer after 30 seconds. Controller streaming and an install cannot run at
the same time; finish/cancel the install or stop streaming before switching.
CRC detects accidental corruption; it does not
authenticate a sender. Successful installs become launcher entries after a
console restart. The repository contains one editable Lua prototype and
non-playable test fixtures; none is silently installed as game content. The
Python validator uses the standard-library `tomllib` and therefore requires
Python 3.11 or newer; other existing TinyGame commands retain their existing
prerequisites. `check`, `describe`, `pack`, and `push` also compile every Lua source in text-only mode using
the pinned Lua parser without running it, including bundle-local modules. It
requires a C compiler and builds a cached parser on first use; the pinned Lua
source must already be cached or available for download. Each Lua source is
limited to 64 KiB, matching the current runtime's candidate limit. Syntax checking does
not validate runtime behavior, asset/level semantics, or module resolution.

The OS-level `BundleGame` adapter loads a stored bundle, runs the Lua update
callback, and feeds its bounded draw frame to the preallocated LVGL presenter.
Persistent-store transaction/recovery behavior, the serial install path, and
launcher startup have not been tested on-device or under filesystem fault
injection. This remains a prototype rather than an accepted end-to-end creator
loop.
