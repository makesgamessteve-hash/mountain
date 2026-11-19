/* Mountain engine - weather simulation. MIT License. */
#include "mtn/weather.h"

#include <math.h>
#include <string.h>

static const char *WX_NAMES[MTN_WX_COUNT] = {"clear", "cloudy", "overcast", "fog", "rain", "storm", "snow"};

const char *mtn_weather_name(mtn_weather_kind k) {
    return (k >= 0 && k < MTN_WX_COUNT) ? WX_NAMES[k] : "unknown";
}

int mtn_weather_from_name(const char *name) {
    for (int i = 0; i < MTN_WX_COUNT; i++)
        if (strcmp(name, WX_NAMES[i]) == 0) return i;
    return -1;
}

void mtn_weather_init(mtn_weather *w, uint32_t seed) {
    static const mtn_weather_look looks[MTN_WX_COUNT] = {
        /* cover precip wind  fog  lightning dark */
        {0.08f, 0.0f, 0.20f, 0.00f, 0.0f, 0.00f}, /* clear */
        {0.50f, 0.0f, 0.35f, 0.04f, 0.0f, 0.08f}, /* cloudy */
        {0.90f, 0.0f, 0.40f, 0.15f, 0.0f, 0.28f}, /* overcast */
        {0.70f, 0.0f, 0.08f, 1.00f, 0.0f, 0.22f}, /* fog */
        {0.95f, 0.7f, 0.50f, 0.30f, 0.0f, 0.42f}, /* rain */
        {1.00f, 1.0f, 0.90f, 0.35f, 1.0f, 0.62f}, /* storm */
        {0.95f, 0.6f, 0.30f, 0.40f, 0.0f, 0.32f}, /* snow */
    };
    static const float weights[MTN_SEASON_COUNT][MTN_WX_COUNT] = {
        {3.0f, 3.0f, 2.0f, 1.0f, 2.5f, 0.5f, 0.5f}, /* spring */
        {5.0f, 3.0f, 1.0f, 0.5f, 1.5f, 1.2f, 0.0f}, /* summer */
        {2.5f, 3.0f, 2.5f, 2.5f, 2.5f, 0.3f, 0.5f}, /* autumn */
        {2.5f, 2.0f, 3.0f, 2.0f, 0.5f, 0.0f, 3.5f}, /* winter */
    };
    memset(w, 0, sizeof *w);
    memcpy(w->look, looks, sizeof looks);
    memcpy(w->weights, weights, sizeof weights);
    mtn_rng_seed(&w->rng, seed, 77);
    w->current = w->target = MTN_WX_CLEAR;
    w->transition_s = 45.0f; /* real seconds, whatever the time speed */
    w->min_duration = 2.0f * 3600.0f;
    w->max_duration = 8.0f * 3600.0f;
    w->time_left = mtn_rng_range(&w->rng, w->min_duration, w->max_duration);
    w->wind_dir = v2(1.0f, 0.0f);
    w->lightning_timer = 5.0f;
    w->temperature = 12.0f;
    w->cloud_cover = looks[0].cloud_cover;
    w->wind = looks[0].wind;
}

float mtn_temperature(mtn_season_blend s, float hour, float altitude_frac) {
    static const float base[MTN_SEASON_COUNT] = {9.0f, 19.0f, 8.0f, -5.0f};
    float t = 0.0f;
    for (int k = 0; k < MTN_SEASON_COUNT; k++) t += base[k] * s.w[k];
    t += 4.0f * cosf((hour - 15.0f) * MTN_TAU / 24.0f);
    t -= altitude_frac * 16.0f;
    return t;
}

static void push_event(mtn_weather *w, int ev) {
    if (w->event_count < 8) w->events[w->event_count++] = ev;
}

mtn_weather_event mtn_weather_pop_event(mtn_weather *w) {
    if (w->event_count == 0) return MTN_WX_EVENT_NONE;
    int ev = w->events[0];
    memmove(w->events, w->events + 1, sizeof(int) * (size_t)(w->event_count - 1));
    w->event_count--;
    return (mtn_weather_event)ev;
}

static mtn_weather_kind choose_next(mtn_weather *w, mtn_season_blend s) {
    float total = 0.0f, wk[MTN_WX_COUNT];
    for (int k = 0; k < MTN_WX_COUNT; k++) {
        wk[k] = 0.0f;
        for (int q = 0; q < MTN_SEASON_COUNT; q++) wk[k] += w->weights[q][k] * s.w[q];
        total += wk[k];
    }
    if (total <= 0.0f) return MTN_WX_CLEAR;
    /* the next state is always different from the current one */
    total -= wk[w->current];
    wk[w->current] = 0.0f;
    if (total <= 0.0f) return w->current == MTN_WX_CLEAR ? MTN_WX_CLOUDY : MTN_WX_CLEAR;
    float r = mtn_rng_float(&w->rng) * total;
    for (int k = 0; k < MTN_WX_COUNT; k++) {
        if (r < wk[k]) return (mtn_weather_kind)k;
        r -= wk[k];
    }
    return MTN_WX_CLEAR;
}

void mtn_weather_set(mtn_weather *w, mtn_weather_kind k, int lock) {
    if (k < 0 || k >= MTN_WX_COUNT) return;
    if (w->target != k) {
        if (w->blend > 0.5f) w->current = w->target;
        w->target = k;
        w->blend = 0.0f;
    }
    w->locked = lock;
}

void mtn_weather_unlock(mtn_weather *w) {
    w->locked = 0;
    w->time_left = mtn_rng_range(&w->rng, w->min_duration, w->max_duration);
}

void mtn_weather_update(mtn_weather *w, float dt_game, float dt_real, mtn_season_blend s, float hour) {
    if (!w->locked) {
        w->time_left -= dt_game;
        if (w->time_left <= 0.0f && w->target == w->current) {
            w->target = choose_next(w, s);
            w->blend = 0.0f;
            w->time_left = mtn_rng_range(&w->rng, w->min_duration, w->max_duration);
        }
    }
    if (w->target != w->current) {
        w->blend += dt_real / w->transition_s;
        if (w->blend >= 1.0f) {
            w->current = w->target;
            w->blend = 0.0f;
        }
    }
    float b = mtn_smoothstep(0.0f, 1.0f, w->blend);
    const mtn_weather_look *a = &w->look[w->current], *t = &w->look[w->target];
    w->cloud_cover = mtn_lerpf(a->cloud_cover, t->cloud_cover, b);
    w->precipitation = mtn_lerpf(a->precipitation, t->precipitation, b);
    w->wind = mtn_lerpf(a->wind, t->wind, b);
    w->fog = mtn_lerpf(a->fog, t->fog, b);
    w->darkness = mtn_lerpf(a->darkness, t->darkness, b);
    w->lightning_rate = mtn_lerpf(a->lightning, t->lightning, b);
    w->temperature = mtn_temperature(s, hour, 0.35f);
    int snow_kind = (w->current == MTN_WX_SNOW && b < 0.5f) || (w->target == MTN_WX_SNOW && b >= 0.5f);
    w->precip_snow = snow_kind || w->temperature < 1.0f;

    /* slowly veering wind */
    float ang = atan2f(w->wind_dir.y, w->wind_dir.x);
    ang += (mtn_rng_float(&w->rng) - 0.5f) * 0.02f * dt_real;
    w->wind_dir = v2(cosf(ang), sinf(ang));

    /* lightning and thunder (real time so they stay readable at high time speeds) */
    w->flash = mtn_maxf(0.0f, w->flash - dt_real * 3.5f);
    if (w->lightning_rate > 0.3f) {
        w->lightning_timer -= dt_real;
        if (w->lightning_timer <= 0.0f) {
            w->flash = 1.0f;
            push_event(w, MTN_WX_EVENT_LIGHTNING);
            w->thunder_delay = mtn_rng_range(&w->rng, 0.8f, 4.0f);
            w->lightning_timer = mtn_rng_range(&w->rng, 10.0f, 40.0f) / w->lightning_rate;
        }
    }
    if (w->thunder_delay > 0.0f) {
        w->thunder_delay -= dt_real;
        if (w->thunder_delay <= 0.0f) push_event(w, MTN_WX_EVENT_THUNDER);
    }

    /* fresh snow builds up while it snows and melts otherwise */
    if (w->precip_snow && w->precipitation > 0.2f) {
        w->snow_ground += dt_game / (2.0f * 3600.0f) * mtn_minf(1.0f, w->precipitation * 1.5f);
    } else {
        float melt_h = s.w[MTN_WINTER] > 0.5f ? 24.0f : 6.0f;
        w->snow_ground -= dt_game / (melt_h * 3600.0f);
    }
    w->snow_ground = mtn_clampf(w->snow_ground, 0.0f, 1.0f);
}
