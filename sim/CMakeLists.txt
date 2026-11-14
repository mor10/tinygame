# TinyGame simulator: the same engine and Lua as the console, built for the host.
#   cmake -S sim -B sim/build && cmake --build sim/build
cmake_minimum_required(VERSION 3.16)
project(tinygame_sim C)
set(CMAKE_C_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(LUA ${ROOT}/components/lua/lua-5.4.9/src)
set(LUA_SRCS
  ${LUA}/lapi.c ${LUA}/lcode.c ${LUA}/lctype.c ${LUA}/ldebug.c ${LUA}/ldo.c ${LUA}/ldump.c
  ${LUA}/lfunc.c ${LUA}/lgc.c ${LUA}/llex.c ${LUA}/lmem.c ${LUA}/lobject.c ${LUA}/lopcodes.c
  ${LUA}/lparser.c ${LUA}/lstate.c ${LUA}/lstring.c ${LUA}/ltable.c ${LUA}/ltm.c ${LUA}/lundump.c
  ${LUA}/lvm.c ${LUA}/lzio.c
  ${LUA}/lauxlib.c ${LUA}/lbaselib.c ${LUA}/lcorolib.c ${LUA}/lmathlib.c ${LUA}/lstrlib.c
  ${LUA}/ltablib.c ${LUA}/lutf8lib.c)

# Embed prelude.lua as a C array; cmake re-runs when the file changes.
set(PRELUDE ${ROOT}/components/tinygame/prelude.lua)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${PRELUDE})
file(READ ${PRELUDE} PRELUDE_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," PRELUDE_BYTES "${PRELUDE_HEX}")
file(WRITE ${CMAKE_BINARY_DIR}/prelude_gen.c
  "const char prelude_lua[] = {${PRELUDE_BYTES}0};\nconst unsigned prelude_lua_len = sizeof prelude_lua - 1;\n")

set(ENGINE_SRCS
  platform_host.c ${CMAKE_BINARY_DIR}/prelude_gen.c
  ${ROOT}/components/tinygame/vm.c ${ROOT}/components/tinygame/gfx.c ${ROOT}/components/tinygame/engine.c ${ROOT}/components/tinygame/os.c
  ${ROOT}/components/font/font5x7.c
  ${LUA_SRCS})

if(EMSCRIPTEN)
  # Browser build: emcmake cmake -S sim -B sim/build-web && cmake --build sim/build-web
  add_executable(tinygame-web web/wasm_main.c ${ENGINE_SRCS})
  target_include_directories(tinygame-web PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${ROOT}/components/tinygame/include ${ROOT}/components/font/include ${LUA})
  target_compile_definitions(tinygame-web PRIVATE LUA_32BITS=1)
  target_compile_options(tinygame-web PRIVATE -ffp-contract=off)
  set_target_properties(tinygame-web PROPERTIES OUTPUT_NAME tinygame)
  target_link_options(tinygame-web PRIVATE
    -sMODULARIZE=1 -sEXPORT_NAME=TinyGame -sENVIRONMENT=web,node -sALLOW_MEMORY_GROWTH=1
    -sEXPORTED_RUNTIME_METHODS=cwrap,HEAPU8,stringToNewUTF8,lengthBytesUTF8
    -sEXPORTED_FUNCTIONS=_malloc,_free)
  return()
endif()

add_executable(tinygame-sim main.c png.c ${ENGINE_SRCS})
target_include_directories(tinygame-sim PRIVATE ${ROOT}/components/tinygame/include ${ROOT}/components/font/include ${LUA})
target_compile_definitions(tinygame-sim PRIVATE LUA_32BITS=1)
# Match the board's single-precision float behaviour: no fused multiply-add, no excess precision.
target_compile_options(tinygame-sim PRIVATE -ffp-contract=off)
target_link_libraries(tinygame-sim PRIVATE m)
