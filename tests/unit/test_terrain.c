#include "test.h"
#include "mtn/terrain.h"

#include <math.h>
#include <stdlib.h>

static mtn_terrain_params small_params(uint32_t seed) {
    mtn_terrain_params p;
    mtn_terrain_default_params(&p);
    p.seed = seed;
    p.size = 129;
    p.cell = 16.0f;
    p.erosion_drops = 4000;
    return p;
}

static void test_generate_deterministic(void) {
    mtn_terrain a, b;
    mtn_terrain_params p = small_params(1234);
    CHECK(mtn_terrain_generate(&a, &p));
    CHECK(mtn_terrain_generate(&b, &p));
    CHECK(memcmp(a.h, b.h, sizeof(float) * 129 * 129) == 0);
    CHECK(mtn_terrain_checksum(&a) == mtn_terrain_checksum(&b));
    CHECK(a.trail_len == b.trail_len);
    CHECK(a.vp_count == b.vp_count);
    p.seed = 99;
    mtn_terrain c;
    mtn_terrain_generate(&c, &p);
    CHECK(memcmp(a.h, c.h, sizeof(float) * 129 * 129) != 0);
    CHECK(mtn_terrain_checksum(&a) != mtn_terrain_checksum(&c));
    mtn_terrain_free(&b);
    mtn_terrain_free(&c);
    mtn_terrain_free(&a);
    CHECK(a.h == NULL);
}

static void test_queries(void) {
    mtn_terrain t;
    mtn_terrain_params p = small_params(7);
    mtn_terrain_generate(&t, &p);
    CHECK(t.max_h > t.min_h + 300.0f);
    CHECK(mtn_terrain_in_playable(&t, t.summit.x, t.summit.z));
    CHECK_NEAR(mtn_terrain_height(&t, t.summit.x, t.summit.z), t.summit.y, 1e-3);
    /* summit is the highest playable point */
    CHECK(mtn_terrain_altitude_frac(&t, t.summit.y) > 0.8f);
    CHECK_NEAR(mtn_terrain_altitude_frac(&t, t.min_h - 10), 0, 0);
    vec3 nrm = mtn_terrain_normal(&t, t.center.x, t.center.z);
    CHECK_NEAR(v3_len(nrm), 1, 1e-4);
    float sl = mtn_terrain_slope(&t, t.center.x, t.center.z);
    CHECK(sl >= 0 && sl <= 1);
    /* out of range sampling clamps */
    CHECK_NEAR(mtn_terrain_height(&t, -100, -100), mtn_terrain_grid(&t, 0, 0), 1e-3);
    CHECK_NEAR(mtn_terrain_grid(&t, 999, 999), t.h[128 * 129 + 128], 0);
    mtn_biome b = mtn_terrain_biome(&t, t.summit.x, t.summit.z);
    CHECK(b == MTN_BIOME_SNOW || b == MTN_BIOME_ROCK || b == MTN_BIOME_SCREE);
    CHECK_STR(mtn_biome_name(MTN_BIOME_FOREST), "forest");
    CHECK_STR(mtn_biome_name((mtn_biome)99), "unknown");
    float m = mtn_terrain_moisture(&t, t.center.x, t.center.z);
    CHECK(m >= 0 && m <= 1);
    /* playable clamp */
    vec3 far = v3(-500, 0, -500);
    CHECK(!mtn_terrain_in_playable(&t, far.x, far.z));
    CHECK(mtn_terrain_clamp_playable(&t, &far));
    CHECK(mtn_terrain_in_playable(&t, far.x + (t.center.x - far.x) * 0.01f, far.z + (t.center.z - far.z) * 0.01f));
    vec3 c = t.center;
    CHECK(!mtn_terrain_clamp_playable(&t, &c));
    /* raycast straight down hits the ground */
    vec3 hit;
    CHECK(mtn_terrain_raycast(&t, v3(t.center.x, 5000, t.center.z), v3(0, -1, 0), 10000, &hit));
    CHECK_NEAR(hit.y, mtn_terrain_height(&t, t.center.x, t.center.z), 0.5);
    CHECK(!mtn_terrain_raycast(&t, v3(t.center.x, 5000, t.center.z), v3(0, 1, 0), 100, NULL));
    mtn_terrain_free(&t);
}

static void test_trail_and_viewpoints(void) {
    mtn_terrain t;
    mtn_terrain_params p = small_params(42);
    mtn_terrain_generate(&t, &p);
    CHECK(t.trail_len > 10);
    CHECK(v3_dist_xz(t.trail[0], t.trailhead) < 40.0f);
    CHECK(v3_dist_xz(t.trail[t.trail_len - 1], t.summit) < 40.0f);
    CHECK(t.cairn_count >= 2);
    CHECK(v3_dist_xz(t.cairns[0], t.trail[0]) < 5.0f); /* a cairn marks the trailhead */
    CHECK(mtn_terrain_trail_distance(&t, t.trail[t.trail_len / 2].x, t.trail[t.trail_len / 2].z) < 12.0f);
    CHECK(t.vp_count >= 2);
    CHECK_NEAR(t.vps[0].pos.x, t.summit.x, 1e-3);
    for (int i = 1; i < t.vp_count; i++)
        for (int j = 0; j < i; j++) CHECK(v3_dist_xz(t.vps[i].pos, t.vps[j].pos) >= 220.0f);
    float o = mtn_terrain_openness(&t, v3(t.summit.x, t.summit.y + 50, t.summit.z), 0, 500);
    CHECK(o > 0.5f);
    float closed = mtn_terrain_openness(&t, v3(t.center.x, t.min_h - 100, t.center.z), 0, 500);
    CHECK(closed < 0.01f);
    mtn_terrain_free(&t);
}

/* The trail of a game-sized world: winding, gentle, level across, well marked. */
static void test_trail_shape(void) {
    mtn_terrain t;
    mtn_terrain_params p;
    mtn_terrain_default_params(&p);
    p.seed = 777;
    p.size = 257;
    p.cell = 12.0f;
    p.erosion_drops = 12000;
    CHECK(mtn_terrain_generate(&t, &p));
    CHECK(t.trail_len > 100);
    CHECK(v3_dist_xz(t.trail[t.trail_len - 1], t.summit) < 1.0f);
    CHECK(t.summit.y >= t.trail[t.trail_len - 1].y - 0.2f); /* the summit is not cut down */
    float len = 0.0f;
    float *s = malloc(sizeof(float) * (size_t)t.trail_len);
    s[0] = 0.0f;
    for (int i = 1; i < t.trail_len; i++) {
        len += v3_dist_xz(t.trail[i - 1], t.trail[i]);
        s[i] = len;
        CHECK(v3_dist_xz(t.trail[i - 1], t.trail[i]) < 3.1f);
    }
    /* winding: much longer than the straight line */
    CHECK(len > 1.5f * v3_dist_xz(t.trail[0], t.summit));
    /* gentle: no stretch of 20 m climbs or drops faster than the limit */
    float worst = 0.0f;
    for (int i = 0, j = 0; i < t.trail_len; i++) {
        while (j < t.trail_len - 1 && s[j] - s[i] < 20.0f) j++;
        if (s[j] - s[i] < 19.0f) break;
        float g = fabsf(t.trail[j].y - t.trail[i].y) / (s[j] - s[i]);
        if (g > worst) worst = g;
    }
    CHECK(worst <= MTN_TRAIL_MAX_GRADE + 0.01f);
    free(s);
    /* level across: the ground never rises above the path strip, which is walkable */
    int biomes = 0, above = 0;
    for (int i = 0; i + 1 < t.trail_len; i++) {
        vec3 a = t.trail[i], b = t.trail[i + 1];
        vec3 d = v3_norm(v3(b.x - a.x, 0, b.z - a.z));
        for (int k = -2; k <= 2; k++) {
            float x = a.x - d.z * 0.55f * (float)k, z = a.z + d.x * 0.55f * (float)k;
            if (mtn_terrain_height(&t, x, z) > a.y + 0.05f) above++;
        }
        float y;
        CHECK(mtn_terrain_trail_surface(&t, a.x, a.z, a.y, &y));
        CHECK_NEAR(y, a.y, 0.3f);
        biomes |= 1 << mtn_terrain_biome(&t, a.x, a.z);
    }
    CHECK(above == 0);
    int kinds = 0;
    for (int b = 0; b < MTN_BIOME_COUNT; b++) kinds += (biomes >> b) & 1;
    CHECK(kinds >= 3); /* through forest and meadows up to the rocks */
    /* off the path there is no path surface */
    float y;
    vec3 a = t.trail[t.trail_len / 2], b = t.trail[t.trail_len / 2 + 1];
    vec3 d = v3_norm(v3(b.x - a.x, 0, b.z - a.z));
    vec3 c;
    CHECK(mtn_terrain_trail_nearest(&t, a.x - d.z * 2.0f, a.z + d.x * 2.0f, &c));
    CHECK(v3_dist_xz(c, a) < 2.5f);
    CHECK(!mtn_terrain_trail_surface(&t, a.x - d.z * 2.0f, a.z + d.x * 2.0f, a.y, &y) ||
          v3_dist_xz(c, v3(a.x - d.z * 2.0f, 0, a.z + d.x * 2.0f)) <= MTN_TRAIL_WALK_HALF);
    /* a little beyond the visible edge is still path, a little more is not */
    CHECK(mtn_terrain_trail_surface(&t, a.x - d.z * 1.35f, a.z + d.x * 1.35f, a.y, &y) || 0);
    CHECK(!mtn_terrain_trail_surface(&t, -1000.0f, -1000.0f, 0.0f, &y));
    CHECK(!mtn_terrain_trail_nearest(&t, -1000.0f, -1000.0f, &c));
    /* markers: painted posts beside the path, a signpost and a cairn at the trailhead */
    CHECK(t.post_count > 10);
    for (int i = 0; i < t.post_count; i++)
        CHECK(mtn_terrain_trail_nearest(&t, t.posts[i].x, t.posts[i].z, &c) && v3_dist_xz(c, t.posts[i]) < 2.5f);
    CHECK(v3_dist_xz(t.signpost, t.trail[0]) < 3.0f);
    CHECK(v3_dist_xz(t.cairns[0], t.trail[0]) < 3.0f);
    CHECK(t.cairn_count >= t.trail_len / 40);
    mtn_terrain_free(&t);
}

/* Every point of the path, on any world, stands on the path surface. */
static void test_trail_surface_everywhere(void) {
    for (uint32_t seed = 1; seed <= 4; seed++) {
        mtn_terrain t;
        mtn_terrain_params p;
        mtn_terrain_default_params(&p);
        p.seed = seed * 1013u;
        p.size = 257;
        p.cell = 12.0f;
        p.erosion_drops = 8000;
        mtn_terrain_generate(&t, &p);
        int missed = 0;
        for (int i = 0; i + 1 < t.trail_len; i++) {
            vec3 m = v3_lerp(t.trail[i], t.trail[i + 1], 0.5f);
            float y = -1e9f;
            if (!mtn_terrain_trail_surface(&t, m.x, m.z, m.y, &y) || fabsf(y - m.y) > 0.05f) missed++;
        }
        CHECK(missed == 0);
        mtn_terrain_free(&t);
    }
}

static void test_lakes(void) {
    /* A bowl must become a lake; with a sea level everything low is water. */
    mtn_terrain t;
    memset(&t, 0, sizeof t);
    mtn_terrain_default_params(&t.p);
    t.n = 33;
    t.cell = 10;
    t.p.lake_min_cells = 4;
    t.p.lake_min_depth = 1.0f;
    t.h = malloc(sizeof(float) * 33 * 33);
    t.water = malloc(sizeof(float) * 33 * 33);
    for (int z = 0; z < 33; z++)
        for (int x = 0; x < 33; x++) {
            float dx = (float)x - 16, dz = (float)z - 16;
            float d = sqrtf(dx * dx + dz * dz);
            t.h[z * 33 + x] = d < 8 ? 10.0f + d : 20.0f + (d - 8) * 0.1f; /* rim at 20 */
        }
    t.h[16] = 0.0f; /* ensure an outlet on the border does not drain the bowl */
    CHECK(mtn_terrain_fill_lakes(&t) == 1);
    CHECK(t.water[16 * 33 + 16] > 15.0f);
    CHECK(mtn_terrain_water(&t, 160, 160) > 15.0f);
    CHECK(mtn_terrain_water(&t, 5, 5) == MTN_NO_WATER);
    t.p.sea_level = 15.0f;
    t.p.lake_min_depth = 100.0f;
    mtn_terrain_fill_lakes(&t);
    CHECK_NEAR(t.water[16 * 33 + 16], 15.0f, 1e-4);
    free(t.h);
    free(t.water);
}

static void test_erosion(void) {
    int n = 64;
    float *h = malloc(sizeof(float) * (size_t)n * (size_t)n);
    for (int z = 0; z < n; z++)
        for (int x = 0; x < n; x++) h[z * n + x] = (float)(x + z) * 4.0f + (float)((x * 7 + z * 3) % 5);
    float before = 0, after = 0;
    for (int i = 0; i < n * n; i++) before += h[i];
    mtn_terrain_erode(h, n, 4.0f, 3000, 5);
    for (int i = 0; i < n * n; i++) after += h[i];
    CHECK(after <= before + 1.0f); /* erosion never creates material */
    CHECK(after < before);
    float copy0 = h[0];
    mtn_terrain_erode(h, n, 4.0f, 0, 5); /* no-op */
    CHECK(h[0] == copy0);
    /* flat terrain: drops stop immediately */
    for (int i = 0; i < n * n; i++) h[i] = 1.0f;
    mtn_terrain_erode(h, n, 4.0f, 100, 5);
    CHECK_NEAR(h[n * 10 + 10], 1.0f, 1e-6);
    free(h);
}

MTN_SUITE(terrain) {
    TEST_RUN(test_generate_deterministic);
    TEST_RUN(test_queries);
    TEST_RUN(test_trail_and_viewpoints);
    TEST_RUN(test_trail_shape);
    TEST_RUN(test_trail_surface_everywhere);
    TEST_RUN(test_lakes);
    TEST_RUN(test_erosion);
}
