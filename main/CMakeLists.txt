idf_component_register(
    SRCS "app_main.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer esp_psram spi_flash esp_driver_ledc esp_driver_gpio esp_driver_rmt led_strip esp_adc esp_driver_i2c nvs_flash esp_lcd esp_wifi esp_event esp_netif esp_http_client lwip bt
)

