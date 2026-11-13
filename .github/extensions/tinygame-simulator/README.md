# TinyGame simulator canvas

This project extension runs TinyGame's **real OS and every game**, compiled to
WebAssembly with LVGL 9.6. Only the board drivers are replaced. New game folders
are included automatically.

Open this repository in the GitHub Copilot app and ask to open the **TinyGame
simulator** canvas. After adding the extension to an already-running session,
reload project extensions or reopen the session so it is discovered. Its canvas
id is `tinygame-simulator`; optional open input `{ "game": "brickbreaker" }` launches a
game after boot. First open downloads the pinned Emscripten toolchain and LVGL
if needed, then builds the firmware. The build requires macOS or Linux
(Windows: WSL), Python 3.11+, Git, curl and Node.js 22+.

The same interface runs in a normal browser with:

```sh
python3 tools/tg.py sim
python3 tools/tg.py sim --board 169
```

The standalone 1.69 profile is provisional and uses a 280x240 screen without
an RGB LED. The default 1.47B profile uses 320x172. Each has separate build
outputs and native-sized screenshots. Legacy game layouts are not yet adapted
to 1.69, and simulation does not verify physical hardware.

## Installing a text game

Pack a validated game with `python3 tools/tg.py pack <id>`, then choose
**Install .tgb** in the simulator and select its `build/games/<id>.tgb` file.
The simulator validates the bundle with the shared firmware code, saves it in
the current browser origin's localStorage, and restarts so it appears in the
launcher. Browser storage quotas vary; a failed write is reported and does not
replace an existing installed bundle. This installs only into the simulator.
To install on a physical console, use `python3 tools/tg.py push <id> --board
147b` or `--board 169`, matching the physically confirmed model.

## Controls

| Knob | Clockwise | Click / hold | Counterclockwise |
|---|---|---|---|
| Left | D | S | A |
| Right | L | K | J |

One turn key tap is four transitions; holding repeats. Drag a knob's rim to
rotate it (80 transitions per revolution), scroll over it, or press its centre.
Arrow keys or dragging the screen tilt the console; releasing springs back to
level. The little dot below the LED shows tilt. Input is released when the
browser loses focus so a key cannot become stuck. Individual knob holds are
game input. Hold S and K together, then release both before three seconds to
open Pause/back; keep both down for 3 seconds for the OS sleep prompt. Release
both before clicking to confirm Yes, or turn the main knob to No and click to
cancel. The gestures can be exercised with `down left`, `down right`, `wait`,
and `up left`, `up right`. `hold` alone injects a legacy long-press event and
does not open Pause.

## Agent actions

| Action | Input | Result |
|---|---|---|
| `reload` | `{}` | Restarts the firmware |
| `launch` | `{ "game": "brickbreaker" }` | Launch acknowledgement |
| `send` | `{ "commands": ["turn right 4", "click", "wait 1"] }` | Number sent after firmware acknowledgements |
| `screenshot` | `{ "label": "brickbreaker" }` | Native 320x172 PNG path in session `files/tinygame-simulator/` |
| `get_output` | `{ "since": 0 }` | Status, build error, connection, indexed serial lines and next cursor |
| `connect_device` | `{ "port": "/dev/cu.usbmodem..." }` or `{}` | USB connection status |
| `disconnect_device` | `{}` | Releases the serial port |

`send` accepts the existing dev protocol, including `turn`, `click`, `hold`,
`down`, `up`, `tilt`, `launch` and `info`; `wait` is handled by the browser.
Use `screenshot` rather than sending `shot` when saving a PNG. Open the page
and wait for **Ready** before calling actions. Actions fail explicitly if
the page is unavailable, a command fails or the firmware does not acknowledge.
Keep only one page open per server URL when using agent actions; extra tabs
are rejected rather than executing a command on multiple firmware instances.
Simulation time pauses while the page is hidden. Bring it to the foreground
and wait for Ready before sending actions.

### A useful game-check sequence

Open with `{ "game": "brickbreaker" }`, wait until `get_output` reports
`status: "ready"` and `pageReady: true`, then:

1. `send` with `{ "commands": ["click", "turn right 20", "wait 1"] }`.
2. `screenshot` with `{ "label": "brickbreaker-play" }`; inspect the returned PNG.
3. `send` with `{ "commands": ["down left", "down right", "up left", "up right"] }` and check the pause menu.
4. Release injected input with
   `{ "commands": ["up left", "up right", "tilt off"] }`.

Use `get_output`'s `next` value as the next `since` cursor. `hold` injects an
OS long-press event for diagnostics; holding one keyboard/mouse knob tests its
game-visible hold state, and holding both tests OS chord timing.
Reload after source edits starts a new run, not the previous game/level.
Do not call `reload` expecting a source build: it restarts the loaded firmware;
the file watcher handles recompilation.

## USB controller

Flash firmware containing `stream on|off` once. Select **Use connected console**
to control the simulator with the physical knobs and accelerometer. The board
shows **CONTROLLING SIMULATOR** and ignores its own menus/games while connected.
Keyboard, mouse and screen tilt remain available alongside the board.

The serial port is **exclusive**: disconnect the controller before `flash`,
`send`, `shot` or `monitor`. Closing the canvas stops the bridge. Loss of USB or
host heartbeat returns the board to normal after a few seconds. With several
boards connected use the action's explicit `port`.
The board must be awake; USB cannot wake deep sleep. Click a physical knob
before connecting. A no-frame error may mean old firmware, a sleeping board
or a serial port already in use. The bridge forwards tilt only, not raw
accelerometer/gyro gestures.

## How it works

Inspired by [Badger's badge simulator](https://github.com/badger/home/tree/main/.github/extensions/badge-simulator):
`joinSession` registers a `createCanvas` declaration, with one ephemeral-port
loopback HTTP server per panel. SSE carries action requests, input frames and
reload events; ordinary HTTP posts carry serial output, replies and PNGs.
Each server URL has an unguessable prefix and rejects cross-origin posts.
App theme tokens style the page around the intentionally dark console.

Watching `src/`, `include/`, `sim/`, `engine/` and `games/` triggers debounced
incremental builds. Changes to a valid `games/<id>/` bundle rebuild its packed
Lua source into the simulator, so the preview runs the same source through the
C++ `BundleGame` adapter without a browser-side rules copy.
Successful builds reload the page; failures keep the last working page and show
the compiler error. Build output is ignored by Git. Settings and best scores
persist in browser localStorage; game state does not. This is not an electrical
or timing-accurate emulator: knob feel, tilt direction and LED colours still
need checks on the real console.
Saves belong to the browser origin, so another port/browser has separate
scores and settings; the device's saves are also separate. For a consistent
standalone-browser origin use the same `tg.py sim --port <number>` each time.

`tg.py test` includes input-mapping and server/bridge tests when Node is
available. After building WebAssembly, `node sim/arduino/test_engine.mjs`
checks the real engine's boot, games, framebuffer, inputs, persistence and
sleep/wake. Changes to this extension additionally need live canvas discovery,
open and action checks; a working Browser preview alone does not verify the
extension provider's RPC routing.
