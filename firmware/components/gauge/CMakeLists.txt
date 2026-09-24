# The gauge is split so that only the renderer needs the graphics layer:
#
#   gauge_math.c / gauge_theme.c / gauge_presets.c   pure C, no dependencies
#   gauge_render.c                                   draws into a gfx surface
#
# The first three are covered by host unit tests; the renderer needs hardware.
idf_component_register(
    SRCS
        "gauge_math.c"
        "gauge_theme.c"
        "gauge_presets.c"
        "gauge_render.c"
    INCLUDE_DIRS "include"
    REQUIRES gfx
)
