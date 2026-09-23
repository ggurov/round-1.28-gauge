idf_component_register(
    SRCS "bsp.c"
    INCLUDE_DIRS "include"
    REQUIRES lvgl esp_lcd esp_driver_spi esp_driver_ledc esp_timer freertos
    # esp_lcd_gc9a01.h is only used inside bsp.c
    PRIV_REQUIRES espressif__esp_lcd_gc9a01
)
