/*
 * test_needle_asset.c - validates the generated needle sprite.
 *
 * The sprite is produced by tools/gen_needle.py and embedded as a raw blob, so
 * nothing else in the build would notice if it were regenerated at the wrong
 * size, off-centre, or pointing the wrong way.  Since the needle's position in
 * the dial comes from the constants in gauge_needle_size.h, those constants and
 * the artwork have to agree.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gauge_needle_size.h"
#include "test_framework.h"

#ifndef NEEDLE_BIN_PATH
#define NEEDLE_BIN_PATH "firmware/components/gauge/assets/needle_argb8888.bin"
#endif

static unsigned char *s_blob;
static long           s_blob_len;

static unsigned char alpha_at(int x, int y)
{
    long off = ((long)y * GAUGE_NEEDLE_W + x) * GAUGE_NEEDLE_BPP;
    return s_blob[off + 3];   /* BGRA little-endian -> A is last */
}

static void load_blob(void)
{
    if (s_blob) {
        return;
    }
    FILE *f = fopen(NEEDLE_BIN_PATH, "rb");
    if (!f) {
        printf("      cannot open %s - run: python tools/gen_needle.py\n", NEEDLE_BIN_PATH);
        return;
    }
    fseek(f, 0, SEEK_END);
    s_blob_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    s_blob = (unsigned char *)malloc((size_t)s_blob_len);
    if (s_blob && fread(s_blob, 1, (size_t)s_blob_len, f) != (size_t)s_blob_len) {
        free(s_blob);
        s_blob = NULL;
    }
    fclose(f);
}

/* -------------------------------------------------------------------------- */

TF_TEST(gauge_needle, blob_exists_and_is_the_declared_size)
{
    load_blob();
    TF_REQUIRE(s_blob != NULL);
    printf("      %s is %ld bytes (expected %d)\n", NEEDLE_BIN_PATH, s_blob_len,
           (int)GAUGE_NEEDLE_BYTES);
    TF_EQ_INT(s_blob_len, GAUGE_NEEDLE_BYTES);
}

TF_TEST(gauge_needle, geometry_constants_match_the_generator)
{
    /* these mirror SIZE / PIVOT in tools/gen_needle.py */
    TF_EQ_INT(GAUGE_NEEDLE_W, 176);
    TF_EQ_INT(GAUGE_NEEDLE_H, 176);
    TF_EQ_INT(GAUGE_NEEDLE_PIVOT_X, GAUGE_NEEDLE_W / 2);
    TF_EQ_INT(GAUGE_NEEDLE_PIVOT_Y, GAUGE_NEEDLE_H / 2);
    TF_EQ_INT(GAUGE_NEEDLE_BPP, 4);
    TF_EQ_INT(GAUGE_NEEDLE_BYTES, GAUGE_NEEDLE_W * GAUGE_NEEDLE_H * 4);
}

TF_TEST(gauge_needle, is_not_empty_and_not_a_solid_block)
{
    load_blob();
    TF_REQUIRE(s_blob != NULL);

    long covered = 0;
    for (long i = 3; i < s_blob_len; i += GAUGE_NEEDLE_BPP) {
        if (s_blob[i] > 0) {
            covered++;
        }
    }
    TF_GE(covered, 300);    /* a blade, not a blank */
    TF_LE(covered, GAUGE_NEEDLE_BYTES / 4 / 10);  /* and not a filled square */
}

TF_TEST(gauge_needle, corners_are_transparent)
{
    load_blob();
    TF_REQUIRE(s_blob != NULL);

    TF_EQ_INT(alpha_at(0, 0), 0);
    TF_EQ_INT(alpha_at(GAUGE_NEEDLE_W - 1, 0), 0);
    TF_EQ_INT(alpha_at(0, GAUGE_NEEDLE_H - 1), 0);
    TF_EQ_INT(alpha_at(GAUGE_NEEDLE_W - 1, GAUGE_NEEDLE_H - 1), 0);
}

TF_TEST(gauge_needle, pivot_is_covered_by_the_blade)
{
    load_blob();
    TF_REQUIRE(s_blob != NULL);
    TF_GE(alpha_at(GAUGE_NEEDLE_PIVOT_X, GAUGE_NEEDLE_PIVOT_Y), 255);
}

TF_TEST(gauge_needle, tip_sits_at_the_declared_distance)
{
    load_blob();
    TF_REQUIRE(s_blob != NULL);

    int top = -1;
    for (int y = 0; y < GAUGE_NEEDLE_H && top < 0; y++) {
        for (int x = 0; x < GAUGE_NEEDLE_W; x++) {
            if (alpha_at(x, y) > 0) {
                top = y;
                break;
            }
        }
    }
    TF_GE(top, 0);
    int distance = GAUGE_NEEDLE_PIVOT_Y - top;
    printf("      tip is %d px above the pivot (declared %d)\n",
           distance, GAUGE_NEEDLE_TIP_DISTANCE);
    TF_NEAR(distance, GAUGE_NEEDLE_TIP_DISTANCE, 3.0);
}

TF_TEST(gauge_needle, blade_points_up_and_has_a_counterweight_tail)
{
    load_blob();
    TF_REQUIRE(s_blob != NULL);

    /* there must be ink below the pivot (the tail the hub covers) */
    bool below = false;
    for (int y = GAUGE_NEEDLE_PIVOT_Y + 2; y < GAUGE_NEEDLE_H; y++) {
        if (alpha_at(GAUGE_NEEDLE_PIVOT_X, y) > 0) {
            below = true;
            break;
        }
    }
    TF_CHECK(below);

    /* and the blade must be narrow near the tip */
    int tip_row = GAUGE_NEEDLE_PIVOT_Y - GAUGE_NEEDLE_TIP_DISTANCE + 6;
    int width = 0;
    for (int x = 0; x < GAUGE_NEEDLE_W; x++) {
        if (alpha_at(x, tip_row) > 0) {
            width++;
        }
    }
    TF_GE(width, 1);
    TF_LE(width, 8);
}

TF_TEST(gauge_needle, blade_is_symmetric_about_the_pivot_column)
{
    load_blob();
    TF_REQUIRE(s_blob != NULL);

    /*
     * The raster is symmetric about the bitmap centre, which lies on the
     * boundary between the two middle columns, so mirror column x with
     * (W - 1 - x) rather than counting either side of the pivot column.
     */
    int left = 0, right = 0;
    for (int y = 0; y < GAUGE_NEEDLE_H; y++) {
        for (int x = 0; x < GAUGE_NEEDLE_W / 2; x++) {
            if (alpha_at(x, y) > 0) {
                left++;
            }
            if (alpha_at(GAUGE_NEEDLE_W - 1 - x, y) > 0) {
                right++;
            }
        }
    }
    /* mirrored, so the counts should be within a couple of percent */
    int diff = left > right ? left - right : right - left;
    int total = left + right;
    TF_GE(total, 300);
    TF_CHECK_MSG(diff * 100 <= total * 5,
                 "blade is lopsided: %d px left of pivot, %d right", left, right);
}

TF_TEST(gauge_needle, edges_are_antialiased)
{
    load_blob();
    TF_REQUIRE(s_blob != NULL);

    long partial = 0;
    for (long i = 3; i < s_blob_len; i += GAUGE_NEEDLE_BPP) {
        if (s_blob[i] > 0 && s_blob[i] < 255) {
            partial++;
        }
    }
    /* 4x supersampling should leave plenty of intermediate coverage values */
    TF_GE(partial, 50);
}

TF_TEST(gauge_needle, pixels_are_white_so_the_theme_can_recolour_them)
{
    load_blob();
    TF_REQUIRE(s_blob != NULL);

    for (long i = 0; i < s_blob_len; i += GAUGE_NEEDLE_BPP) {
        if (s_blob[i + 3] == 0) {
            continue;
        }
        /* B, G, R are little-endian and must all be 0xFF */
        if (s_blob[i] != 0xFF || s_blob[i + 1] != 0xFF || s_blob[i + 2] != 0xFF) {
            tf_fail(__FILE__, __LINE__,
                    "pixel at byte %ld is not white (B=%u G=%u R=%u); "
                    "recolouring would tint it incorrectly",
                    i, s_blob[i], s_blob[i + 1], s_blob[i + 2]);
            return;
        }
    }
}
