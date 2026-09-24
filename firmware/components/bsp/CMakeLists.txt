idf_component_register(
    SRCS "bsp.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_lcd esp_driver_spi esp_driver_ledc freertos
    # esp_lcd_gc9a01.h is only used inside bsp.c
    PRIV_REQUIRES espressif__esp_lcd_gc9a01
)
