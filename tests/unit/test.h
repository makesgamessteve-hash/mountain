/* Minimal unit test framework for the Mountain engine. MIT License. */
#ifndef MTN_TEST_H
#define MTN_TEST_H

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef void (*mtn_test_fn)(void);
typedef struct { const char *name; mtn_test_fn fn; } mtn_test_case;

extern int mtn_test_failures;
extern int mtn_test_checks;
extern const char *mtn_test_current;

#define CHECK(cond)                                                                        \
    do {                                                                                   \
        mtn_test_checks++;                                                                 \
        if (!(cond)) {                                                                     \
            mtn_test_failures++;                                                           \
            fprintf(stderr, "  FAIL %s:%d [%s] %s\n", __FILE__, __LINE__, mtn_test_current, \
                    #cond);                                                                \
        }                                                                                  \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                                      \
    do {                                                                                           \
        double _a = (double)(a), _b = (double)(b);                                                 \
        mtn_test_checks++;                                                                         \
        if (!(fabs(_a - _b) <= (eps))) {                                                           \
            mtn_test_failures++;                                                                   \
            fprintf(stderr, "  FAIL %s:%d [%s] %s=%g != %s=%g\n", __FILE__, __LINE__,              \
                    mtn_test_current, #a, _a, #b, _b);                                             \
        }                                                                                          \
    } while (0)

#define CHECK_STR(a, b) CHECK(strcmp((a), (b)) == 0)

/* Each test file defines a suite: `MTN_SUITE(name) { TEST_RUN(fn); ... }` */
#define MTN_SUITE(name) void mtn_suite_##name(void)
#define TEST_RUN(fn)                    \
    do {                                \
        mtn_test_current = #fn;         \
        fn();                           \
    } while (0)

#endif
