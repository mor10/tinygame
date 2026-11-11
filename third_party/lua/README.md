# Lua 5.4.7 (vendored)

The Lua interpreter used by the rules host (`engine/lua/script.cpp`).

- Version: Lua 5.4.7
- Source: `git clone --depth 1 --branch v5.4.7 https://github.com/lua/lua`
- Tag commit: `1ab3208a1fceb12fca8f24ba57d6e13c5bff15e3`
- The files in `src/` are byte for byte the tag's, except the one change below. Do not edit them further.
  Lua is configured only by compiler flags and by which libraries the host
  opens.
- **One local change:** `src/luaconf.h` wraps `#define LUA_32BITS 0` in
  `#if !defined(LUA_32BITS)`, so the build's `-DLUA_32BITS=1` takes effect and
  Lua uses 32-bit integers and 32-bit floats (the ESP32-S3 has only a
  single-precision FPU). Every other file is byte-identical to the tag.
  See `docs/decisions/0009-lua-32-bit-numbers.md`.

## Which files are here

Rule: every `*.c` and `*.h` file from the top folder of the tagged commit
(the Lua core and its standard libraries), except these five:

- `lua.c` (the stand-alone interpreter program)
- `luac.c` (the stand-alone compiler; not in that repository at all)
- `onelua.c` (a build of everything as one file)
- `ltests.c` and `ltests.h` (Lua's internal test hooks)

Nothing else was copied: no test suite (`testes/`), manual, or makefiles.
That gives 59 files: 32 `.c` and 27 `.h`.

The standard libraries the game scripts must not reach (`liolib.c`,
`loslib.c`, `loadlib.c`, `ldblib.c`, `lcorolib.c`, `lutf8lib.c`) are compiled
and linked because the build takes every `.c` file in `src/`, but the host
never opens them, so scripts cannot call them. `linit.c` (`luaL_openlibs`) is
likewise never called. On Linux the linker prints a warning about `tmpnam`
in `loslib.c`; it is harmless here.

## Licence

Lua is free software under the MIT licence
(<https://www.lua.org/license.html>). The notice from `src/lua.h`:

```
/******************************************************************************
* Copyright (C) 1994-2024 Lua.org, PUC-Rio.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/

```
