/*
 * test_main.c - on-target test entry point.
 *
 * Prints a final machine-readable line so tools/run_device_tests.py can tell a
 * completed run from a hang or a panic:
 *
 *     TESTS_COMPLETE total=<n> failures=<n> ignored=<n>
 */
#include <stdio.h>

#include "lvgl_test_env.h"
#include "unity.h"

void app_main(void)
{
    lv_display_t *disp = lvgl_test_env_init();
    if (!disp) {
        printf("\nTESTS_COMPLETE total=0 failures=1 ignored=0\n");
        fflush(stdout);
        return;
    }

    printf("\n==== round-1.28-gauge on-target tests ====\n");
    fflush(stdout);

    unity_run_all_tests();

    printf("\n==== TESTS_COMPLETE total=%d failures=%d ignored=%d ====\n",
           Unity.NumberOfTests, Unity.TestFailures, Unity.TestIgnores);
    fflush(stdout);
}
