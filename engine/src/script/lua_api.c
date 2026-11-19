/* Mountain engine - Lua 5.4 scripting layer. Exposes the engine to the game
 * scripts in assets/scripts (world, time, weather, flora, fauna, house, camera,
 * photos, ui, audio, input, storage). Scripts run in a restricted environment.
 * MIT License. */
#include "mtn/game.h"
#include "mtn/vfs.h"

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static mtn_game *g_game;

/* ------------------------------------------------------------------ helpers */

static float fnum(lua_State *L, int t, const char *k, float def) {
    lua_getfield(L, t, k);
    float v = lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : def;
    lua_pop(L, 1);
    return v;
}

static int fint(lua_State *L, int t, const char *k, int def) {
    lua_getfield(L, t, k);
    int v = lua_isnumber(L, -1) ? (int)lua_tointeger(L, -1) : def;
    if (lua_isboolean(L, -1)) v = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return v;
}

static void fstr(lua_State *L, int t, const char *k, char *out, size_t n) {
    lua_getfield(L, t, k);
    if (lua_isstring(L, -1)) snprintf(out, n, "%s", lua_tostring(L, -1));
    lua_pop(L, 1);
}

static vec3 color_at(lua_State *L, int idx, vec3 def) {
    if (!lua_istable(L, idx)) return def;
    idx = lua_absindex(L, idx);
    vec3 c = def;
    lua_rawgeti(L, idx, 1); c.x = lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : c.x; lua_pop(L, 1);
    lua_rawgeti(L, idx, 2); c.y = lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : c.y; lua_pop(L, 1);
    lua_rawgeti(L, idx, 3); c.z = lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : c.z; lua_pop(L, 1);
    if (c.x > 1.0f || c.y > 1.0f || c.z > 1.0f) c = v3_scale(c, 1.0f / 255.0f);
    return c;
}

static vec3 fcolor(lua_State *L, int t, const char *k, vec3 def) {
    lua_getfield(L, t, k);
    vec3 c = color_at(L, -1, def);
    lua_pop(L, 1);
    return c;
}

static int mask_from(lua_State *L, int t, const char *k, const char *const *names, int count, int def) {
    lua_getfield(L, t, k);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return def;
    }
    int mask = 0;
    lua_Integer n = luaL_len(L, -1);
    for (lua_Integer i = 1; i <= n; i++) {
        lua_rawgeti(L, -1, i);
        const char *s = lua_tostring(L, -1);
        for (int j = 0; s && j < count; j++)
            if (strcmp(s, names[j]) == 0) mask |= 1 << j;
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return mask;
}

static const char *const BIOMES[MTN_BIOME_COUNT] = {"water", "forest", "meadow", "alpine", "scree", "rock", "snow"};
static const char *const SEASONS[MTN_SEASON_COUNT] = {"spring", "summer", "autumn", "winter"};

static uint32_t ucolor(lua_State *L, int idx, uint32_t def) {
    if (lua_isinteger(L, idx)) return (uint32_t)lua_tointeger(L, idx);
    if (lua_istable(L, idx)) {
        vec3 c = color_at(L, idx, v3(1, 1, 1));
        lua_rawgeti(L, idx, 4);
        float a = lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : 1.0f;
        lua_pop(L, 1);
        if (a > 1.0f) a /= 255.0f;
        return mtn_pack_rgba(c.x, c.y, c.z, a);
    }
    return def;
}

/* ------------------------------------------------------------------ engine */

static int l_engine_log(lua_State *L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; i++) {
        size_t len;
        const char *s = luaL_tolstring(L, i, &len);
        fprintf(stdout, "%s%s", i > 1 ? "\t" : "", s);
        lua_pop(L, 1);
    }
    fprintf(stdout, "\n");
    fflush(stdout);
    return 0;
}

static int l_engine_quit(lua_State *L) { (void)L; g_game->quit_request = 1; return 0; }
static int l_engine_platform(lua_State *L) { lua_pushstring(L, g_game->platform); return 1; }
static int l_engine_version(lua_State *L) { lua_pushstring(L, MTN_VERSION); return 1; }
static int l_engine_fps(lua_State *L) { lua_pushnumber(L, g_game->fps); return 1; }
static int l_engine_time(lua_State *L) { lua_pushnumber(L, g_game->real_time); return 1; }
static int l_engine_frame(lua_State *L) { lua_pushinteger(L, g_game->frame); return 1; }
static int l_engine_test_mode(lua_State *L) { lua_pushboolean(L, g_game->test_mode); return 1; }
static int l_engine_fullscreen(lua_State *L) { g_game->fullscreen_request = lua_toboolean(L, 1) ? 1 : 2; return 0; }
static int l_engine_reload(lua_State *L) { (void)L; g_game->reload_request = 1; return 0; }
static int l_engine_xr(lua_State *L) {
    lua_pushboolean(L, g_game->xr_supported);
    lua_pushboolean(L, g_game->xr_active);
    return 2;
}

static int l_engine_settings(lua_State *L) {
    mtn_settings *s = &g_game->settings;
    if (lua_istable(L, 1)) {
        s->mouse_sensitivity = fnum(L, 1, "mouse_sensitivity", s->mouse_sensitivity);
        s->invert_y = fint(L, 1, "invert_y", s->invert_y);
        s->volume = mtn_clampf(fnum(L, 1, "volume", s->volume), 0.0f, 1.0f);
        int lines = fint(L, 1, "pixel_lines", s->pixel_lines);
        if (lines == 180 || lines == 216 || lines == 270 || lines == 360) s->pixel_lines = lines;
        s->fov = mtn_clampf(fnum(L, 1, "fov", s->fov), 50.0f, 100.0f);
        s->show_fps = fint(L, 1, "show_fps", s->show_fps);
    }
    lua_newtable(L);
    lua_pushnumber(L, s->mouse_sensitivity); lua_setfield(L, -2, "mouse_sensitivity");
    lua_pushboolean(L, s->invert_y); lua_setfield(L, -2, "invert_y");
    lua_pushnumber(L, s->volume); lua_setfield(L, -2, "volume");
    lua_pushinteger(L, s->pixel_lines); lua_setfield(L, -2, "pixel_lines");
    lua_pushnumber(L, s->fov); lua_setfield(L, -2, "fov");
    lua_pushboolean(L, s->show_fps); lua_setfield(L, -2, "show_fps");
    return 1;
}

/* ------------------------------------------------------------------ world */

static int l_world_params(lua_State *L) {
    mtn_terrain_params *p = &g_game->tparams;
    luaL_checktype(L, 1, LUA_TTABLE);
    p->size = fint(L, 1, "size", p->size);
    p->cell = fnum(L, 1, "cell", p->cell);
    p->peak_height = fnum(L, 1, "peak_height", p->peak_height);
    p->range_height = fnum(L, 1, "range_height", p->range_height);
    p->hill_height = fnum(L, 1, "hill_height", p->hill_height);
    p->ridge_strength = fnum(L, 1, "ridge_strength", p->ridge_strength);
    p->erosion_drops = fint(L, 1, "erosion_drops", p->erosion_drops);
    p->lake_min_depth = fnum(L, 1, "lake_min_depth", p->lake_min_depth);
    p->tree_line = fnum(L, 1, "tree_line", p->tree_line);
    p->snow_line = fnum(L, 1, "snow_line", p->snow_line);
    p->playable_radius = fnum(L, 1, "playable_radius", p->playable_radius);
    lua_getfield(L, 1, "sea_level");
    if (lua_isnumber(L, -1)) p->sea_level = (float)lua_tonumber(L, -1);
    lua_pop(L, 1);
    return 0;
}

static int l_world_generate(lua_State *L) {
    uint32_t seed;
    if (lua_isnumber(L, 1)) seed = (uint32_t)lua_tointeger(L, 1);
    else seed = (uint32_t)time(NULL) * 2654435761u + (uint32_t)clock();
    mtn_game_new_world(g_game, seed);
    lua_pushinteger(L, seed);
    return 1;
}

static int l_world_seed(lua_State *L) { lua_pushinteger(L, g_game->seed); return 1; }
static int l_world_exists(lua_State *L) { lua_pushboolean(L, g_game->has_world); return 1; }
static int l_world_height(lua_State *L) {
    lua_pushnumber(L, mtn_terrain_height(&g_game->terrain, (float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2)));
    return 1;
}
static int l_world_biome(lua_State *L) {
    lua_pushstring(L, mtn_biome_name(mtn_terrain_biome(&g_game->terrain, (float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2))));
    return 1;
}
static int push3(lua_State *L, vec3 v) {
    lua_pushnumber(L, v.x);
    lua_pushnumber(L, v.y);
    lua_pushnumber(L, v.z);
    return 3;
}
static int l_world_slope(lua_State *L) {
    lua_pushnumber(L, mtn_terrain_slope(&g_game->terrain, (float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2)));
    return 1;
}
static int l_world_water(lua_State *L) {
    float w = mtn_terrain_water(&g_game->terrain, (float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2));
    if (w == MTN_NO_WATER) return 0;
    lua_pushnumber(L, w);
    return 1;
}
static int l_world_viewpoint(lua_State *L) {
    int i = (int)luaL_checkinteger(L, 1) - 1;
    if (i < 0 || i >= g_game->terrain.vp_count) return 0;
    const mtn_viewpoint *v = &g_game->terrain.vps[i];
    lua_pushnumber(L, v->pos.x);
    lua_pushnumber(L, v->pos.y - 1.7);
    lua_pushnumber(L, v->pos.z);
    lua_pushnumber(L, v->yaw);
    return 4;
}
static int l_world_summit(lua_State *L) { return push3(L, g_game->terrain.summit); }
static int l_world_checksum(lua_State *L) {
    lua_pushinteger(L, g_game->has_world ? (lua_Integer)mtn_terrain_checksum(&g_game->terrain) : 0);
    return 1;
}
static int l_world_trailhead(lua_State *L) { return push3(L, g_game->terrain.trailhead); }
static int l_world_info(lua_State *L) {
    mtn_terrain *t = &g_game->terrain;
    lua_newtable(L);
    lua_pushnumber(L, t->world_size); lua_setfield(L, -2, "size");
    lua_pushnumber(L, t->min_h); lua_setfield(L, -2, "min_height");
    lua_pushnumber(L, t->max_h); lua_setfield(L, -2, "max_height");
    lua_pushinteger(L, t->lake_count); lua_setfield(L, -2, "lakes");
    lua_pushinteger(L, t->trail_len); lua_setfield(L, -2, "trail_points");
    lua_pushinteger(L, t->cairn_count); lua_setfield(L, -2, "cairns");
    lua_pushinteger(L, t->vp_count); lua_setfield(L, -2, "viewpoints");
    lua_pushinteger(L, g_game->flora.count); lua_setfield(L, -2, "plants");
    lua_pushnumber(L, g_game->snowline); lua_setfield(L, -2, "snowline");
    return 1;
}
static int l_world_altitude(lua_State *L) {
    lua_pushnumber(L, mtn_terrain_altitude_frac(&g_game->terrain, (float)luaL_checknumber(L, 1)));
    return 1;
}

/* ------------------------------------------------------------------ planet / time */

static int l_planet_set(lua_State *L) {
    mtn_planet *p = &g_game->planet;
    luaL_checktype(L, 1, LUA_TTABLE);
    fstr(L, 1, "name", p->name, sizeof p->name);
    p->day_length_s = fnum(L, 1, "day_length", p->day_length_s);
    p->days_per_season = fint(L, 1, "days_per_season", p->days_per_season);
    p->latitude_deg = fnum(L, 1, "latitude", p->latitude_deg);
    p->axial_tilt_deg = fnum(L, 1, "axial_tilt", p->axial_tilt_deg);
    p->sun_size = fnum(L, 1, "sun_size", p->sun_size);
    p->sun_color = fcolor(L, 1, "sun_color", p->sun_color);
    p->sky_day_zenith = fcolor(L, 1, "day_zenith", p->sky_day_zenith);
    p->sky_day_horizon = fcolor(L, 1, "day_horizon", p->sky_day_horizon);
    p->sky_sunset_horizon = fcolor(L, 1, "sunset_horizon", p->sky_sunset_horizon);
    p->sky_night_zenith = fcolor(L, 1, "night_zenith", p->sky_night_zenith);
    p->sky_night_horizon = fcolor(L, 1, "night_horizon", p->sky_night_horizon);
    lua_getfield(L, 1, "moons");
    if (lua_istable(L, -1)) {
        int n = (int)luaL_len(L, -1);
        p->moon_count = n > MTN_MAX_MOONS ? MTN_MAX_MOONS : n;
        for (int i = 0; i < p->moon_count; i++) {
            lua_rawgeti(L, -1, i + 1);
            int t = lua_gettop(L);
            mtn_moon_def *m = &p->moons[i];
            m->period_days = fnum(L, t, "period", 8.0f);
            m->phase0 = fnum(L, t, "phase", 0.0f);
            m->inclination = fnum(L, t, "inclination", 5.0f) * MTN_DEG2RAD;
            m->size = fnum(L, t, "size", 0.035f);
            m->color = fcolor(L, t, "color", v3(0.95f, 0.93f, 0.88f));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    return 0;
}

static int l_planet_name(lua_State *L) { lua_pushstring(L, g_game->planet.name); return 1; }
static int l_time_hour(lua_State *L) { lua_pushnumber(L, mtn_clock_hour(&g_game->clock)); return 1; }
static int l_time_set_hour(lua_State *L) { mtn_clock_set_hour(&g_game->clock, (float)luaL_checknumber(L, 1)); return 0; }
static int l_time_day(lua_State *L) { lua_pushinteger(L, mtn_clock_day(&g_game->clock) + 1); return 1; }
static int l_time_speed(lua_State *L) {
    if (lua_isnumber(L, 1)) g_game->clock.speed = mtn_clampf((float)lua_tonumber(L, 1), 0.0f, 3600.0f);
    lua_pushnumber(L, g_game->clock.speed);
    return 1;
}
static int l_time_pause(lua_State *L) {
    if (!lua_isnone(L, 1)) g_game->clock.paused = lua_toboolean(L, 1);
    lua_pushboolean(L, g_game->clock.paused);
    return 1;
}
static int l_time_season(lua_State *L) {
    mtn_season_blend b = g_game->season;
    lua_pushstring(L, mtn_season_name((mtn_season)b.dominant));
    lua_newtable(L);
    for (int k = 0; k < MTN_SEASON_COUNT; k++) {
        lua_pushnumber(L, b.w[k]);
        lua_setfield(L, -2, SEASONS[k]);
    }
    return 2;
}
static int l_time_set_season(lua_State *L) {
    const char *s = luaL_checkstring(L, 1);
    for (int k = 0; k < MTN_SEASON_COUNT; k++)
        if (strcmp(s, SEASONS[k]) == 0) {
            mtn_clock_set_season(&g_game->clock, &g_game->planet, (mtn_season)k);
            g_game->season = mtn_season_blend_at(mtn_clock_year_frac(&g_game->clock, &g_game->planet));
            lua_pushboolean(L, 1);
            return 1;
        }
    lua_pushboolean(L, 0);
    return 1;
}
static int l_time_year(lua_State *L) { lua_pushnumber(L, mtn_clock_year_frac(&g_game->clock, &g_game->planet)); return 1; }
static int l_time_daylight(lua_State *L) { lua_pushnumber(L, g_game->sky.daylight); return 1; }
static int l_time_moon(lua_State *L) {
    int i = (int)luaL_optinteger(L, 1, 1) - 1;
    lua_pushnumber(L, mtn_moon_phase(&g_game->planet, i, g_game->clock.days));
    lua_pushnumber(L, i >= 0 && i < MTN_MAX_MOONS ? g_game->sky.moon_lit[i] : 0.0);
    return 2;
}
static int l_time_day_length(lua_State *L) {
    lua_pushnumber(L, mtn_day_length_hours(&g_game->planet, mtn_clock_year_frac(&g_game->clock, &g_game->planet)));
    return 1;
}
static int l_time_sun(lua_State *L) { return push3(L, g_game->sky.sun_dir); }

/* ------------------------------------------------------------------ weather */

static int l_weather_get(lua_State *L) {
    mtn_weather *w = &g_game->weather;
    lua_pushstring(L, mtn_weather_name(w->blend > 0.5f ? w->target : w->current));
    lua_newtable(L);
    lua_pushnumber(L, w->cloud_cover); lua_setfield(L, -2, "clouds");
    lua_pushnumber(L, w->precipitation); lua_setfield(L, -2, "precipitation");
    lua_pushboolean(L, w->precip_snow); lua_setfield(L, -2, "snowing");
    lua_pushnumber(L, w->wind); lua_setfield(L, -2, "wind");
    lua_pushnumber(L, w->fog); lua_setfield(L, -2, "fog");
    lua_pushnumber(L, w->snow_ground); lua_setfield(L, -2, "snow_ground");
    lua_pushnumber(L, g_game->temperature); lua_setfield(L, -2, "temperature");
    lua_pushboolean(L, w->locked); lua_setfield(L, -2, "locked");
    lua_pushstring(L, mtn_weather_name(w->target)); lua_setfield(L, -2, "target");
    return 2;
}
static int l_weather_set(lua_State *L) {
    int k = mtn_weather_from_name(luaL_checkstring(L, 1));
    if (k < 0) return luaL_error(L, "unknown weather '%s'", lua_tostring(L, 1));
    mtn_weather_set(&g_game->weather, (mtn_weather_kind)k, lua_isnone(L, 2) ? 1 : lua_toboolean(L, 2));
    if (lua_toboolean(L, 3)) { /* instant */
        g_game->weather.current = g_game->weather.target;
        g_game->weather.blend = 0.0f;
    }
    return 0;
}
static int l_weather_table(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    for (int k = 0; k < MTN_SEASON_COUNT; k++) {
        lua_getfield(L, 1, SEASONS[k]);
        if (lua_istable(L, -1)) {
            int t = lua_gettop(L);
            for (int w = 0; w < MTN_WX_COUNT; w++)
                g_game->weather.weights[k][w] = mtn_maxf(0.0f, fnum(L, t, mtn_weather_name((mtn_weather_kind)w), g_game->weather.weights[k][w]));
        }
        lua_pop(L, 1);
    }
    return 0;
}
static int l_weather_unlock(lua_State *L) { (void)L; mtn_weather_unlock(&g_game->weather); return 0; }
static int l_weather_snow(lua_State *L) {
    g_game->weather.snow_ground = mtn_clampf((float)luaL_checknumber(L, 1), 0, 1);
    return 0;
}

/* ------------------------------------------------------------------ flora / fauna */

static int l_flora_species(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    static const char *const KINDS[] = {"tree", "bush", "ground", "boulder"};
    static const char *const CROWNS[] = {"cone", "round", "oval", "umbrella"};
    mtn_plant_species s;
    mtn_species_defaults(&s);
    fstr(L, 1, "name", s.name, sizeof s.name);
    char kind[16] = "tree", crown[16] = "round";
    fstr(L, 1, "kind", kind, sizeof kind);
    fstr(L, 1, "crown", crown, sizeof crown);
    for (int i = 0; i < 4; i++) {
        if (strcmp(kind, KINDS[i]) == 0) s.kind = i;
        if (strcmp(crown, CROWNS[i]) == 0) s.crown = i;
    }
    s.deciduous = fint(L, 1, "deciduous", 0);
    s.height_min = fnum(L, 1, "height_min", s.height_min);
    s.height_max = fnum(L, 1, "height_max", s.height_max);
    s.trunk_radius = fnum(L, 1, "trunk_radius", s.trunk_radius);
    s.crown_width = fnum(L, 1, "crown_width", s.crown_width);
    lua_getfield(L, 1, "tiles");
    if (lua_istable(L, -1)) {
        s.leaf_tile_count = 0;
        for (int i = 1; i <= 4; i++) {
            lua_rawgeti(L, -1, i);
            if (lua_isnumber(L, -1)) s.leaf_tiles[s.leaf_tile_count++] = (int)lua_tointeger(L, -1);
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    s.bark_tile = fint(L, 1, "bark_tile", s.bark_tile);
    s.bark_color = fcolor(L, 1, "bark_color", s.bark_color);
    lua_getfield(L, 1, "colors");
    if (lua_istable(L, -1)) {
        int t = lua_gettop(L);
        for (int k = 0; k < MTN_SEASON_COUNT; k++) s.leaf_color[k] = fcolor(L, t, SEASONS[k], s.leaf_color[k]);
    }
    lua_pop(L, 1);
    s.blossom_tile = fint(L, 1, "blossom_tile", -1);
    s.bloom_mask = mask_from(L, 1, "bloom", SEASONS, MTN_SEASON_COUNT, 0);
    s.alt_min = fnum(L, 1, "alt_min", s.alt_min);
    s.alt_max = fnum(L, 1, "alt_max", s.alt_max);
    s.slope_max = fnum(L, 1, "slope_max", s.slope_max);
    s.moist_min = fnum(L, 1, "moist_min", s.moist_min);
    s.moist_max = fnum(L, 1, "moist_max", s.moist_max);
    s.biome_mask = mask_from(L, 1, "biomes", BIOMES, MTN_BIOME_COUNT, s.biome_mask);
    s.density = fnum(L, 1, "density", s.density);
    s.patchiness = fnum(L, 1, "patchiness", s.patchiness);
    int idx = mtn_flora_add_species(&g_game->flora, &s);
    lua_pushinteger(L, idx);
    return 1;
}
static int l_flora_near(lua_State *L) {
    vec3 c = v3((float)luaL_checknumber(L, 1), 0, (float)luaL_checknumber(L, 2));
    float r = (float)luaL_checknumber(L, 3);
    int n = 0;
    for (int i = 0; i < g_game->flora.count; i++) {
        const mtn_plant *p = &g_game->flora.plants[i];
        if (!p->hidden && g_game->flora.species[p->species].kind != MTN_PLANT_GROUND && v3_dist_xz(p->pos, c) < r) n++;
    }
    lua_pushinteger(L, n);
    return 1;
}
static int l_flora_clear(lua_State *L) { (void)L; mtn_flora_clear_species(&g_game->flora); return 0; }
static int l_flora_count(lua_State *L) {
    lua_pushinteger(L, g_game->flora.count);
    lua_pushinteger(L, g_game->flora.species_count);
    return 2;
}

static int l_fauna_species(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    static const char *const KINDS[] = {"quadruped", "bird", "soaring", "butterfly", "firefly"};
    mtn_animal_species s;
    mtn_animal_defaults(&s);
    fstr(L, 1, "name", s.name, sizeof s.name);
    char kind[16] = "quadruped";
    fstr(L, 1, "kind", kind, sizeof kind);
    for (int i = 0; i < 5; i++)
        if (strcmp(kind, KINDS[i]) == 0) s.kind = i;
    s.size = fnum(L, 1, "size", s.size);
    s.leg_len = fnum(L, 1, "leg", s.leg_len);
    s.neck_len = fnum(L, 1, "neck", s.neck_len);
    s.head_size = fnum(L, 1, "head", s.head_size);
    s.color = fcolor(L, 1, "color", s.color);
    s.color2 = fcolor(L, 1, "color2", s.color2);
    s.horns = fint(L, 1, "horns", s.horns);
    s.tail = fnum(L, 1, "tail", s.tail);
    s.walk_speed = fnum(L, 1, "walk_speed", s.walk_speed);
    s.run_speed = fnum(L, 1, "run_speed", s.run_speed);
    s.comfort_dist = fnum(L, 1, "comfort", s.comfort_dist);
    s.alt_min = fnum(L, 1, "alt_min", s.alt_min);
    s.alt_max = fnum(L, 1, "alt_max", s.alt_max);
    s.biome_mask = mask_from(L, 1, "biomes", BIOMES, MTN_BIOME_COUNT, s.biome_mask);
    s.hour_start = fnum(L, 1, "from", s.hour_start);
    s.hour_end = fnum(L, 1, "to", s.hour_end);
    s.season_mask = mask_from(L, 1, "seasons", SEASONS, MTN_SEASON_COUNT, s.season_mask);
    s.group_min = fint(L, 1, "group_min", s.group_min);
    s.group_max = fint(L, 1, "group_max", s.group_max);
    s.max_groups = fint(L, 1, "max_groups", s.max_groups);
    s.spawn_chance = fnum(L, 1, "chance", s.spawn_chance);
    s.sprite_tile = fint(L, 1, "tile", s.sprite_tile);
    fstr(L, 1, "sound", s.sound, sizeof s.sound);
    lua_pushinteger(L, mtn_fauna_add_species(&g_game->fauna, &s));
    return 1;
}
static int l_fauna_clear(lua_State *L) {
    (void)L;
    g_game->fauna.species_count = 0;
    mtn_fauna_clear(&g_game->fauna);
    return 0;
}
static int l_fauna_count(lua_State *L) {
    int sp = -1;
    if (lua_isstring(L, 1))
        for (int i = 0; i < g_game->fauna.species_count; i++)
            if (strcmp(g_game->fauna.species[i].name, lua_tostring(L, 1)) == 0) sp = i;
    if (lua_isstring(L, 1) && sp < 0) {
        lua_pushinteger(L, 0);
        return 1;
    }
    lua_pushinteger(L, mtn_fauna_count(&g_game->fauna, sp));
    return 1;
}
static int l_fauna_spawn(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    for (int i = 0; i < g_game->fauna.species_count; i++)
        if (strcmp(g_game->fauna.species[i].name, name) == 0) {
            vec3 at = v3((float)luaL_checknumber(L, 2), 0, (float)luaL_checknumber(L, 3));
            lua_pushinteger(L, mtn_fauna_spawn_group(&g_game->fauna, &g_game->terrain, i, at));
            return 1;
        }
    lua_pushinteger(L, 0);
    return 1;
}

/* ------------------------------------------------------------------ player */

static int l_player_pos(lua_State *L) { return push3(L, g_game->player.pos); }
static int l_player_set_pos(lua_State *L) {
    g_game->player.pos = v3((float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3));
    g_game->player.vel = v3(0, 0, 0);
    return 0;
}
static int l_player_yaw(lua_State *L) {
    if (lua_isnumber(L, 1)) g_game->player.yaw = (float)lua_tonumber(L, 1);
    if (lua_isnumber(L, 2)) g_game->player.pitch = (float)lua_tonumber(L, 2);
    lua_pushnumber(L, g_game->player.yaw);
    lua_pushnumber(L, g_game->player.pitch);
    return 2;
}
static int l_player_trailhead(lua_State *L) { (void)L; mtn_game_teleport_trailhead(g_game); return 0; }
static int l_player_inside(lua_State *L) { lua_pushboolean(L, g_game->interior); return 1; }
static int l_player_surface(lua_State *L) { lua_pushstring(L, mtn_surface_name(g_game->player.surface)); return 1; }
static int l_player_altitude(lua_State *L) {
    lua_pushnumber(L, g_game->interior ? g_game->house.floor_y : g_game->player.pos.y);
    return 1;
}
static int l_player_speeds(lua_State *L) {
    if (lua_isnumber(L, 1)) g_game->player.walk_speed = (float)lua_tonumber(L, 1);
    if (lua_isnumber(L, 2)) g_game->player.run_speed = (float)lua_tonumber(L, 2);
    lua_pushnumber(L, g_game->player.walk_speed);
    lua_pushnumber(L, g_game->player.run_speed);
    return 2;
}

/* ------------------------------------------------------------------ house */

static int l_house_summon(lua_State *L) {
    lua_pushboolean(L, mtn_game_summon_house(g_game));
    lua_pushstring(L, g_game->message_time > 0 ? g_game->message : "");
    return 2;
}
static int l_house_unsummon(lua_State *L) { lua_pushboolean(L, mtn_game_unsummon_house(g_game)); return 1; }
static int l_house_exists(lua_State *L) { lua_pushboolean(L, mtn_house_exists(&g_game->house)); return 1; }
static int l_house_state(lua_State *L) {
    static const char *st[] = {"none", "appearing", "present", "vanishing"};
    lua_pushstring(L, st[g_game->house.state]);
    lua_pushstring(L, mtn_house_variant_name(g_game->house.variant));
    return 2;
}
static int l_house_interact(lua_State *L) { lua_pushboolean(L, mtn_game_interact(g_game)); return 1; }
static int l_house_door(lua_State *L) {
    lua_pushboolean(L, g_game->house.door_open);
    lua_pushnumber(L, g_game->house.door_angle);
    return 2;
}
static int l_house_distance(lua_State *L) {
    if (!mtn_house_exists(&g_game->house)) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushnumber(L, g_game->interior ? 0.0 : v3_dist_xz(g_game->player.pos, g_game->house.origin));
    lua_pushnumber(L, g_game->interior ? sqrtf(g_game->player.pos.x * g_game->player.pos.x + g_game->player.pos.z * g_game->player.pos.z)
                                       : mtn_house_door_distance(&g_game->house, g_game->player.pos));
    return 2;
}
static int l_house_door_pos(lua_State *L) {
    if (!mtn_house_exists(&g_game->house)) return 0;
    vec3 d = mtn_house_door_world(&g_game->house);
    vec3 f = g_game->house.front;
    lua_pushnumber(L, d.x);
    lua_pushnumber(L, d.y);
    lua_pushnumber(L, d.z);
    lua_pushnumber(L, atan2f(-f.x, f.z)); /* yaw that looks at the door from outside */
    return 4;
}
static int l_house_can_unsummon(lua_State *L) {
    lua_pushboolean(L, !g_game->interior && mtn_house_can_unsummon(&g_game->house, g_game->player.pos));
    return 1;
}
static int l_house_room(lua_State *L) {
    int idx = g_game->interior ? mtn_house_room_at(g_game->player.pos.z) : -1;
    lua_pushinteger(L, idx);
    if (idx >= 0) {
        int t = mtn_house_room_type_for(&g_game->house, idx);
        lua_pushstring(L, t >= 0 ? g_game->house.types[t].name : "room");
        return 2;
    }
    return 1;
}
static int l_house_clear_rooms(lua_State *L) { (void)L; g_game->house.type_count = 0; return 0; }
static int l_house_define_room(lua_State *L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    mtn_room_type t;
    mtn_room_type_defaults(&t);
    fstr(L, 1, "name", t.name, sizeof t.name);
    t.width = mtn_clampf(fnum(L, 1, "width", t.width), 4.0f, 16.0f);
    t.height = mtn_clampf(fnum(L, 1, "height", t.height), 3.0f, 8.0f);
    t.wall_tile = fint(L, 1, "wall_tile", t.wall_tile);
    t.floor_tile = fint(L, 1, "floor_tile", t.floor_tile);
    vec3 wc = fcolor(L, 1, "wall_color", v3(1, 1, 1)), fc = fcolor(L, 1, "floor_color", v3(1, 1, 1));
    t.wall_color = mtn_pack_rgba(wc.x, wc.y, wc.z, 1);
    t.floor_color = mtn_pack_rgba(fc.x, fc.y, fc.z, 1);
    t.light_color = fcolor(L, 1, "light", t.light_color);
    t.weight = fnum(L, 1, "weight", 1.0f);
    t.is_cabin = fint(L, 1, "cabin", 0);
    lua_getfield(L, 1, "items");
    if (lua_istable(L, -1)) {
        int n = (int)luaL_len(L, -1);
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(L, -1, i);
            int it = lua_gettop(L);
            char pre[24] = "";
            lua_rawgeti(L, it, 1);
            if (lua_isstring(L, -1)) snprintf(pre, sizeof pre, "%s", lua_tostring(L, -1));
            lua_pop(L, 1);
            fstr(L, it, "prefab", pre, sizeof pre);
            mtn_room_add_item(&t, pre, fnum(L, it, "x", 0), fnum(L, it, "z", 0), fnum(L, it, "rot", 0), fnum(L, it, "scale", 1),
                              fint(L, it, "arg", 0));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    lua_pushinteger(L, mtn_house_add_room_type(&g_game->house, &t));
    return 1;
}

/* ------------------------------------------------------------------ camera & photos */

static int l_camera_mode(lua_State *L) {
    if (!lua_isnone(L, 1)) g_game->camera_mode = lua_toboolean(L, 1);
    lua_pushboolean(L, g_game->camera_mode);
    return 1;
}
static int l_camera_fov(lua_State *L) {
    if (lua_isnumber(L, 1)) g_game->photo_fov = mtn_clampf((float)lua_tonumber(L, 1), 20.0f, 90.0f);
    lua_pushnumber(L, g_game->photo_fov);
    return 1;
}
static int l_camera_filter(lua_State *L) {
    if (lua_isstring(L, 1)) {
        int f = mtn_filter_from_name(lua_tostring(L, 1));
        if (f >= 0) g_game->photo_filter = f;
    }
    lua_pushstring(L, mtn_filter_name(g_game->photo_filter));
    return 1;
}
static int l_camera_shoot(lua_State *L) { (void)L; g_game->photo_request = 1; return 0; }
static int l_photos_count(lua_State *L) { lua_pushinteger(L, g_game->gallery.count); return 1; }
static int l_photos_name(lua_State *L) {
    int i = (int)luaL_checkinteger(L, 1) - 1;
    if (i < 0 || i >= g_game->gallery.count) return 0;
    lua_pushstring(L, g_game->gallery.names[i]);
    return 1;
}
static int l_photos_delete(lua_State *L) { g_game->delete_request = (int)luaL_checkinteger(L, 1) - 1; return 0; }
static int l_photos_download(lua_State *L) { g_game->download_request = (int)luaL_checkinteger(L, 1) - 1; return 0; }
static int l_photos_dir(lua_State *L) {
    char d[600];
    snprintf(d, sizeof d, "%sphotos", g_game->user_dir);
    lua_pushstring(L, d);
    return 1;
}

/* ------------------------------------------------------------------ ui */

static int l_ui_size(lua_State *L) {
    lua_pushnumber(L, g_game->ui.w);
    lua_pushnumber(L, g_game->ui.h);
    return 2;
}
static int l_ui_text(lua_State *L) {
    float w = mtn_ui_text(&g_game->ui, (float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_optnumber(L, 5, 1.0),
                          luaL_checkstring(L, 3), ucolor(L, 4, 0xFFFFFFFFu));
    lua_pushnumber(L, w);
    return 1;
}
static int l_ui_text_width(lua_State *L) {
    lua_pushnumber(L, mtn_ui_text_width(luaL_checkstring(L, 1), (float)luaL_optnumber(L, 2, 1.0)));
    return 1;
}
static int l_ui_rect(lua_State *L) {
    mtn_ui_rect(&g_game->ui, (float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3),
                (float)luaL_checknumber(L, 4), ucolor(L, 5, 0x80000000u));
    return 0;
}
static int l_ui_icon(lua_State *L) {
    mtn_ui_icon(&g_game->ui, (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_optnumber(L, 4, 16),
                (int)luaL_checkinteger(L, 1), ucolor(L, 5, 0xFFFFFFFFu));
    return 0;
}
static int l_ui_button(lua_State *L) {
    int clicked = mtn_ui_button(&g_game->ui, luaL_checkstring(L, 1), (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3),
                                (float)luaL_checknumber(L, 4), (float)luaL_checknumber(L, 5), luaL_optstring(L, 6, ""),
                                (int)luaL_optinteger(L, 7, -1));
    if (clicked) mtn_game_play(g_game, "ui_click", 0.4f);
    lua_pushboolean(L, clicked);
    return 1;
}
static int l_ui_photo(lua_State *L) {
    int idx = (int)luaL_checkinteger(L, 1) - 1;
    mtn_ui_image(&g_game->ui, (float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3), (float)luaL_checknumber(L, 4),
                 (float)luaL_checknumber(L, 5), MTN_UI_TEX_PHOTO + idx, 0, 0, 1, 1, 0xFFFFFFFFu);
    return 0;
}
static int l_ui_modal(lua_State *L) {
    if (!lua_isnone(L, 1)) g_game->ui_modal = lua_toboolean(L, 1);
    lua_pushboolean(L, g_game->ui_modal);
    return 1;
}
static int l_ui_message(lua_State *L) {
    if (lua_isstring(L, 1)) mtn_game_message(g_game, lua_tostring(L, 1), (float)luaL_optnumber(L, 2, 3.0));
    if (g_game->message_time > 0) {
        lua_pushstring(L, g_game->message);
        lua_pushnumber(L, g_game->message_time);
        return 2;
    }
    return 0;
}
static int l_ui_pointer(lua_State *L) {
    lua_pushnumber(L, g_game->ui.px);
    lua_pushnumber(L, g_game->ui.py);
    lua_pushboolean(L, g_game->ui.down);
    return 3;
}
static int l_ui_error(lua_State *L) {
    if (g_game->script_error[0]) {
        lua_pushstring(L, g_game->script_error);
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ input / audio / storage */

static int l_input_mode(lua_State *L) {
    static const char *modes[] = {"keyboard", "touch", "gamepad", "xr"};
    if (lua_isstring(L, 1))
        for (int i = 0; i < 4; i++)
            if (strcmp(lua_tostring(L, 1), modes[i]) == 0) g_game->input.mode = i;
    lua_pushstring(L, modes[mtn_clampi(g_game->input.mode, 0, 3)]);
    return 1;
}
static int l_input_down(lua_State *L) {
    lua_pushboolean(L, mtn_input_down(&g_game->input, mtn_action_from_name(luaL_checkstring(L, 1))));
    return 1;
}
static int l_input_bind(lua_State *L) {
    const char *a = luaL_checkstring(L, 1), *k = luaL_checkstring(L, 2);
    if (mtn_action_from_name(a) < 0) return luaL_error(L, "unknown action '%s'", a);
    if (g_game->binding_count >= MTN_MAX_BINDINGS) return 0;
    mtn_binding *b = &g_game->bindings[g_game->binding_count++];
    snprintf(b->action, sizeof b->action, "%s", a);
    snprintf(b->key, sizeof b->key, "%s", k);
    return 0;
}

static int l_input_unbind_all(lua_State *L) {
    (void)L;
    g_game->clear_bindings = 1;
    g_game->binding_count = 0;
    return 0;
}

static int l_audio_play(lua_State *L) {
    int s = mtn_mixer_find(&g_game->mixer, luaL_checkstring(L, 1));
    lua_pushboolean(L, mtn_mixer_play(&g_game->mixer, s, (float)luaL_optnumber(L, 2, 1.0), (float)luaL_optnumber(L, 3, 0.0),
                                      (float)luaL_optnumber(L, 4, 1.0)) >= 0);
    return 1;
}
static int l_audio_ambient(lua_State *L) {
    int s = mtn_mixer_find(&g_game->mixer, luaL_checkstring(L, 1));
    mtn_mixer_ambient(&g_game->mixer, s, (float)luaL_checknumber(L, 2), (float)luaL_optnumber(L, 3, 2.0));
    return 0;
}
static int l_audio_loaded(lua_State *L) { lua_pushinteger(L, g_game->mixer.sound_count); return 1; }

static int storage_path(lua_State *L, char *out, size_t n) {
    const char *name = luaL_checkstring(L, 1);
    for (const char *p = name; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '-')) return 0;
    snprintf(out, n, "%s%s.lua", g_game->user_dir, name);
    return 1;
}
static int l_storage_save(lua_State *L) {
    char path[700];
    size_t len;
    const char *data = luaL_checklstring(L, 2, &len);
    int ok = storage_path(L, path, sizeof path) && mtn_file_write(path, data, len);
    if (ok) g_game->storage_dirty = 1;
    lua_pushboolean(L, ok);
    return 1;
}
static int l_storage_load(lua_State *L) {
    char path[700];
    if (!storage_path(L, path, sizeof path)) return 0;
    size_t size;
    char *d = mtn_file_read(path, &size);
    if (!d) return 0;
    lua_pushlstring(L, d, size);
    free(d);
    return 1;
}

/* ------------------------------------------------------------------ restricted loading */

static int l_require(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    lua_getfield(L, LUA_REGISTRYINDEX, "mtn_loaded");
    lua_getfield(L, -1, name);
    if (!lua_isnil(L, -1)) return 1;
    lua_pop(L, 1);
    char rel[256];
    snprintf(rel, sizeof rel, "scripts/%s.lua", name);
    for (char *p = rel + 8; *p; p++)
        if (*p == '.' && strcmp(p, ".lua") != 0) *p = '/';
    size_t size;
    char *src = mtn_vfs_read(rel, &size);
    if (!src) return luaL_error(L, "module '%s' not found (%s)", name, rel);
    char chunk[300];
    snprintf(chunk, sizeof chunk, "@%s", rel);
    int st = luaL_loadbuffer(L, src, size, chunk);
    free(src);
    if (st != LUA_OK) return lua_error(L);
    lua_call(L, 0, 1);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushboolean(L, 1);
    }
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, name);
    return 1;
}

static int l_os_time(lua_State *L) { lua_pushinteger(L, (lua_Integer)time(NULL)); return 1; }
static int l_os_clock(lua_State *L) { lua_pushnumber(L, (lua_Number)clock() / CLOCKS_PER_SEC); return 1; }
static int l_os_date(lua_State *L) {
    char buf[64];
    time_t t = (time_t)luaL_optinteger(L, 2, (lua_Integer)time(NULL));
    struct tm *tm = localtime(&t);
    strftime(buf, sizeof buf, luaL_optstring(L, 1, "%Y-%m-%d %H:%M:%S"), tm);
    lua_pushstring(L, buf);
    return 1;
}

typedef struct { const char *lib; const luaL_Reg *fns; } lib_def;

void mtn_script_error(mtn_game *g, const char *where, const char *msg) {
    snprintf(g->script_error, sizeof g->script_error, "%s: %s", where, msg ? msg : "error");
    fprintf(stderr, "[script] %s\n", g->script_error);
}

static void open_api(lua_State *L) {
    static const luaL_Reg engine[] = {{"log", l_engine_log}, {"quit", l_engine_quit}, {"platform", l_engine_platform},
        {"version", l_engine_version}, {"fps", l_engine_fps}, {"time", l_engine_time}, {"frame", l_engine_frame},
        {"test_mode", l_engine_test_mode}, {"fullscreen", l_engine_fullscreen}, {"reload", l_engine_reload},
        {"settings", l_engine_settings}, {"xr", l_engine_xr}, {NULL, NULL}};
    static const luaL_Reg world[] = {{"params", l_world_params}, {"generate", l_world_generate}, {"seed", l_world_seed},
        {"exists", l_world_exists}, {"height", l_world_height}, {"biome", l_world_biome}, {"summit", l_world_summit},
        {"trailhead", l_world_trailhead}, {"info", l_world_info}, {"altitude", l_world_altitude}, {"slope", l_world_slope},
        {"water", l_world_water}, {"viewpoint", l_world_viewpoint}, {"checksum", l_world_checksum}, {NULL, NULL}};
    static const luaL_Reg planet[] = {{"set", l_planet_set}, {"name", l_planet_name}, {NULL, NULL}};
    static const luaL_Reg tm[] = {{"hour", l_time_hour}, {"set_hour", l_time_set_hour}, {"day", l_time_day},
        {"speed", l_time_speed}, {"pause", l_time_pause}, {"season", l_time_season}, {"set_season", l_time_set_season},
        {"year", l_time_year}, {"daylight", l_time_daylight}, {"moon", l_time_moon}, {"day_length", l_time_day_length},
        {"sun", l_time_sun}, {NULL, NULL}};
    static const luaL_Reg weather[] = {{"get", l_weather_get}, {"set", l_weather_set}, {"unlock", l_weather_unlock}, {"table", l_weather_table},
        {"snow", l_weather_snow}, {NULL, NULL}};
    static const luaL_Reg flora[] = {{"species", l_flora_species}, {"clear", l_flora_clear}, {"count", l_flora_count}, {"near", l_flora_near}, {NULL, NULL}};
    static const luaL_Reg fauna[] = {{"species", l_fauna_species}, {"clear", l_fauna_clear}, {"count", l_fauna_count},
        {"spawn", l_fauna_spawn}, {NULL, NULL}};
    static const luaL_Reg player[] = {{"pos", l_player_pos}, {"set_pos", l_player_set_pos}, {"yaw", l_player_yaw},
        {"trailhead", l_player_trailhead}, {"inside", l_player_inside}, {"surface", l_player_surface},
        {"altitude", l_player_altitude}, {"speeds", l_player_speeds}, {NULL, NULL}};
    static const luaL_Reg house[] = {{"summon", l_house_summon}, {"unsummon", l_house_unsummon}, {"exists", l_house_exists},
        {"state", l_house_state}, {"interact", l_house_interact}, {"door", l_house_door}, {"distance", l_house_distance},
        {"can_unsummon", l_house_can_unsummon}, {"door_pos", l_house_door_pos}, {"room", l_house_room}, {"clear_rooms", l_house_clear_rooms},
        {"define_room", l_house_define_room}, {NULL, NULL}};
    static const luaL_Reg camera[] = {{"mode", l_camera_mode}, {"fov", l_camera_fov}, {"filter", l_camera_filter},
        {"shoot", l_camera_shoot}, {NULL, NULL}};
    static const luaL_Reg photos[] = {{"count", l_photos_count}, {"name", l_photos_name}, {"delete", l_photos_delete},
        {"download", l_photos_download}, {"dir", l_photos_dir}, {NULL, NULL}};
    static const luaL_Reg ui[] = {{"size", l_ui_size}, {"text", l_ui_text}, {"text_width", l_ui_text_width}, {"rect", l_ui_rect},
        {"icon", l_ui_icon}, {"button", l_ui_button}, {"photo", l_ui_photo}, {"modal", l_ui_modal}, {"message", l_ui_message},
        {"pointer", l_ui_pointer}, {"error", l_ui_error}, {NULL, NULL}};
    static const luaL_Reg input[] = {{"mode", l_input_mode}, {"down", l_input_down}, {"bind", l_input_bind}, {"unbind_all", l_input_unbind_all}, {NULL, NULL}};
    static const luaL_Reg audio[] = {{"play", l_audio_play}, {"ambient", l_audio_ambient}, {"loaded", l_audio_loaded}, {NULL, NULL}};
    static const luaL_Reg storage[] = {{"save", l_storage_save}, {"load", l_storage_load}, {NULL, NULL}};
    static const luaL_Reg os[] = {{"time", l_os_time}, {"clock", l_os_clock}, {"date", l_os_date}, {NULL, NULL}};
    static const lib_def libs[] = {{"engine", engine}, {"world", world}, {"planet", planet}, {"time", tm}, {"weather", weather},
        {"flora", flora}, {"fauna", fauna}, {"player", player}, {"house", house}, {"camera", camera}, {"photos", photos},
        {"ui", ui}, {"input", input}, {"audio", audio}, {"storage", storage}, {"os", os}};
    for (size_t i = 0; i < sizeof libs / sizeof libs[0]; i++) {
        lua_newtable(L);
        luaL_setfuncs(L, libs[i].fns, 0);
        lua_setglobal(L, libs[i].lib);
    }
    lua_pushcfunction(L, l_engine_log);
    lua_setglobal(L, "print");
    lua_pushcfunction(L, l_require);
    lua_setglobal(L, "require");
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, "mtn_loaded");
    /* no file or native code access from scripts */
    static const char *const banned[] = {"dofile", "loadfile", NULL};
    for (int i = 0; banned[i]; i++) {
        lua_pushnil(L);
        lua_setglobal(L, banned[i]);
    }
}

static lua_State *new_state(void) {
    lua_State *L = luaL_newstate();
    static const luaL_Reg safe[] = {{LUA_GNAME, luaopen_base}, {LUA_TABLIBNAME, luaopen_table}, {LUA_STRLIBNAME, luaopen_string},
                                    {LUA_MATHLIBNAME, luaopen_math}, {LUA_UTF8LIBNAME, luaopen_utf8}, {LUA_COLIBNAME, luaopen_coroutine},
                                    {NULL, NULL}};
    for (const luaL_Reg *l = safe; l->func; l++) {
        luaL_requiref(L, l->name, l->func, 1);
        lua_pop(L, 1);
    }
    open_api(L);
    return L;
}

static int traceback(lua_State *L) {
    luaL_traceback(L, L, lua_tostring(L, 1), 1);
    return 1;
}

int mtn_script_call(mtn_game *g, const char *fn, int nargs) {
    lua_State *L = g->L;
    if (!L) return 0;
    lua_getglobal(L, fn);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1 + nargs);
        return 0;
    }
    lua_insert(L, -1 - nargs);
    lua_pushcfunction(L, traceback);
    lua_insert(L, -2 - nargs);
    int base = lua_gettop(L) - nargs - 1;
    int st = lua_pcall(L, nargs, 0, base);
    if (st != LUA_OK) {
        mtn_script_error(g, fn, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    lua_remove(L, base);
    return st == LUA_OK;
}

int mtn_script_run_string(mtn_game *g, const char *code, const char *name) {
    if (!g->L) return 0;
    lua_pushcfunction(g->L, traceback);
    int st = luaL_loadbuffer(g->L, code, strlen(code), name);
    if (st == LUA_OK) st = lua_pcall(g->L, 0, 0, -2);
    if (st != LUA_OK) {
        mtn_script_error(g, name, lua_tostring(g->L, -1));
        lua_pop(g->L, 1);
    }
    lua_pop(g->L, 1);
    return st == LUA_OK;
}

static int load_main(mtn_game *g) {
    size_t size;
    char *src = mtn_vfs_read("scripts/main.lua", &size);
    if (!src) {
        mtn_script_error(g, "scripts/main.lua", "file not found next to the game (assets/scripts/main.lua)");
        return 0;
    }
    lua_pushcfunction(g->L, traceback);
    int st = luaL_loadbuffer(g->L, src, size, "@scripts/main.lua");
    free(src);
    if (st == LUA_OK) st = lua_pcall(g->L, 0, 0, -2);
    if (st != LUA_OK) {
        mtn_script_error(g, "scripts/main.lua", lua_tostring(g->L, -1));
        lua_pop(g->L, 1);
    }
    lua_pop(g->L, 1);
    return st == LUA_OK;
}

int mtn_script_init(mtn_game *g) {
    g_game = g;
    g->script_error[0] = 0;
    g->L = new_state();
    if (!load_main(g)) return 0;
    lua_pushboolean(g->L, 0);
    return mtn_script_call(g, "init", 1);
}

int mtn_script_reload(mtn_game *g) {
    /* soft reload: keep the Lua state (and coroutines such as game tests),
     * forget cached modules and run main.lua again */
    g_game = g;
    g->script_error[0] = 0;
    if (!g->L) g->L = new_state();
    lua_newtable(g->L);
    lua_setfield(g->L, LUA_REGISTRYINDEX, "mtn_loaded");
    if (!load_main(g)) return 0;
    lua_pushboolean(g->L, 1);
    return mtn_script_call(g, "init", 1);
}

void mtn_script_shutdown(mtn_game *g) {
    if (g->L) lua_close(g->L);
    g->L = NULL;
}

void mtn_script_action(mtn_game *g, const char *action, int pressed) {
    if (!g->L) return;
    lua_pushstring(g->L, action);
    lua_pushboolean(g->L, pressed);
    mtn_script_call(g, "action", 2);
}

void mtn_script_update(mtn_game *g, float dt) {
    if (!g->L) return;
    lua_pushnumber(g->L, dt);
    mtn_script_call(g, "update", 1);
}

void mtn_script_ui(mtn_game *g) {
    if (g->L) mtn_script_call(g, "draw", 0);
    if (g->script_error[0]) {
        /* scripts are broken: show the error instead of crashing */
        mtn_ui *ui = &g->ui;
        mtn_ui_rect(ui, 8, 8, ui->w - 16, 64, 0xD0101040u);
        char line[96];
        snprintf(line, sizeof line, "%.56s", g->script_error);
        mtn_ui_text(ui, 14, 14, 1.0f, "Script error (F5 reloads):", 0xFF80C0FFu);
        mtn_ui_text(ui, 14, 30, 1.0f, line, 0xFFFFFFFFu);
        if (strlen(g->script_error) > 56) {
            snprintf(line, sizeof line, "%.56s", g->script_error + 56);
            mtn_ui_text(ui, 14, 42, 1.0f, line, 0xFFFFFFFFu);
        }
    }
}
