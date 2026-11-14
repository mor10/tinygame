idf_component_register(
    SRCS "main.c" "display.c" "gfx.c" "game.c" "engine.c"
    INCLUDE_DIRS "."
    REQUIRES tthal font lua esp_driver_usb_serial_jtag esp_timer esp_psram
    EMBED_TXTFILES "../carts/hello.lua" "../carts/brick.lua" "prelude.lua"
)
