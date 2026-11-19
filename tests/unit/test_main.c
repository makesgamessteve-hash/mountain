/* Unit test runner for the Mountain engine. MIT License. */
#include "test.h"

#include <stdlib.h>

int mtn_test_failures = 0;
int mtn_test_checks = 0;
const char *mtn_test_current = "";

#define SUITES(X)                                                                                  \
    X(math) X(rng) X(terrain) X(sky) X(weather) X(mesh) X(flora) X(fauna) X(player) X(house) X(room) \
        X(photo) X(input) X(audio) X(vfs) X(game) X(script)

#define DECL(n) void mtn_suite_##n(void);
SUITES(DECL)
#undef DECL

int main(int argc, char **argv) {
    const char *only = argc > 1 ? argv[1] : NULL;
#define RUN(n)                                                \
    if (!only || strcmp(only, #n) == 0) {                     \
        int before = mtn_test_failures;                       \
        mtn_suite_##n();                                      \
        printf("%-12s %s\n", #n, mtn_test_failures == before ? "ok" : "FAILED"); \
    }
    SUITES(RUN)
#undef RUN
    printf("%d checks, %d failures\n", mtn_test_checks, mtn_test_failures);
    return mtn_test_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
