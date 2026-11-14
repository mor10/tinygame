# TinyTwist board drivers (named tthal because ESP-IDF already has a "hal" component), shared by the screen lab (firmware/) and the console (console/).
idf_component_register(
    SRCS "lcd.c" "input.c" "imu.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd esp_driver_spi esp_driver_gpio esp_driver_ledc esp_driver_pcnt esp_driver_i2c esp_timer
)
