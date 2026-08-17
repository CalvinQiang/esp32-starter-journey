idf_component_register(
    SRCS "app_main.c" "lcd_st7789.c"
    INCLUDE_DIRS "."
    REQUIRES esp_lcd driver esp_timer esp_psram lvgl esp_lvgl_port
)
