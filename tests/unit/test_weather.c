#include "test.h"
#include "mtn/weather.h"

static mtn_season_blend pure(mtn_season s) {
    mtn_season_blend b;
    memset(&b, 0, sizeof b);
    b.w[s] = 1.0f;
    b.dominant = s;
    return b;
}

/* Completes the current transition without real time passing (no lightning). */
/* weather blends in real time: tests advance game and real time together unless they test the difference */
static void settle(mtn_weather *w, mtn_season_blend s) { mtn_weather_update(w, w->transition_s + 1.0f, w->transition_s + 1.0f, s, 12.0f); }

static void test_names(void) {
    const char *expect[MTN_WX_COUNT] = {"clear", "cloudy", "overcast", "fog", "rain", "storm", "snow"};
    for (int k = 0; k < MTN_WX_COUNT; k++) {
        CHECK_STR(mtn_weather_name((mtn_weather_kind)k), expect[k]);
        CHECK(mtn_weather_from_name(expect[k]) == k);
    }
    CHECK_STR(mtn_weather_name(MTN_WX_COUNT), "unknown");
    CHECK_STR(mtn_weather_name((mtn_weather_kind)-1), "unknown");
    CHECK(mtn_weather_from_name("hail") == -1);
    CHECK(mtn_weather_from_name("") == -1);
}

static void test_init(void) {
    mtn_weather w;
    mtn_weather_init(&w, 42);
    CHECK(w.current == MTN_WX_CLEAR);
    CHECK(w.target == MTN_WX_CLEAR);
    CHECK(!w.locked);
    CHECK_NEAR(w.blend, 0.0f, 0);
    CHECK(w.time_left >= w.min_duration && w.time_left <= w.max_duration);
    CHECK_NEAR(w.cloud_cover, w.look[MTN_WX_CLEAR].cloud_cover, 0);
    CHECK_NEAR(w.wind, w.look[MTN_WX_CLEAR].wind, 0);
    CHECK_NEAR(w.snow_ground, 0.0f, 0);
    CHECK(w.event_count == 0);
    CHECK(mtn_weather_pop_event(&w) == MTN_WX_EVENT_NONE);
    /* storms are the only lightning state, snow never happens in summer */
    for (int k = 0; k < MTN_WX_COUNT; k++) CHECK((w.look[k].lightning > 0.0f) == (k == MTN_WX_STORM));
    CHECK_NEAR(w.weights[MTN_SUMMER][MTN_WX_SNOW], 0.0f, 0);
    /* same seed, same weather */
    mtn_weather w2;
    mtn_weather_init(&w2, 42);
    CHECK_NEAR(w.time_left, w2.time_left, 0);
}

static void test_temperature(void) {
    mtn_season_blend su = pure(MTN_SUMMER), wi = pure(MTN_WINTER), sp = pure(MTN_SPRING);
    CHECK(mtn_temperature(su, 15.0f, 0.0f) > mtn_temperature(wi, 15.0f, 0.0f) + 20.0f);
    /* warmest mid afternoon, coldest before dawn */
    CHECK_NEAR(mtn_temperature(su, 15.0f, 0.0f), 23.0f, 1e-4);
    CHECK_NEAR(mtn_temperature(su, 3.0f, 0.0f), 15.0f, 1e-4);
    /* 16 C colder at the summit */
    CHECK_NEAR(mtn_temperature(sp, 9.0f, 0.0f) - mtn_temperature(sp, 9.0f, 1.0f), 16.0f, 1e-4);
    CHECK(mtn_temperature(wi, 12.0f, 0.35f) < 1.0f);
    /* blends interpolate */
    mtn_season_blend half = pure(MTN_SUMMER);
    half.w[MTN_SUMMER] = 0.5f;
    half.w[MTN_WINTER] = 0.5f;
    CHECK_NEAR(mtn_temperature(half, 15.0f, 0.0f), 0.5f * (19.0f - 5.0f) + 4.0f, 1e-4);
}

static void test_never_repeats(void) {
    mtn_weather w;
    mtn_weather_init(&w, 7);
    int seen[MTN_WX_COUNT] = {0};
    int changes = 0, repeats = 0;
    mtn_season_blend su = pure(MTN_SUMMER);
    for (int i = 0; i < 3000; i++) {
        mtn_weather_kind cur = w.current, tgt = w.target;
        mtn_weather_update(&w, 1800.0f, 1800.0f, su, 12.0f);
        if (cur == tgt && w.target != tgt) {
            changes++;
            if (w.target == cur) repeats++;
            seen[w.target]++;
        }
        CHECK(w.target >= 0 && w.target < MTN_WX_COUNT);
    }
    CHECK(changes > 100);
    CHECK(repeats == 0);
    CHECK(seen[MTN_WX_SNOW] == 0); /* no snow in summer */
    CHECK(seen[MTN_WX_CLEAR] > 0);
    CHECK(seen[MTN_WX_STORM] > 0);
    CHECK(seen[MTN_WX_CLEAR] > seen[MTN_WX_FOG]); /* clear is 10x likelier than fog in summer */

    /* in winter there is snow and no storms */
    memset(seen, 0, sizeof seen);
    mtn_season_blend wi = pure(MTN_WINTER);
    for (int i = 0; i < 3000; i++) {
        mtn_weather_kind cur = w.current, tgt = w.target;
        mtn_weather_update(&w, 1800.0f, 1800.0f, wi, 12.0f);
        if (cur == tgt && w.target != tgt) seen[w.target]++;
    }
    CHECK(seen[MTN_WX_SNOW] > 0);
    CHECK(seen[MTN_WX_STORM] == 0);

    /* time only runs out while unlocked */
    mtn_weather_init(&w, 3);
    float left = w.time_left;
    mtn_weather_update(&w, 60.0f, 60.0f, su, 12.0f);
    CHECK_NEAR(w.time_left, left - 60.0f, 1e-2);
}

static void test_choose_fallbacks(void) {
    mtn_weather w;
    mtn_season_blend sp = pure(MTN_SPRING);

    /* no weights at all: back to clear */
    mtn_weather_init(&w, 1);
    mtn_weather_set(&w, MTN_WX_RAIN, 0);
    settle(&w, sp);
    CHECK(w.current == MTN_WX_RAIN);
    memset(w.weights, 0, sizeof w.weights);
    w.time_left = 0.0f;
    mtn_weather_update(&w, 1.0f, 1.0f, sp, 12.0f);
    CHECK(w.target == MTN_WX_CLEAR);
    CHECK(w.time_left >= w.min_duration);

    /* only the current state is allowed: it must still change */
    mtn_weather_init(&w, 1);
    memset(w.weights, 0, sizeof w.weights);
    w.weights[MTN_SPRING][MTN_WX_CLEAR] = 1.0f;
    w.time_left = 0.0f;
    mtn_weather_update(&w, 1.0f, 1.0f, sp, 12.0f);
    CHECK(w.target == MTN_WX_CLOUDY);
    settle(&w, sp);
    CHECK(w.current == MTN_WX_CLOUDY);
    memset(w.weights, 0, sizeof w.weights);
    w.weights[MTN_SPRING][MTN_WX_CLOUDY] = 1.0f;
    w.time_left = 0.0f;
    mtn_weather_update(&w, 1.0f, 1.0f, sp, 12.0f);
    CHECK(w.target == MTN_WX_CLEAR);

    /* exactly one other state: always picked */
    mtn_weather_init(&w, 9);
    memset(w.weights, 0, sizeof w.weights);
    w.weights[MTN_SPRING][MTN_WX_CLEAR] = 5.0f;
    w.weights[MTN_SPRING][MTN_WX_FOG] = 1.0f;
    w.time_left = 0.0f;
    mtn_weather_update(&w, 1.0f, 1.0f, sp, 12.0f);
    CHECK(w.target == MTN_WX_FOG);

    /* corrupt (NaN) weights must not produce an invalid state */
    mtn_weather_init(&w, 1);
    for (int k = 0; k < MTN_WX_COUNT; k++) w.weights[MTN_SPRING][k] = NAN;
    w.time_left = 0.0f;
    mtn_weather_update(&w, 1.0f, 1.0f, sp, 12.0f);
    CHECK(w.target == MTN_WX_CLEAR);
}

static void test_set_and_blend(void) {
    mtn_weather w;
    mtn_season_blend sp = pure(MTN_SPRING);
    mtn_weather_init(&w, 5);

    /* invalid kinds are ignored */
    mtn_weather_set(&w, (mtn_weather_kind)-1, 1);
    mtn_weather_set(&w, MTN_WX_COUNT, 1);
    CHECK(w.target == MTN_WX_CLEAR);
    CHECK(!w.locked);

    mtn_weather_set(&w, MTN_WX_OVERCAST, 1);
    CHECK(w.target == MTN_WX_OVERCAST);
    CHECK(w.current == MTN_WX_CLEAR);
    CHECK(w.locked);
    /* halfway through the transition the look is in between */
    mtn_weather_update(&w, w.transition_s * 0.5f, w.transition_s * 0.5f, sp, 12.0f);
    CHECK_NEAR(w.blend, 0.5f, 1e-4);
    CHECK_NEAR(w.cloud_cover, 0.5f * (w.look[MTN_WX_CLEAR].cloud_cover + w.look[MTN_WX_OVERCAST].cloud_cover), 1e-4);
    CHECK(w.darkness > 0.0f && w.darkness < w.look[MTN_WX_OVERCAST].darkness);
    mtn_weather_update(&w, w.transition_s * 0.6f, w.transition_s * 0.6f, sp, 12.0f);
    CHECK(w.current == MTN_WX_OVERCAST);
    CHECK_NEAR(w.blend, 0.0f, 0);
    CHECK_NEAR(w.cloud_cover, w.look[MTN_WX_OVERCAST].cloud_cover, 1e-6);
    CHECK_NEAR(w.fog, w.look[MTN_WX_OVERCAST].fog, 1e-6);

    /* locked weather stays forever */
    float left = w.time_left;
    for (int i = 0; i < 200; i++) mtn_weather_update(&w, 3600.0f, 3600.0f, sp, 12.0f);
    CHECK(w.current == MTN_WX_OVERCAST && w.target == MTN_WX_OVERCAST);
    CHECK_NEAR(w.time_left, left, 0);

    /* re-setting the same target only changes the lock */
    mtn_weather_set(&w, MTN_WX_OVERCAST, 0);
    CHECK(!w.locked);
    CHECK(w.current == MTN_WX_OVERCAST);

    /* changing the target past the midpoint commits the old target first */
    mtn_weather_set(&w, MTN_WX_RAIN, 1);
    mtn_weather_update(&w, w.transition_s * 0.7f, w.transition_s * 0.7f, sp, 12.0f);
    CHECK(w.blend > 0.5f);
    mtn_weather_set(&w, MTN_WX_FOG, 1);
    CHECK(w.current == MTN_WX_RAIN);
    CHECK(w.target == MTN_WX_FOG);
    CHECK_NEAR(w.blend, 0.0f, 0);
    /* ... but before the midpoint the old current state is kept */
    mtn_weather_update(&w, w.transition_s * 0.2f, w.transition_s * 0.2f, sp, 12.0f);
    mtn_weather_set(&w, MTN_WX_CLEAR, 1);
    CHECK(w.current == MTN_WX_RAIN);
    CHECK(w.target == MTN_WX_CLEAR);

    mtn_weather_unlock(&w);
    CHECK(!w.locked);
    CHECK(w.time_left >= w.min_duration && w.time_left <= w.max_duration);

    /* the wind veers but stays a unit vector */
    for (int i = 0; i < 100; i++) mtn_weather_update(&w, 10.0f, 1.0f, sp, 12.0f);
    CHECK_NEAR(sqrtf(w.wind_dir.x * w.wind_dir.x + w.wind_dir.y * w.wind_dir.y), 1.0f, 1e-4);
    CHECK(w.wind_dir.y != 0.0f);
}

static void test_precip_type(void) {
    mtn_weather w;
    mtn_weather_init(&w, 2);
    mtn_weather_set(&w, MTN_WX_RAIN, 1);
    settle(&w, pure(MTN_SUMMER));
    mtn_weather_update(&w, 1.0f, 1.0f, pure(MTN_SUMMER), 14.0f);
    CHECK(w.temperature > 10.0f);
    CHECK(!w.precip_snow);
    CHECK_NEAR(w.precipitation, 0.7f, 1e-6);
    /* the same rain falls as snow on a winter night */
    mtn_weather_update(&w, 1.0f, 1.0f, pure(MTN_WINTER), 3.0f);
    CHECK(w.temperature < 1.0f);
    CHECK(w.precip_snow);
    /* snow weather is snow even when it is warm */
    mtn_weather_set(&w, MTN_WX_SNOW, 1);
    mtn_weather_update(&w, 1.0f, 1.0f, pure(MTN_SUMMER), 15.0f);
    CHECK(w.temperature > 1.0f);
    CHECK(!w.precip_snow); /* still mostly rain */
    mtn_weather_update(&w, w.transition_s * 0.6f, w.transition_s * 0.6f, pure(MTN_SUMMER), 15.0f);
    CHECK(w.precip_snow); /* past the midpoint towards snow */
    settle(&w, pure(MTN_SUMMER));
    CHECK(w.current == MTN_WX_SNOW);
    CHECK(w.precip_snow);
}

static void test_lightning(void) {
    mtn_weather w;
    mtn_season_blend su = pure(MTN_SUMMER);
    mtn_weather_init(&w, 11);

    /* no lightning in clear weather */
    for (int i = 0; i < 1000; i++) mtn_weather_update(&w, 1.0f, 0.1f, su, 12.0f);
    CHECK(w.event_count == 0);
    CHECK_NEAR(w.flash, 0.0f, 0);

    mtn_weather_set(&w, MTN_WX_STORM, 1);
    settle(&w, su);
    CHECK_NEAR(w.lightning_rate, 1.0f, 1e-6);
    /* the blend took real time, so strikes may already have happened: restart the clock */
    while (mtn_weather_pop_event(&w) != MTN_WX_EVENT_NONE) {}
    w.lightning_timer = 5.0f;
    w.thunder_delay = 0.0f;
    w.flash = 0.0f;
    int got_lightning = 0, got_thunder = 0;
    float t = 0.0f, t_flash = -1.0f;
    for (int i = 0; i < 2000 && !got_thunder; i++) {
        mtn_weather_update(&w, 1.0f, 0.05f, su, 12.0f);
        t += 0.05f;
        mtn_weather_event ev;
        while ((ev = mtn_weather_pop_event(&w)) != MTN_WX_EVENT_NONE) {
            if (ev == MTN_WX_EVENT_LIGHTNING) {
                CHECK(!got_lightning);
                CHECK_NEAR(w.flash, 1.0f, 0);
                got_lightning = 1;
                t_flash = t;
            } else if (ev == MTN_WX_EVENT_THUNDER) {
                CHECK(got_lightning); /* thunder follows the flash */
                got_thunder = 1;
                CHECK(t - t_flash >= 0.75f && t - t_flash <= 4.1f);
            }
        }
    }
    CHECK(got_lightning);
    CHECK(got_thunder);
    CHECK(t_flash > 4.9f && t_flash < 5.1f); /* first strike after the initial 5 s timer */
    /* the flash has faded by the time the thunder arrives (and the next strike is >= 10 s away) */
    CHECK_NEAR(w.flash, 0.0f, 0);
    CHECK(w.lightning_timer > 5.0f);

    /* unread events are capped */
    while (mtn_weather_pop_event(&w) != MTN_WX_EVENT_NONE) {}
    for (int i = 0; i < 2000; i++) mtn_weather_update(&w, 1.0f, 1.0f, su, 12.0f);
    CHECK(w.event_count == 8);
    int n = 0;
    while (mtn_weather_pop_event(&w) != MTN_WX_EVENT_NONE) n++;
    CHECK(n == 8);
    CHECK(w.event_count == 0);
}

static void test_snow_ground(void) {
    mtn_weather w;
    mtn_season_blend wi = pure(MTN_WINTER), su = pure(MTN_SUMMER);
    mtn_weather_init(&w, 4);
    mtn_weather_set(&w, MTN_WX_SNOW, 1);
    settle(&w, wi);
    float s0 = w.snow_ground;
    CHECK(s0 > 0.0f);
    /* one hour of snow: 0.5 * min(1, 0.6 * 1.5) */
    mtn_weather_update(&w, 3600.0f, 3600.0f, wi, 12.0f);
    CHECK_NEAR(w.snow_ground - s0, 0.45f, 1e-4);
    for (int i = 0; i < 10; i++) mtn_weather_update(&w, 3600.0f, 3600.0f, wi, 12.0f);
    CHECK_NEAR(w.snow_ground, 1.0f, 0);

    /* melts slowly in winter */
    mtn_weather_set(&w, MTN_WX_CLEAR, 1);
    settle(&w, wi);
    float s1 = w.snow_ground;
    CHECK(s1 < 1.0f);
    mtn_weather_update(&w, 6.0f * 3600.0f, 6.0f * 3600.0f, wi, 12.0f);
    CHECK_NEAR(s1 - w.snow_ground, 0.25f, 1e-4);
    /* ... and quickly otherwise */
    float s2 = w.snow_ground;
    mtn_weather_update(&w, 3600.0f, 3600.0f, su, 12.0f);
    CHECK_NEAR(s2 - w.snow_ground, 1.0f / 6.0f, 1e-4);
    mtn_weather_update(&w, 24.0f * 3600.0f, 24.0f * 3600.0f, su, 12.0f);
    CHECK_NEAR(w.snow_ground, 0.0f, 0);

    /* cold rain also builds snow */
    mtn_weather_set(&w, MTN_WX_RAIN, 1);
    settle(&w, wi);
    mtn_weather_update(&w, 3600.0f, 3600.0f, wi, 3.0f);
    CHECK(w.precip_snow);
    CHECK(w.snow_ground > 0.4f);
}

MTN_SUITE(weather) {
    TEST_RUN(test_names);
    TEST_RUN(test_init);
    TEST_RUN(test_temperature);
    TEST_RUN(test_never_repeats);
    TEST_RUN(test_choose_fallbacks);
    TEST_RUN(test_set_and_blend);
    TEST_RUN(test_precip_type);
    TEST_RUN(test_lightning);
    TEST_RUN(test_snow_ground);
}
