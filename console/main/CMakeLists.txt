idf_component_register(
    SRCS "main.c" "display.c" "game.c" "platform_esp.c"
    INCLUDE_DIRS "."
    REQUIRES tthal font lua tinygame esp_driver_usb_serial_jtag esp_timer esp_psram
    EMBED_TXTFILES "../carts/hello.lua" "../carts/brick.lua"
    WHOLE_ARCHIVE   # tinygame calls back into platform_esp.c, so keep every object linked
)
