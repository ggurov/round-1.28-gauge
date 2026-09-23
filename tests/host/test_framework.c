/*
 * test_framework.c - registry, runner and reporting.
 */
#include "test_framework.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define TF_MAX_TESTS 256

typedef struct {
    const char *suite;
    const char *name;
    tf_test_fn_t fn;
} tf_entry_t;

static tf_entry_t s_tests[TF_MAX_TESTS];
static int s_count;
static int s_current_failures;
static int s_total_failures;
static int s_total_asserts;

void tf_register(const char *suite, const char *name, tf_test_fn_t fn)
{
    if (s_count >= TF_MAX_TESTS) {
        printf("test_framework: registry full (%d), dropping %s.%s\n",
               TF_MAX_TESTS, suite, name);
        return;
    }
    s_tests[s_count].suite = suite;
    s_tests[s_count].name = name;
    s_tests[s_count].fn = fn;
    s_count++;
}

void tf_fail(const char *file, int line, const char *fmt, ...)
{
    /* Trim the path down to the file name for readability. */
    const char *short_file = file;
    for (const char *p = file; *p; p++) {
        if (*p == '/' || *p == '\\') {
            short_file = p + 1;
        }
    }

    printf("      %s:%d: ", short_file, line);
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");

    s_current_failures++;
    s_total_failures++;
}

int tf_current_failures(void)
{
    return s_current_failures;
}

static bool suite_matches(const char *suite, const char *filter)
{
    if (!filter || !*filter) {
        return true;
    }
    /* substring match so `math` picks up `gauge_math` */
    return strstr(suite, filter) != NULL;
}

int tf_run_all(const char *suite_filter)
{
    int ran = 0;
    int failed = 0;
    const char *last_suite = NULL;

    printf("\n==================== host tests ====================\n");

    for (int i = 0; i < s_count; i++) {
        const tf_entry_t *t = &s_tests[i];
        if (!suite_matches(t->suite, suite_filter)) {
            continue;
        }
        if (!last_suite || strcmp(last_suite, t->suite) != 0) {
            printf("\n  [%s]\n", t->suite);
            last_suite = t->suite;
        }

        s_current_failures = 0;
        s_total_asserts++;
        t->fn();
        ran++;

        if (s_current_failures == 0) {
            printf("    PASS  %s\n", t->name);
        } else {
            printf("    FAIL  %s  (%d)\n", t->name, s_current_failures);
            failed++;
        }
    }

    printf("\n-------------------- summary --------------------\n");
    if (ran == 0) {
        printf("  no tests matched \"%s\"\n", suite_filter ? suite_filter : "");
        return 1;
    }
    printf("  tests : %d run, %d passed, %d failed\n", ran, ran - failed, failed);
    printf("  result: %s\n", failed == 0 ? "OK" : "FAILED");
    printf("===================================================\n\n");

    return failed;
}
