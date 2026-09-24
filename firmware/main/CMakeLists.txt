set(COMPONENT_SRCS
    "app_main.c"
    "app_console.c"
    "app_tests.c"
)

idf_component_register(
    SRCS ${COMPONENT_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES bsp gfx console driver esp_timer freertos nvs_flash
)
