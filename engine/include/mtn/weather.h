/* Mountain engine - weather simulation. MIT License. */
#ifndef MTN_WEATHER_H
#define MTN_WEATHER_H

#include "mtn/math.h"
#include "mtn/rng.h"
#include "mtn/sky.h"

typedef enum {
    MTN_WX_CLEAR = 0,
    MTN_WX_CLOUDY,
    MTN_WX_OVERCAST,
    MTN_WX_FOG,
    MTN_WX_RAIN,
    MTN_WX_STORM,
    MTN_WX_SNOW,
    MTN_WX_COUNT
} mtn_weather_kind;

typedef struct {
    float cloud_cover, precipitation, wind, fog, lightning, darkness;
} mtn_weather_look;

typedef enum { MTN_WX_EVENT_NONE = 0, MTN_WX_EVENT_LIGHTNING, MTN_WX_EVENT_THUNDER } mtn_weather_event;

typedef struct mtn_weather {
    mtn_weather_kind current, target;
    float blend;             /* 0..1 progress towards target */
    float transition_s;      /* real seconds for a transition */
    float time_left;         /* game seconds until the next change */
    float min_duration, max_duration;
    int locked;
    mtn_rng rng;
    mtn_weather_look look[MTN_WX_COUNT];
    float weights[MTN_SEASON_COUNT][MTN_WX_COUNT];
    /* blended output */
    float cloud_cover, precipitation, wind, fog, darkness, lightning_rate;
    vec2 wind_dir;
    float flash;             /* 0..1 lightning flash brightness */
    float snow_ground;       /* 0..1 fresh snow cover */
    float temperature;       /* degrees C at the reference altitude */
    int precip_snow;
    float lightning_timer;
    float thunder_delay;
    int events[8];
    int event_count;
} mtn_weather;

void mtn_weather_init(mtn_weather *w, uint32_t seed);
/* dt_game in game seconds, dt_real in real seconds (for flashes and thunder). */
void mtn_weather_update(mtn_weather *w, float dt_game, float dt_real, mtn_season_blend season, float hour);
void mtn_weather_set(mtn_weather *w, mtn_weather_kind k, int lock);
void mtn_weather_unlock(mtn_weather *w);
mtn_weather_event mtn_weather_pop_event(mtn_weather *w);
const char *mtn_weather_name(mtn_weather_kind k);
int mtn_weather_from_name(const char *name);
/* temperature (C) at an altitude fraction for a season/hour; snow below ~1 C. */
float mtn_temperature(mtn_season_blend season, float hour, float altitude_frac);

#endif
