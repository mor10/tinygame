# Lua 5.4.9 from lua.org (sha256 2335b6c5...6fb8e6), vendored unchanged except one guard in
# luaconf.h so LUA_32BITS can be set here. Left out on purpose: lua.c, luac.c (standalone tools),
# liolib.c, loslib.c, loadlib.c, ldblib.c (the cart sandbox has no io/os/package/debug) and
# linit.c (the host opens only the libraries it allows).
set(src lua-5.4.9/src)
idf_component_register(
    SRCS ${src}/lapi.c ${src}/lcode.c ${src}/lctype.c ${src}/ldebug.c ${src}/ldo.c ${src}/ldump.c
         ${src}/lfunc.c ${src}/lgc.c ${src}/llex.c ${src}/lmem.c ${src}/lobject.c ${src}/lopcodes.c
         ${src}/lparser.c ${src}/lstate.c ${src}/lstring.c ${src}/ltable.c ${src}/ltm.c ${src}/lundump.c
         ${src}/lvm.c ${src}/lzio.c
         ${src}/lauxlib.c ${src}/lbaselib.c ${src}/lcorolib.c ${src}/lmathlib.c ${src}/lstrlib.c
         ${src}/ltablib.c ${src}/lutf8lib.c
    INCLUDE_DIRS ${src}
)
target_compile_definitions(${COMPONENT_LIB} PUBLIC LUA_32BITS=1)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error -O2)
# Same float behaviour as the simulator: no fused multiply-add (the S3 FPU has one).
target_compile_options(${COMPONENT_LIB} PRIVATE -ffp-contract=off)
