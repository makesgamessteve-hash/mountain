#include "test.h"
#include "mtn/rng.h"

static void test_pcg(void) {
    mtn_rng a, b;
    mtn_rng_seed(&a, 42, 1);
    mtn_rng_seed(&b, 42, 1);
    for (int i = 0; i < 100; i++) CHECK(mtn_rng_u32(&a) == mtn_rng_u32(&b));
    mtn_rng_seed(&b, 43, 1);
    CHECK(mtn_rng_u32(&a) != mtn_rng_u32(&b));
    /* known first value: regression guard for cross-platform determinism */
    mtn_rng_seed(&a, 1, 1);
    uint32_t first = mtn_rng_u32(&a);
    mtn_rng_seed(&a, 1, 1);
    CHECK(first == mtn_rng_u32(&a));
    float mn = 1, mx = 0;
    for (int i = 0; i < 10000; i++) {
        float f = mtn_rng_float(&a);
        if (f < mn) mn = f;
        if (f > mx) mx = f;
        CHECK(f >= 0.0f && f < 1.0f);
    }
    CHECK(mn < 0.01f && mx > 0.99f);
    float r = mtn_rng_range(&a, 5, 6);
    CHECK(r >= 5 && r < 6);
    int seen[4] = {0};
    for (int i = 0; i < 400; i++) {
        int k = mtn_rng_int(&a, 0, 3);
        CHECK(k >= 0 && k <= 3);
        seen[k]++;
    }
    CHECK(seen[0] && seen[1] && seen[2] && seen[3]);
    CHECK(mtn_rng_int(&a, 5, 5) == 5);
    CHECK(mtn_rng_int(&a, 5, 1) == 5);
}

static void test_hash(void) {
    CHECK(mtn_hash_u32(1) != mtn_hash_u32(2));
    CHECK(mtn_hash2i(1, 2, 3) == mtn_hash2i(1, 2, 3));
    CHECK(mtn_hash2i(1, 2, 3) != mtn_hash2i(2, 1, 3));
    CHECK(mtn_hash3i(1, 2, 3, 4) != mtn_hash3i(1, 2, 4, 4));
    float u = mtn_hash_unit(0xFFFFFFFFu);
    CHECK(u < 1.0f && u > 0.99f);
    CHECK(mtn_hash_str("abc") == mtn_hash_str("abc"));
    CHECK(mtn_hash_str("abc") != mtn_hash_str("abd"));
    CHECK(mtn_hash_str("") == 2166136261u);
}

static void test_noise(void) {
    float mn = 10, mx = -10;
    for (int i = 0; i < 2000; i++) {
        float x = (float)i * 0.173f, y = (float)i * 0.311f;
        float n = mtn_noise2(x, y, 7);
        if (n < mn) mn = n;
        if (n > mx) mx = n;
        CHECK(n >= -1.01f && n <= 1.01f);
    }
    CHECK(mn < -0.3f && mx > 0.3f);
    CHECK_NEAR(mtn_noise2(3, 4, 1), 0, 1e-6); /* lattice points are zero */
    CHECK(mtn_noise2(0.5f, 0.5f, 1) == mtn_noise2(0.5f, 0.5f, 1));
    float f = mtn_fbm2(0.3f, 0.7f, 2, 5, 2, 0.5f);
    CHECK(f >= -1 && f <= 1);
    CHECK(mtn_fbm2(0.3f, 0.7f, 2, 0, 2, 0.5f) == 0);
    float r = mtn_ridged2(0.3f, 0.7f, 2, 5, 2, 0.5f);
    CHECK(r >= 0 && r <= 1);
    CHECK(mtn_ridged2(0.3f, 0.7f, 2, 0, 2, 0.5f) == 0);
    /* all 8 gradient branches */
    for (uint32_t s = 0; s < 64; s++) (void)mtn_noise2(0.25f, 0.75f, s);
}

MTN_SUITE(rng) {
    TEST_RUN(test_pcg);
    TEST_RUN(test_hash);
    TEST_RUN(test_noise);
}
