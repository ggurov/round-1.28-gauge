idf_component_register(
    SRCS "bsp.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl esp_lcd esp_driver_spi esp_driver_ledc esp_timer freertos
)
