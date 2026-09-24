# The gauge is split so that only the renderer needs a graphics library:
#
#   gauge_math.c / gauge_theme.c / gauge_presets.c   pure C, no dependencies
#   gauge_render.c                                   draws into a gfx surface
#
# The LVGL widget that used to live here was dropped: at 869 KB the firmware
# was three quarters library, and the panel did not render any better for it.
idf_component_register(
    SRCS
        "gauge_math.c"
        "gauge_theme.c"
        "gauge_presets.c"
    INCLUDE_DIRS "include"
)
