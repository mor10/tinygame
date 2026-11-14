idf_component_register(
    SRCS "main.c" "lab.c"
    INCLUDE_DIRS "."
    REQUIRES tthal font esp_driver_gpio esp_driver_usb_serial_jtag esp_timer esp_psram
)
