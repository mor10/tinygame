# Lua is built with 32-bit numbers, by one edit to luaconf.h

- **Date:** 2026-11-11
- **Status:** decided by the implementer; flagged to the maintainer for review

## Question
The spec asks for Lua built for 32-bit numbers. Lua 5.4.7's `luaconf.h` sets `LUA_32BITS` to 0 unconditionally, so a compiler flag cannot turn it on.

## Options considered
- Keep Lua unmodified and run with 64-bit integers and doubles.
- Change one line of `luaconf.h` so the build flag works.

## Decision
`third_party/lua/src/luaconf.h` wraps the definition in `#if !defined(LUA_32BITS)`. The build passes `-DLUA_32BITS=1`. Every other vendored file is byte-identical to tag `v5.4.7` (commit `1ab3208`).

## Evidence
The ESP32-S3 has a single-precision floating-point unit; doubles are computed in software. Editing `luaconf.h` is the configuration method Lua's own documentation describes. A host probe confirms `lua_Number` and `lua_Integer` are 4 bytes after the change. The speed difference on the console is not yet measured.

## Reopen if
The Lua speed spike shows no meaningful gain, or a game needs integers above about two billion or more precise fractions.
