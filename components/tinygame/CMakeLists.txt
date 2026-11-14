# Portable engine: Lua host (vm), rasterizer (gfx), sprites and maps (engine), and the Lua prelude.
# Plain C11 plus Lua. Each target supplies the functions in include/platform.h.
# The same sources build for the simulator through sim/CMakeLists.txt.
idf_component_register(
    SRCS "vm.c" "gfx.c" "engine.c" "os.c"
    INCLUDE_DIRS "include"
    REQUIRES lua font
    EMBED_TXTFILES "prelude.lua"
)
# Same float behaviour as the simulator: no fused multiply-add (the S3 FPU has one).
target_compile_options(${COMPONENT_LIB} PRIVATE -ffp-contract=off)
