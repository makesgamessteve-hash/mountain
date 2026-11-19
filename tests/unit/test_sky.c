#include "test.h"
#include "mtn/sky.h"
#include "mtn/weather.h"

static void test_planet_and_clock(void) {
    mtn_planet p;
    mtn_planet_earth(&p);
    CHECK_STR(p.name, "Earth");
    CHECK(p.moon_count == 1);
    CHECK(p.days_per_season == 7);
    CHECK_NEAR(p.latitude_deg, 46.0f, 1e-6);

    mtn_clock c;
    mtn_clock_init(&c, 3.0f, 6.0f);
    CHECK(mtn_clock_day(&c) == 3);
    CHECK_NEAR(mtn_clock_hour(&c), 6.0f, 1e-4);
    CHECK_NEAR(c.speed, 1.0f, 0);
    CHECK(!c.paused);

    /* 1440 real seconds per game day: one real minute is one game hour */
    mtn_clock_update(&c, &p, 60.0f);
    CHECK_NEAR(mtn_clock_hour(&c), 7.0f, 1e-4);
    CHECK_NEAR(mtn_clock_game_seconds(&c, &p, 1.0f), 60.0f, 1e-4);
    c.speed = 2.0f;
    CHECK_NEAR(mtn_clock_game_seconds(&c, &p, 1.0f), 120.0f, 1e-4);
    mtn_clock_update(&c, &p, 60.0f);
    CHECK_NEAR(mtn_clock_hour(&c), 9.0f, 1e-4);

    /* a full day rolls the day counter over */
    c.speed = 1.0f;
    mtn_clock_update(&c, &p, 1440.0f);
    CHECK(mtn_clock_day(&c) == 4);
    CHECK_NEAR(mtn_clock_hour(&c), 9.0f, 1e-3);

    /* paused clocks and broken planets do not advance */
    double before = c.days;
    c.paused = 1;
    mtn_clock_update(&c, &p, 600.0f);
    CHECK(c.days == before);
    CHECK_NEAR(mtn_clock_game_seconds(&c, &p, 1.0f), 0.0f, 0);
    c.paused = 0;
    mtn_planet broken = p;
    broken.day_length_s = 0.0f;
    mtn_clock_update(&c, &broken, 600.0f);
    CHECK(c.days == before);
    CHECK_NEAR(mtn_clock_game_seconds(&c, &broken, 1.0f), 0.0f, 0);

    /* set_hour wraps both ways and keeps the day */
    mtn_clock_set_hour(&c, 25.0f);
    CHECK(mtn_clock_day(&c) == 4);
    CHECK_NEAR(mtn_clock_hour(&c), 1.0f, 1e-3);
    mtn_clock_set_hour(&c, -1.0f);
    CHECK(mtn_clock_day(&c) == 4);
    CHECK_NEAR(mtn_clock_hour(&c), 23.0f, 1e-3);
    mtn_clock_set_hour(&c, 12.0f);
    CHECK_NEAR(mtn_clock_hour(&c), 12.0f, 1e-3);
}

static void test_year_and_seasons(void) {
    mtn_planet p;
    mtn_planet_earth(&p);
    mtn_clock c;
    /* the year has 4 * 7 = 28 days */
    mtn_clock_init(&c, 14.0f, 0.0f);
    CHECK_NEAR(mtn_clock_year_frac(&c, &p), 0.5f, 1e-5);
    mtn_clock_init(&c, 28.0f + 7.0f, 0.0f);
    CHECK_NEAR(mtn_clock_year_frac(&c, &p), 0.25f, 1e-5);
    /* a planet without seasons still has a (4 day) year */
    mtn_planet odd = p;
    odd.days_per_season = 0;
    mtn_clock_init(&c, 1.0f, 0.0f);
    CHECK_NEAR(mtn_clock_year_frac(&c, &odd), 0.25f, 1e-5);

    /* set_season lands in the middle of the season, same year, same hour */
    mtn_clock_init(&c, 30.0f, 15.0f);
    for (int s = 0; s < MTN_SEASON_COUNT; s++) {
        mtn_clock_set_season(&c, &p, (mtn_season)s);
        CHECK(mtn_clock_day(&c) == 28 + s * 7 + 3);
        CHECK_NEAR(mtn_clock_hour(&c), 15.0f, 1e-3);
        mtn_season_blend b = mtn_season_blend_at(mtn_clock_year_frac(&c, &p));
        CHECK(b.dominant == s);
        CHECK(b.w[s] > 0.85f);
    }

    /* blends always sum to 1, wrap around, and are pure at season centres */
    for (int i = -20; i <= 60; i++) {
        float f = (float)i / 40.0f;
        mtn_season_blend b = mtn_season_blend_at(f);
        float sum = 0.0f;
        for (int k = 0; k < MTN_SEASON_COUNT; k++) {
            CHECK(b.w[k] >= 0.0f && b.w[k] <= 1.0f);
            sum += b.w[k];
        }
        CHECK_NEAR(sum, 1.0f, 1e-5);
        CHECK(b.w[b.dominant] >= 0.25f);
    }
    mtn_season_blend summer = mtn_season_blend_at(0.375f);
    CHECK(summer.dominant == MTN_SUMMER);
    CHECK_NEAR(summer.w[MTN_SUMMER], 1.0f, 1e-6);
    CHECK_NEAR(summer.w[MTN_WINTER], 0.0f, 1e-6);
    mtn_season_blend wrapped = mtn_season_blend_at(1.375f);
    CHECK_NEAR(wrapped.w[MTN_SUMMER], 1.0f, 1e-6);
    mtn_season_blend winter = mtn_season_blend_at(-0.125f);
    CHECK(winter.dominant == MTN_WINTER);
    CHECK_NEAR(winter.w[MTN_WINTER], 1.0f, 1e-6);
    /* the year boundary sits exactly between winter and spring */
    mtn_season_blend edge = mtn_season_blend_at(0.0f);
    CHECK_NEAR(edge.w[MTN_SPRING], 0.5f, 1e-5);
    CHECK_NEAR(edge.w[MTN_WINTER], 0.5f, 1e-5);
    CHECK_NEAR(edge.w[MTN_SUMMER], 0.0f, 1e-6);

    CHECK_STR(mtn_season_name(MTN_SPRING), "spring");
    CHECK_STR(mtn_season_name(MTN_SUMMER), "summer");
    CHECK_STR(mtn_season_name(MTN_AUTUMN), "autumn");
    CHECK_STR(mtn_season_name(MTN_WINTER), "winter");
    CHECK_STR(mtn_season_name(MTN_SEASON_COUNT), "unknown");
    CHECK_STR(mtn_season_name((mtn_season)-1), "unknown");
}

static void test_sun(void) {
    mtn_planet p;
    mtn_planet_earth(&p);
    const float summer = 0.375f, winter = 0.875f; /* solstices in the middle of summer and winter */
    vec3 noon = mtn_sun_direction(&p, summer, 12.0f);
    vec3 midnight = mtn_sun_direction(&p, summer, 0.0f);
    CHECK_NEAR(v3_len(noon), 1.0f, 1e-5);
    CHECK_NEAR(v3_len(midnight), 1.0f, 1e-5);
    CHECK(noon.y > 0.5f);
    CHECK(midnight.y < 0.0f);
    /* at noon the sun stands due south (+z), at latitude 46 elevation = 90 - 46 + 23.44 */
    CHECK_NEAR(noon.x, 0.0f, 1e-5);
    CHECK(noon.z > 0.0f);
    CHECK_NEAR(asinf(noon.y) / MTN_DEG2RAD, 90.0f - 46.0f + 23.44f, 0.05);
    /* rises in the east (+x) and sets in the west */
    CHECK(mtn_sun_direction(&p, summer, 7.0f).x > 0.0f);
    CHECK(mtn_sun_direction(&p, summer, 17.0f).x < 0.0f);
    /* the winter sun stays low */
    vec3 wnoon = mtn_sun_direction(&p, winter, 12.0f);
    CHECK(wnoon.y > 0.0f);
    CHECK(wnoon.y < noon.y - 0.3f);
    CHECK_NEAR(asinf(wnoon.y) / MTN_DEG2RAD, 90.0f - 46.0f - 23.44f, 0.05);
    /* the sun climbs all morning */
    float prev = -2.0f;
    for (int h = 0; h <= 12; h++) {
        float y = mtn_sun_direction(&p, summer, (float)h).y;
        CHECK(y > prev);
        prev = y;
    }

    /* summer days are longer than winter days, equinox is 12 hours */
    float ls = mtn_day_length_hours(&p, summer), lw = mtn_day_length_hours(&p, winter);
    CHECK(ls > 15.0f && ls < 16.5f);
    CHECK(lw > 7.5f && lw < 9.0f);
    CHECK_NEAR(ls + lw, 24.0f, 1e-3);
    CHECK_NEAR(mtn_day_length_hours(&p, 0.125f), 12.0f, 1e-3); /* spring equinox */
    /* the day length matches the sunrise: sun above the horizon half a day length around noon */
    CHECK(mtn_sun_direction(&p, summer, 12.0f - ls * 0.5f + 0.1f).y > 0.0f);
    CHECK(mtn_sun_direction(&p, summer, 12.0f - ls * 0.5f - 0.1f).y < 0.0f);
    /* polar day and polar night */
    mtn_planet polar = p;
    polar.latitude_deg = 80.0f;
    CHECK_NEAR(mtn_day_length_hours(&polar, summer), 24.0f, 1e-3);
    CHECK_NEAR(mtn_day_length_hours(&polar, winter), 0.0f, 1e-3);
    CHECK(mtn_sun_direction(&polar, summer, 0.0f).y > 0.0f);
}

static void test_moons(void) {
    mtn_planet p;
    mtn_planet_earth(&p);
    /* phase0 = 0.45, period 8 days */
    CHECK_NEAR(mtn_moon_phase(&p, 0, 0.0), 0.45f, 1e-6);
    CHECK_NEAR(mtn_moon_phase(&p, 0, 0.4), 0.5f, 1e-6);
    CHECK_NEAR(mtn_moon_phase(&p, 0, 2.4), 0.75f, 1e-5);
    CHECK_NEAR(mtn_moon_phase(&p, 0, 4.6), 0.025f, 1e-5); /* wrapped past new moon */
    CHECK_NEAR(mtn_moon_phase(&p, 0, 8.0), 0.45f, 1e-5);
    for (int d = 0; d < 40; d++) {
        float ph = mtn_moon_phase(&p, 0, (double)d * 0.37);
        CHECK(ph >= 0.0f && ph < 1.0f);
    }
    /* invalid moons */
    CHECK_NEAR(mtn_moon_phase(&p, 1, 3.0), 0.0f, 0);
    CHECK_NEAR(mtn_moon_phase(&p, -1, 3.0), 0.0f, 0);
    mtn_planet still = p;
    still.moons[0].period_days = 0.0f;
    CHECK_NEAR(mtn_moon_phase(&still, 0, 3.0), 0.0f, 0);

    /* a full moon is opposite the sun: highest around midnight */
    mtn_planet full = p;
    full.moons[0].phase0 = 0.5f;
    vec3 m0 = mtn_moon_direction(&full, 0, 0.0, 0.0f, 0.0f);
    vec3 sun0 = mtn_sun_direction(&full, 0.0f, 0.0f);
    CHECK_NEAR(v3_len(m0), 1.0f, 1e-5);
    CHECK(m0.y > 0.5f);
    CHECK(sun0.y < -0.5f);
    CHECK(v3_dot(m0, sun0) < -0.9f);
    /* a new moon travels with the sun */
    mtn_planet newm = p;
    newm.moons[0].phase0 = 0.0f;
    vec3 n0 = mtn_moon_direction(&newm, 0, 0.0, 0.0f, 12.0f);
    vec3 sun12 = mtn_sun_direction(&newm, 0.0f, 12.0f);
    CHECK(v3_dot(n0, sun12) > 0.99f);
    /* an invalid moon index still gives a unit vector (no inclination, phase 0) */
    vec3 inv = mtn_moon_direction(&p, 7, 2.0, 0.3f, 22.0f);
    CHECK_NEAR(v3_len(inv), 1.0f, 1e-5);
    vec3 inv2 = mtn_moon_direction(&p, -1, 2.0, 0.3f, 22.0f);
    CHECK_NEAR(v3_len(inv2), 1.0f, 1e-5);
}

static float lum(vec3 c) { return c.x * 0.3f + c.y * 0.59f + c.z * 0.11f; }

static void test_sky_day_night(void) {
    mtn_planet p;
    mtn_planet_earth(&p);
    mtn_clock c;
    mtn_sky_state day, night, dusk;
    mtn_clock_init(&c, 0.0f, 0.0f);
    mtn_clock_set_season(&c, &p, MTN_SUMMER);
    mtn_clock_set_hour(&c, 12.0f);
    mtn_sky_compute(&day, &p, &c, NULL);
    CHECK_NEAR(day.daylight, 1.0f, 1e-5);
    CHECK_NEAR(day.sunset, 0.0f, 1e-5);
    CHECK_NEAR(day.sun_visibility, 1.0f, 1e-5);
    CHECK_NEAR(day.star_visibility, 0.0f, 1e-5);
    CHECK_NEAR(day.cloud_cover, 0.0f, 0);
    CHECK_NEAR(day.light_dir.x, day.sun_dir.x, 1e-6);
    CHECK_NEAR(day.light_dir.y, day.sun_dir.y, 1e-6);
    CHECK(day.light_color.x > 0.9f);
    CHECK_NEAR(day.fog_density, 0.00028f, 1e-7);
    CHECK(day.zenith.z > day.zenith.x); /* blue sky */
    CHECK(day.star_angle >= 0.0f && day.star_angle < MTN_TAU);
    CHECK_NEAR(day.star_axis.y, sinf(46.0f * MTN_DEG2RAD), 1e-5);
    CHECK_NEAR(v3_len(day.star_axis), 1.0f, 1e-5);

    mtn_clock_set_hour(&c, 0.0f);
    mtn_sky_compute(&night, &p, &c, NULL);
    CHECK_NEAR(night.daylight, 0.0f, 1e-5);
    CHECK_NEAR(night.sun_visibility, 0.0f, 1e-5);
    CHECK_NEAR(night.star_visibility, 1.0f, 1e-5);
    CHECK(lum(night.zenith) < lum(day.zenith) * 0.2f);
    CHECK(lum(night.ambient) < lum(day.ambient));
    CHECK(lum(night.cloud_color) < lum(day.cloud_color));
    CHECK(night.sun_dir.y < 0.0f);

    /* golden hour: find sunrise and check that it is coloured */
    float rise = 0.0f;
    for (float h = 3.0f; h < 9.0f; h += 0.01f)
        if (mtn_sun_direction(&p, mtn_clock_year_frac(&c, &p), h).y > 0.03f) {
            rise = h;
            break;
        }
    CHECK(rise > 3.0f);
    mtn_clock_set_hour(&c, rise);
    mtn_sky_compute(&dusk, &p, &c, NULL);
    CHECK(dusk.sunset > 0.9f);
    CHECK(dusk.daylight > 0.3f && dusk.daylight < 0.8f);
    CHECK(dusk.horizon.x > dusk.horizon.z); /* orange horizon */
    CHECK(dusk.light_dir.y > 0.0f);

    /* sun just below the horizon but still lighting: light comes from a grazing angle */
    mtn_clock_set_hour(&c, rise - 0.12f);
    mtn_sky_compute(&dusk, &p, &c, NULL);
    CHECK(dusk.sun_dir.y <= 0.02f);
    CHECK(dusk.light_color.x > 0.0f);
    CHECK(dusk.light_dir.y > 0.0f);
    CHECK_NEAR(v3_len(dusk.light_dir), 1.0f, 1e-5);
}

static void test_sky_moonlight(void) {
    mtn_planet p;
    mtn_planet_earth(&p);
    mtn_clock c;
    mtn_sky_state s;

    /* full moon at midnight lights the scene */
    p.moons[0].phase0 = 0.5f;
    mtn_clock_init(&c, 0.0f, 0.0f);
    mtn_sky_compute(&s, &p, &c, NULL);
    CHECK_NEAR(s.moon_lit[0], 1.0f, 1e-5);
    CHECK(s.moon_dir[0].y > 0.5f);
    CHECK_NEAR(s.light_dir.x, s.moon_dir[0].x, 1e-6);
    CHECK_NEAR(s.light_dir.y, s.moon_dir[0].y, 1e-6);
    CHECK(s.light_color.z > s.light_color.x); /* bluish moonlight */
    CHECK(s.light_color.z > 0.2f);
    vec3 full_ambient = s.ambient;

    /* new moon: dark, light falls back to the (grazing) sun direction with no intensity */
    p.moons[0].phase0 = 0.0f;
    mtn_sky_compute(&s, &p, &c, NULL);
    CHECK_NEAR(s.moon_lit[0], 0.0f, 1e-5);
    CHECK_NEAR(s.light_color.x + s.light_color.y + s.light_color.z, 0.0f, 1e-5);
    CHECK_NEAR(s.light_dir.y, 0.02f / sqrtf(0.02f * 0.02f + s.sun_dir.x * s.sun_dir.x + s.sun_dir.z * s.sun_dir.z),
               1e-4);
    CHECK(lum(s.ambient) < lum(full_ambient));

    /* two moons: the brighter visible one wins */
    p.moon_count = 2;
    p.moons[1] = p.moons[0];
    p.moons[1].phase0 = 0.5f;
    p.moons[1].inclination = 0.0f;
    mtn_sky_compute(&s, &p, &c, NULL);
    CHECK_NEAR(s.moon_lit[1], 1.0f, 1e-5);
    CHECK_NEAR(s.light_dir.x, s.moon_dir[1].x, 1e-6);
    CHECK_NEAR(s.light_dir.y, s.moon_dir[1].y, 1e-6);
}

static void test_sky_weather(void) {
    mtn_planet p;
    mtn_planet_earth(&p);
    mtn_clock c;
    mtn_clock_init(&c, 10.0f, 12.0f);
    mtn_sky_state clear, storm, flash;
    mtn_weather w;
    mtn_weather_init(&w, 1);
    w.cloud_cover = 0.0f;
    w.darkness = 0.0f;
    w.flash = 0.0f;
    w.fog = 0.0f;
    w.precipitation = 0.0f;
    mtn_sky_compute(&clear, &p, &c, &w);
    mtn_sky_state none;
    mtn_sky_compute(&none, &p, &c, NULL);
    CHECK_NEAR(clear.light_color.x, none.light_color.x, 1e-6);
    CHECK_NEAR(clear.zenith.z, none.zenith.z, 1e-6);

    w.cloud_cover = 1.0f;
    w.darkness = 0.6f;
    w.fog = 0.5f;
    w.precipitation = 1.0f;
    mtn_sky_compute(&storm, &p, &c, &w);
    CHECK(lum(storm.light_color) < lum(clear.light_color) * 0.2f);
    CHECK(lum(storm.ambient) < lum(clear.ambient));
    CHECK(lum(storm.zenith) < lum(clear.zenith));
    CHECK(fabsf(storm.zenith.z - storm.zenith.x) < fabsf(clear.zenith.z - clear.zenith.x)); /* greyer */
    CHECK(storm.sun_visibility < 0.2f);
    CHECK_NEAR(storm.star_visibility, 0.0f, 1e-6);
    CHECK_NEAR(storm.cloud_cover, 1.0f, 0);
    CHECK_NEAR(storm.fog_density, 0.00028f + 0.005f + 0.0016f, 1e-6);

    w.flash = 1.0f;
    mtn_sky_compute(&flash, &p, &c, &w);
    CHECK(lum(flash.zenith) > lum(storm.zenith) + 0.1f);
    CHECK(lum(flash.horizon) > lum(storm.horizon));
    CHECK(lum(flash.ambient) > lum(storm.ambient));

    /* clouds hide the stars at night */
    mtn_clock_set_hour(&c, 0.0f);
    w.flash = 0.0f;
    w.cloud_cover = 0.5f;
    mtn_sky_compute(&storm, &p, &c, &w);
    CHECK_NEAR(storm.star_visibility, 0.5f, 1e-4);
}

MTN_SUITE(sky) {
    TEST_RUN(test_planet_and_clock);
    TEST_RUN(test_year_and_seasons);
    TEST_RUN(test_sun);
    TEST_RUN(test_moons);
    TEST_RUN(test_sky_day_night);
    TEST_RUN(test_sky_moonlight);
    TEST_RUN(test_sky_weather);
}
