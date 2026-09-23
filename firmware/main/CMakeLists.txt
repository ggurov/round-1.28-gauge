set(COMPONENT_SRCS
    "app_main.c"
    "app_console.c"
    "gauge_demo.c"
)

idf_component_register(
    SRCS ${COMPONENT_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES bsp gauge lvgl esp_lcd nvs_flash esp_timer driver console
)
