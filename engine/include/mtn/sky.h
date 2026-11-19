/* Mountain engine - planet, clock, seasons, celestial bodies and sky colours. MIT License. */
#ifndef MTN_SKY_H
#define MTN_SKY_H

#include "mtn/math.h"

#define MTN_MAX_MOONS 4

typedef enum { MTN_SPRING = 0, MTN_SUMMER, MTN_AUTUMN, MTN_WINTER, MTN_SEASON_COUNT } mtn_season;

typedef struct {
    float period_days;   /* synodic period in game days */
    float phase0;        /* 0..1 phase at day 0 (0 new, 0.5 full) */
    float inclination;   /* radians, tilt of the orbit */
    float size;          /* angular radius in radians */
    vec3 color;
} mtn_moon_def;

typedef struct {
    char name[32];
    float day_length_s;      /* real seconds per game day at 1x speed */
    int days_per_season;
    float latitude_deg;
    float axial_tilt_deg;
    float sun_size;          /* angular radius in radians */
    vec3 sun_color;
    vec3 sky_day_zenith, sky_day_horizon;
    vec3 sky_sunset_horizon;
    vec3 sky_night_zenith, sky_night_horizon;
    int moon_count;
    mtn_moon_def moons[MTN_MAX_MOONS];
} mtn_planet;

typedef struct {
    double days;   /* game days since the world started (fractional) */
    float speed;   /* game time multiplier */
    int paused;
} mtn_clock;

typedef struct { float w[MTN_SEASON_COUNT]; int dominant; } mtn_season_blend;

typedef struct {
    vec3 zenith, horizon;
    vec3 light_dir;     /* direction towards the dominant light (sun or moon) */
    vec3 light_color;   /* direct light colour * intensity */
    vec3 ambient;
    vec3 fog_color;
    float fog_density;
    float star_visibility;
    float sun_visibility;
    float daylight;     /* 0 night .. 1 day */
    float sunset;       /* 0..1 amount of golden hour colouring */
    vec3 sun_dir;
    vec3 moon_dir[MTN_MAX_MOONS];
    float moon_lit[MTN_MAX_MOONS];   /* illuminated fraction 0..1 */
    float star_angle;   /* rotation of the celestial sphere */
    vec3 star_axis;
    float cloud_cover;
    vec3 cloud_color;
} mtn_sky_state;

struct mtn_weather;

void mtn_planet_earth(mtn_planet *p);

void mtn_clock_init(mtn_clock *c, float start_day, float start_hour);
void mtn_clock_update(mtn_clock *c, const mtn_planet *p, float dt_real);
float mtn_clock_hour(const mtn_clock *c);
int mtn_clock_day(const mtn_clock *c);
float mtn_clock_year_frac(const mtn_clock *c, const mtn_planet *p);
void mtn_clock_set_hour(mtn_clock *c, float hour);
void mtn_clock_set_season(mtn_clock *c, const mtn_planet *p, mtn_season s);
/* game seconds that pass for dt real seconds */
float mtn_clock_game_seconds(const mtn_clock *c, const mtn_planet *p, float dt_real);

mtn_season_blend mtn_season_blend_at(float year_frac);
const char *mtn_season_name(mtn_season s);

vec3 mtn_sun_direction(const mtn_planet *p, float year_frac, float hour);
vec3 mtn_moon_direction(const mtn_planet *p, int i, double days, float year_frac, float hour);
float mtn_moon_phase(const mtn_planet *p, int i, double days); /* 0 new .. 0.5 full .. 1 new */
float mtn_day_length_hours(const mtn_planet *p, float year_frac);

void mtn_sky_compute(mtn_sky_state *s, const mtn_planet *p, const mtn_clock *c, const struct mtn_weather *w);

#endif
