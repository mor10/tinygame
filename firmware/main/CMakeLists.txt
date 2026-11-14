idf_component_register(
    SRCS "main.c" "lcd.c" "lab.c" "font5x7.c"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd esp_driver_spi esp_driver_gpio esp_driver_ledc esp_driver_usb_serial_jtag esp_timer esp_psram
)
