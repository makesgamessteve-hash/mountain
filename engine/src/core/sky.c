/* Mountain engine - planet, clock, seasons, celestial bodies and sky colours. MIT License. */
#include "mtn/sky.h"
#include "mtn/weather.h"

#include <math.h>
#include <string.h>

void mtn_planet_earth(mtn_planet *p) {
    memset(p, 0, sizeof *p);
    strcpy(p->name, "Earth");
    p->day_length_s = 1440.0f; /* 24 real minutes: 1 game hour = 1 minute */
    p->days_per_season = 7;
    p->latitude_deg = 46.0f;
    p->axial_tilt_deg = 23.44f;
    p->sun_size = 0.02f;
    p->sun_color = v3(1.0f, 0.95f, 0.85f);
    p->sky_day_zenith = v3(0.22f, 0.42f, 0.82f);
    p->sky_day_horizon = v3(0.66f, 0.78f, 0.92f);
    p->sky_sunset_horizon = v3(1.0f, 0.52f, 0.28f);
    p->sky_night_zenith = v3(0.010f, 0.014f, 0.040f);
    p->sky_night_horizon = v3(0.035f, 0.045f, 0.090f);
    p->moon_count = 1;
    p->moons[0].period_days = 8.0f;
    p->moons[0].phase0 = 0.45f;
    p->moons[0].inclination = 5.1f * MTN_DEG2RAD;
    p->moons[0].size = 0.035f;
    p->moons[0].color = v3(0.95f, 0.93f, 0.88f);
}

void mtn_clock_init(mtn_clock *c, float start_day, float start_hour) {
    c->days = (double)start_day + (double)start_hour / 24.0;
    c->speed = 1.0f;
    c->paused = 0;
}

void mtn_clock_update(mtn_clock *c, const mtn_planet *p, float dt_real) {
    if (c->paused || p->day_length_s <= 0.0f) return;
    c->days += (double)dt_real * (double)c->speed / (double)p->day_length_s;
}

float mtn_clock_game_seconds(const mtn_clock *c, const mtn_planet *p, float dt_real) {
    if (c->paused || p->day_length_s <= 0.0f) return 0.0f;
    return dt_real * c->speed * 86400.0f / p->day_length_s;
}

float mtn_clock_hour(const mtn_clock *c) { return (float)((c->days - floor(c->days)) * 24.0); }
int mtn_clock_day(const mtn_clock *c) { return (int)floor(c->days); }

static float year_days(const mtn_planet *p) { return (float)(p->days_per_season > 0 ? p->days_per_season : 1) * 4.0f; }

float mtn_clock_year_frac(const mtn_clock *c, const mtn_planet *p) {
    double y = c->days / (double)year_days(p);
    return (float)(y - floor(y));
}

void mtn_clock_set_hour(mtn_clock *c, float hour) {
    hour = fmodf(hour, 24.0f);
    if (hour < 0) hour += 24.0f;
    c->days = floor(c->days) + (double)hour / 24.0;
}

void mtn_clock_set_season(mtn_clock *c, const mtn_planet *p, mtn_season s) {
    double yd = (double)year_days(p);
    double year_start = floor(c->days / yd) * yd;
    float hour = mtn_clock_hour(c);
    double dps = yd / 4.0;
    c->days = year_start + (double)s * dps + floor(dps * 0.5) + (double)hour / 24.0;
}

mtn_season_blend mtn_season_blend_at(float f) {
    mtn_season_blend b;
    float sum = 0.0f;
    f = f - floorf(f);
    for (int k = 0; k < MTN_SEASON_COUNT; k++) {
        float c = ((float)k + 0.5f) / 4.0f;
        float d = fabsf(f - c);
        if (d > 0.5f) d = 1.0f - d;
        float w = mtn_clampf(1.0f - (d - 0.07f) / 0.18f, 0.0f, 1.0f);
        b.w[k] = w;
        sum += w;
    }
    b.dominant = 0;
    for (int k = 0; k < MTN_SEASON_COUNT; k++) {
        b.w[k] /= sum;
        if (b.w[k] > b.w[b.dominant]) b.dominant = k;
    }
    return b;
}

const char *mtn_season_name(mtn_season s) {
    static const char *n[] = {"spring", "summer", "autumn", "winter"};
    return (s >= 0 && s < MTN_SEASON_COUNT) ? n[s] : "unknown";
}

static vec3 celestial_dir(float lat, float dec, float hour_angle) {
    float x_e = -cosf(dec) * sinf(hour_angle);
    float y_n = cosf(lat) * sinf(dec) - sinf(lat) * cosf(dec) * cosf(hour_angle);
    float z_u = sinf(lat) * sinf(dec) + cosf(lat) * cosf(dec) * cosf(hour_angle);
    return v3_norm(v3(x_e, z_u, -y_n));
}

static float declination(const mtn_planet *p, float year_frac) {
    return p->axial_tilt_deg * MTN_DEG2RAD * sinf(MTN_TAU * year_frac);
}

vec3 mtn_sun_direction(const mtn_planet *p, float year_frac, float hour) {
    float h = (hour - 12.0f) * (MTN_TAU / 24.0f);
    return celestial_dir(p->latitude_deg * MTN_DEG2RAD, declination(p, year_frac), h);
}

float mtn_moon_phase(const mtn_planet *p, int i, double days) {
    if (i < 0 || i >= p->moon_count || p->moons[i].period_days <= 0) return 0.0f;
    double ph = days / (double)p->moons[i].period_days + (double)p->moons[i].phase0;
    return (float)(ph - floor(ph));
}

vec3 mtn_moon_direction(const mtn_planet *p, int i, double days, float year_frac, float hour) {
    float phase = mtn_moon_phase(p, i, days);
    float h = (hour - 12.0f) * (MTN_TAU / 24.0f) - phase * MTN_TAU;
    float incl = (i >= 0 && i < p->moon_count) ? p->moons[i].inclination : 0.0f;
    float dec = p->axial_tilt_deg * MTN_DEG2RAD * sinf(MTN_TAU * (year_frac + phase)) +
                incl * sinf(MTN_TAU * phase * 3.0f + (float)i);
    return celestial_dir(p->latitude_deg * MTN_DEG2RAD, dec, h);
}

float mtn_day_length_hours(const mtn_planet *p, float year_frac) {
    float lat = p->latitude_deg * MTN_DEG2RAD;
    float c = -tanf(lat) * tanf(declination(p, year_frac));
    c = mtn_clampf(c, -1.0f, 1.0f);
    return 2.0f * acosf(c) / (MTN_TAU / 24.0f);
}

static vec3 desaturate(vec3 c, float amount, float darken) {
    float l = 0.3f * c.x + 0.59f * c.y + 0.11f * c.z;
    vec3 g = v3(l, l, l);
    return v3_scale(v3_lerp(c, g, amount), 1.0f - darken);
}

void mtn_sky_compute(mtn_sky_state *s, const mtn_planet *p, const mtn_clock *c, const struct mtn_weather *w) {
    float yf = mtn_clock_year_frac(c, p);
    float hour = mtn_clock_hour(c);
    vec3 sun = mtn_sun_direction(p, yf, hour);
    float e = sun.y;
    float cover = w ? w->cloud_cover : 0.0f;
    float darkness = w ? w->darkness : 0.0f;
    float flash = w ? w->flash : 0.0f;

    s->sun_dir = sun;
    s->daylight = mtn_smoothstep(-0.12f, 0.12f, e);
    s->sunset = (1.0f - mtn_smoothstep(0.0f, 0.30f, fabsf(e - 0.03f))) * mtn_smoothstep(-0.20f, -0.02f, e);
    s->sun_visibility = mtn_smoothstep(-0.03f, 0.02f, e) * (1.0f - cover * 0.9f);

    vec3 zen = v3_lerp(p->sky_night_zenith, p->sky_day_zenith, s->daylight);
    vec3 hor = v3_lerp(p->sky_night_horizon, p->sky_day_horizon, s->daylight);
    hor = v3_lerp(hor, p->sky_sunset_horizon, s->sunset * 0.85f);
    zen = v3_lerp(zen, v3(0.28f, 0.27f, 0.50f), s->sunset * 0.35f);

    /* moons */
    int best_moon = -1;
    float best_moon_light = 0.0f;
    for (int i = 0; i < p->moon_count && i < MTN_MAX_MOONS; i++) {
        s->moon_dir[i] = mtn_moon_direction(p, i, c->days, yf, hour);
        float ph = mtn_moon_phase(p, i, c->days);
        s->moon_lit[i] = 0.5f * (1.0f - cosf(MTN_TAU * ph));
        float ml = s->moon_lit[i] * mtn_smoothstep(-0.05f, 0.15f, s->moon_dir[i].y);
        if (ml > best_moon_light) {
            best_moon_light = ml;
            best_moon = i;
        }
    }

    vec3 sun_col = v3_lerp(v3(1.0f, 0.55f, 0.30f), p->sun_color, mtn_smoothstep(0.0f, 0.35f, e));
    float sun_int = mtn_smoothstep(-0.04f, 0.18f, e);
    if (sun_int > 0.02f || best_moon < 0) {
        s->light_dir = e > 0.02f ? sun : v3_norm(v3(sun.x, 0.02f, sun.z));
        s->light_color = v3_scale(sun_col, 1.05f * sun_int);
    } else {
        s->light_dir = s->moon_dir[best_moon];
        s->light_color = v3_scale(v3(0.55f, 0.62f, 0.85f), 0.32f * best_moon_light);
    }
    s->ambient = v3_add(v3_lerp(v3(0.035f, 0.045f, 0.085f), v3(0.36f, 0.40f, 0.48f), s->daylight),
                        v3_scale(v3(0.05f, 0.06f, 0.10f), best_moon_light));
    s->ambient = v3_lerp(s->ambient, v3(0.55f, 0.42f, 0.38f), s->sunset * 0.25f);

    /* weather influence */
    s->light_color = v3_scale(s->light_color, 1.0f - 0.78f * cover * cover - 0.3f * darkness);
    s->ambient = v3_scale(s->ambient, 1.0f - 0.35f * darkness);
    zen = desaturate(zen, cover * 0.85f, darkness * 0.55f);
    hor = desaturate(hor, cover * 0.75f, darkness * 0.45f);
    if (flash > 0.0f) {
        vec3 fl = v3(0.75f, 0.78f, 0.9f);
        zen = v3_lerp(zen, fl, flash * 0.7f);
        hor = v3_lerp(hor, fl, flash * 0.7f);
        s->ambient = v3_lerp(s->ambient, fl, flash * 0.6f);
    }
    s->zenith = zen;
    s->horizon = hor;
    s->fog_color = v3_lerp(hor, zen, 0.15f);
    s->fog_density = 0.00028f + (w ? (w->fog * 0.010f + w->precipitation * 0.0016f) : 0.0f);
    s->star_visibility = (1.0f - mtn_smoothstep(-0.28f, 0.0f, e)) * (1.0f - cover);
    s->star_angle = (float)fmod(c->days * (1.0 + 1.0 / (double)year_days(p)), 1.0) * MTN_TAU;
    float lat = p->latitude_deg * MTN_DEG2RAD;
    s->star_axis = v3(0.0f, sinf(lat), -cosf(lat));
    s->cloud_cover = cover;
    vec3 cday = v3(0.96f, 0.96f, 0.97f), cnight = v3(0.06f, 0.07f, 0.10f);
    s->cloud_color = v3_lerp(cnight, cday, s->daylight);
    s->cloud_color = v3_lerp(s->cloud_color, v3(1.0f, 0.62f, 0.45f), s->sunset * 0.6f);
    s->cloud_color = desaturate(s->cloud_color, cover * 0.5f, darkness * 0.6f);
}
