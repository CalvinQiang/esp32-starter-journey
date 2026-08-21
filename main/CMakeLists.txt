idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer esp_psram spi_flash esp_driver_ledc esp_driver_gpio esp_driver_rmt led_strip esp_adc
)

