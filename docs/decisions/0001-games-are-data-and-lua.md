# Games are data plus Lua on a C++ engine

- **Date:** 2026-11-11
- **Status:** decided

## Question
How are games written and run?

## Options considered
- Compiled C++ against LVGL, as in v1.
- Text data and small Lua rules, run by a C++ engine built into the firmware and simulator.
- A scripting runtime for everything (MicroPython or Lua without an engine).

## Decision
Text data plus Lua rules on a C++ engine. Native C++ games remain possible as an escape hatch.

## Evidence
Design reasoning only; not yet measured. Expected gains: no firmware build to change a game, sandboxed sharing, far fewer tokens per change, and a format a small offline model can produce. Lua was chosen over a custom rule language because models already know it well.

## Reopen if
The Lua speed spike (docs/spikes/) shows 60 ticks per second with 64 actors is not reachable on the ESP32-S3.
