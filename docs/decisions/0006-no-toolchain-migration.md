# The toolchain is not migrated during the rebuild

- **Date:** 2026-11-11
- **Status:** decided

## Question
Move off PlatformIO espressif32 6.12.0 (Arduino core 2.x) as part of v2?

## Options considered
- Migrate to ESP-IDF or a community platform fork now.
- Keep v1's pinned toolchain for the rebuild.

## Decision
Keep PlatformIO espressif32 6.12.0, Arduino core 2.x, C++17 and LVGL 9.6.0.

## Evidence
Combining a rewrite with a toolchain change makes failures hard to attribute. The frozen Arduino core is recorded as an open risk in spec section 18.

## Reopen if
A needed feature (for example the display driver) cannot be built on the current toolchain.
