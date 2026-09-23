idf_component_register(
    SRCS
        "gauge.c"
        "gauge_theme.c"
        "gauge_presets.c"
        "gauge_needle.c"
    INCLUDE_DIRS "include"
    EMBED_FILES "assets/needle_argb8888.bin"
    REQUIRES lvgl
)
