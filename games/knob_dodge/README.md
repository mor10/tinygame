# KNOB DODGE

Turn the main knob to move the green square. Dodge the falling block. Click
either knob to start over after a hit.

This is the first editable Lua gameplay prototype. Its update, input, and
drawing commands are covered by host tests. Its world bounds adapt to the
runtime screen size, including the candidate 1.47B and 1.69 viewports. The
simulator packages this source and launches it through the same C++ bundle
adapter and Lua runtime used by installed games; edits to this folder rebuild
the WASM preview. Board installation uses `python3 tools/tg.py push knob_dodge
--board 147b` (or `--board 169` for a physically confirmed 1.69) once compatible
install firmware is on the device; any flash remains separately authorized.
Check and pack it with `python3 tools/tg.py check knob_dodge` and
`python3 tools/tg.py pack knob_dodge`.
