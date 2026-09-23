idf_component_register(
    SRCS
        "test_main.c"
        "lvgl_test_env.c"
        "test_gauge_widget.c"
        "test_gauge_needle.c"
    INCLUDE_DIRS "."
    REQUIRES gauge lvgl unity esp_timer
)
