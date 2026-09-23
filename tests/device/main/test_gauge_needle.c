/*
 * test_gauge_needle.c - validates the embedded sprite on the real target.
 *
 * The blob reaches the firmware through ESP-IDF's EMBED_FILES, which depends on
 * a linker symbol name derived from the asset's path.  If that ever changes,
 * gauge_needle_image() returns NULL and the needle silently disappears - this
 * test is what catches it.
 */
#include <string.h>

#include "gauge_needle.h"
#include "gauge_needle_size.h"
#include "lvgl_test_env.h"
#include "unity.h"

TEST_CASE("needle: embedded blob resolves to an image descriptor", "[needle]")
{
    const lv_image_dsc_t *dsc = gauge_needle_image();
    TEST_ASSERT_NOT_NULL_MESSAGE(dsc, "EMBED_FILES symbol did not resolve - check "
                                      "gauge_needle.c against the asset path");
    TEST_ASSERT_NOT_NULL(dsc->data);
}

TEST_CASE("needle: descriptor describes the expected bitmap", "[needle]")
{
    const lv_image_dsc_t *dsc = gauge_needle_image();
    TEST_ASSERT_NOT_NULL(dsc);

    TEST_ASSERT_EQUAL_UINT8(LV_IMAGE_HEADER_MAGIC, dsc->header.magic);
    TEST_ASSERT_EQUAL_UINT8(LV_COLOR_FORMAT_ARGB8888, dsc->header.cf);
    TEST_ASSERT_EQUAL_INT(GAUGE_NEEDLE_W, dsc->header.w);
    TEST_ASSERT_EQUAL_INT(GAUGE_NEEDLE_H, dsc->header.h);
    TEST_ASSERT_EQUAL_INT(GAUGE_NEEDLE_W * GAUGE_NEEDLE_BPP, dsc->header.stride);
    TEST_ASSERT_EQUAL_UINT32(GAUGE_NEEDLE_BYTES, dsc->data_size);
}

TEST_CASE("needle: the sprite is white with an alpha mask", "[needle]")
{
    const lv_image_dsc_t *dsc = gauge_needle_image();
    TEST_ASSERT_NOT_NULL(dsc);

    const uint8_t *px = (const uint8_t *)dsc->data;

    /* corners transparent, pivot opaque */
    const size_t corner = 0;
    const size_t pivot = ((size_t)GAUGE_NEEDLE_PIVOT_Y * GAUGE_NEEDLE_W + GAUGE_NEEDLE_PIVOT_X)
                         * GAUGE_NEEDLE_BPP;
    TEST_ASSERT_EQUAL_UINT8(0, px[corner + 3]);
    TEST_ASSERT_EQUAL_UINT8(255, px[pivot + 3]);

    /* every covered pixel must be pure white so image_recolor tints it cleanly */
    for (size_t i = 0; i < GAUGE_NEEDLE_BYTES; i += GAUGE_NEEDLE_BPP) {
        if (px[i + 3] == 0) {
            continue;
        }
        TEST_ASSERT_EQUAL_UINT8(0xFF, px[i + 0]);
        TEST_ASSERT_EQUAL_UINT8(0xFF, px[i + 1]);
        TEST_ASSERT_EQUAL_UINT8(0xFF, px[i + 2]);
    }
}

TEST_CASE("needle: the tip is the declared distance from the pivot", "[needle]")
{
    const lv_image_dsc_t *dsc = gauge_needle_image();
    TEST_ASSERT_NOT_NULL(dsc);
    const uint8_t *px = (const uint8_t *)dsc->data;

    int top = -1;
    for (int y = 0; y < GAUGE_NEEDLE_H && top < 0; y++) {
        for (int x = 0; x < GAUGE_NEEDLE_W; x++) {
            if (px[((size_t)y * GAUGE_NEEDLE_W + x) * GAUGE_NEEDLE_BPP + 3] > 0) {
                top = y;
                break;
            }
        }
    }
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, top);

    int distance = GAUGE_NEEDLE_PIVOT_Y - top;
    TEST_ASSERT_INT_WITHIN(3, GAUGE_NEEDLE_TIP_DISTANCE, distance);
}
