# TinyGame simulator canvas

This project extension runs TinyGame's **real OS and every game**, compiled to
WebAssembly with LVGL 9.6. Only the board drivers are replaced. New game folders
are included automatically.

Open this repository in the GitHub Copilot app and ask to open the **TinyGame
simulator** canvas. After adding the extension to an already-running session,
reload project extensions or reopen the session so it is discovered. Its canvas
id is `tinygame-simulator`; optional open input `{ "game": "maze" }` launches a
game after boot. First open downloads the pinned Emscripten toolchain and LVGL
if needed, then builds the firmware. The build requires macOS or Linux
(Windows: WSL), Python 3.10+, Git, curl and Node.js 22+.

The same interface runs in a normal browser with:

```sh
python3 tools/tg.py sim
```

## Controls

| Knob | Clockwise | Click (hold for pause/back) | Counterclockwise |
|---|---|---|---|
| Left | A | S | D |
| Right | J | K | L |

One turn key tap is four transitions; holding repeats. Drag a knob's rim to
rotate it (80 transitions per revolution), scroll over it, or press its centre.
Arrow keys or dragging the screen tilt the console; releasing springs back to
level. The little dot below the LED shows tilt. Input is released when the
browser loses focus so a key cannot become stuck.

## Agent actions

| Action | Input | Result |
|---|---|---|
| `reload` | `{}` | Restarts the firmware |
| `launch` | `{ "game": "brickbreaker" }` | Launch acknowledgement |
| `send` | `{ "commands": ["turn right 4", "click", "wait 1"] }` | Number sent after firmware acknowledgements |
| `screenshot` | `{ "label": "maze" }` | Native 320x172 PNG path in session `files/tinygame-simulator/` |
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

## USB controller

Flash firmware containing `stream on|off` once. Select **Use connected console**
to control the simulator with the physical knobs and accelerometer. The board
shows **CONTROLLING SIMULATOR** and ignores its own menus/games while connected.
Keyboard, mouse and screen tilt remain available alongside the board.

The serial port is **exclusive**: disconnect the controller before `flash`,
`send`, `shot` or `monitor`. Closing the canvas stops the bridge. Loss of USB or
host heartbeat returns the board to normal after a few seconds. With several
boards connected use the action's explicit `port`.

## How it works

Inspired by [Badger's badge simulator](https://github.com/badger/home/tree/main/.github/extensions/badge-simulator):
`joinSession` registers a `createCanvas` declaration, with one ephemeral-port
loopback HTTP server per panel. SSE carries action requests, input frames and
reload events; ordinary HTTP posts carry serial output, replies and PNGs.
Each server URL has an unguessable prefix and rejects cross-origin posts.
App theme tokens style the page around the intentionally dark console.

Watching `src/`, `include/` and `sim/` triggers debounced incremental builds.
Successful builds reload the page; failures keep the last working page and show
the compiler error. Build output is ignored by Git. Settings and best scores
persist in browser localStorage; game state does not. This is not an electrical
or timing-accurate emulator: knob feel, tilt direction and LED colours still
need checks on the real console.
