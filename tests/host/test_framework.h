/*
 * test_framework.h - a tiny test registry that works identically on the host
 * (MinGW GCC) and on the ESP32-S3 (xtensa GCC + Unity is not required).
 *
 * Registering a test is a single macro; there is no list to keep in sync:
 *
 *     TF_TEST(gauge_math, value_to_angle_maps_endpoints) {
 *         gauge_scale_t s = { ... };
 *         TF_EQ_INT((int)gauge_math_value_to_angle(&s, s.min), 225);
 *     }
 *
 * Registration happens through a constructor function, which both GCC targets
 * support, so tests are picked up automatically by main().
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*tf_test_fn_t)(void);

/* Called by the TF_TEST machinery; not usually needed directly. */
void tf_register(const char *suite, const char *name, tf_test_fn_t fn);

/* Runs every registered test, optionally filtered by suite name.
 * Returns the number of failed tests (0 == success). */
int tf_run_all(const char *suite_filter);

/* Records a failure.  Called by the TF_* assertion macros. */
void tf_fail(const char *file, int line, const char *fmt, ...);

/* Number of assertions that have failed in the current test. */
int tf_current_failures(void);

/* -------------------------------------------------------------------------- */

#define TF_TEST(suite, name)                                                    \
    static void suite##_##name(void);                                           \
    static void tf_ctor_##suite##_##name(void) __attribute__((constructor));     \
    static void tf_ctor_##suite##_##name(void)                                  \
    {                                                                           \
        tf_register(#suite, #name, suite##_##name);                             \
    }                                                                           \
    static void suite##_##name(void)

/* -------------------------------------------------------------------------- */
/* assertions                                                                 */
/* -------------------------------------------------------------------------- */

/* Soft assertion: records a failure and keeps going. */
#define TF_CHECK(cond)                                                          \
    do {                                                                        \
        if (!(cond)) {                                                          \
            tf_fail(__FILE__, __LINE__, "CHECK failed: %s", #cond);             \
        }                                                                       \
    } while (0)

#define TF_CHECK_MSG(cond, ...)                                                 \
    do {                                                                        \
        if (!(cond)) {                                                          \
            tf_fail(__FILE__, __LINE__, __VA_ARGS__);                           \
        }                                                                       \
    } while (0)

/* Hard assertion: records a failure and abandons the test (use before deref). */
#define TF_REQUIRE(cond)                                                        \
    do {                                                                        \
        if (!(cond)) {                                                          \
            tf_fail(__FILE__, __LINE__, "REQUIRE failed: %s", #cond);           \
            return;                                                             \
        }                                                                       \
    } while (0)

#define TF_EQ_INT(actual, expected)                                             \
    do {                                                                        \
        long long tf_a_ = (long long)(actual);                                  \
        long long tf_e_ = (long long)(expected);                                \
        if (tf_a_ != tf_e_) {                                                   \
            tf_fail(__FILE__, __LINE__, "%s == %s: got %lld, want %lld",        \
                    #actual, #expected, tf_a_, tf_e_);                          \
        }                                                                       \
    } while (0)

#define TF_NEAR(actual, expected, eps)                                          \
    do {                                                                        \
        double tf_a_ = (double)(actual);                                        \
        double tf_e_ = (double)(expected);                                      \
        double tf_d_ = tf_a_ - tf_e_;                                           \
        if (tf_d_ < 0) tf_d_ = -tf_d_;                                          \
        if (!(tf_d_ <= (double)(eps))) {                                        \
            tf_fail(__FILE__, __LINE__, "%s ~ %s: got %.6f, want %.6f (tol %g)",\
                    #actual, #expected, tf_a_, tf_e_, (double)(eps));           \
        }                                                                       \
    } while (0)

#define TF_GE(actual, floor_)                                                   \
    do {                                                                        \
        double tf_a_ = (double)(actual);                                        \
        double tf_f_ = (double)(floor_);                                        \
        if (!(tf_a_ >= tf_f_)) {                                                \
            tf_fail(__FILE__, __LINE__, "%s >= %s: got %g, floor %g",           \
                    #actual, #floor_, tf_a_, tf_f_);                            \
        }                                                                       \
    } while (0)

#define TF_LE(actual, ceil_)                                                    \
    do {                                                                        \
        double tf_a_ = (double)(actual);                                        \
        double tf_c_ = (double)(ceil_);                                         \
        if (!(tf_a_ <= tf_c_)) {                                                \
            tf_fail(__FILE__, __LINE__, "%s <= %s: got %g, ceiling %g",         \
                    #actual, #ceil_, tf_a_, tf_c_);                             \
        }                                                                       \
    } while (0)

#define TF_STR_EQ(actual, expected)                                             \
    do {                                                                        \
        const char *tf_a_ = (actual);                                           \
        const char *tf_e_ = (expected);                                         \
        if (!tf_a_ || !tf_e_ || strcmp(tf_a_, tf_e_) != 0) {                    \
            tf_fail(__FILE__, __LINE__, "%s == %s: got \"%s\", want \"%s\"",    \
                    #actual, #expected, tf_a_ ? tf_a_ : "(null)",               \
                    tf_e_ ? tf_e_ : "(null)");                                  \
        }                                                                       \
    } while (0)

#define TF_NOT_NULL(ptr)                                                        \
    do {                                                                        \
        if ((ptr) == NULL) {                                                    \
            tf_fail(__FILE__, __LINE__, "%s is NULL", #ptr);                    \
        }                                                                       \
    } while (0)

#ifdef __cplusplus
}
#endif
