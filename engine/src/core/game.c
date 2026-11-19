/* Mountain engine - game state and simulation step (platform independent). MIT License. */
#include "mtn/game.h"
#include "mtn/vfs.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void mtn_game_init(mtn_game *g, const char *platform, const char *user_dir) {
    memset(g, 0, sizeof *g);
    snprintf(g->platform, sizeof g->platform, "%s", platform ? platform : "unknown");
    snprintf(g->user_dir, sizeof g->user_dir, "%s", user_dir ? user_dir : "");
    mtn_planet_earth(&g->planet);
    mtn_clock_init(&g->clock, 0.0f, 8.0f);
    mtn_terrain_default_params(&g->tparams);
    mtn_flora_init(&g->flora);
    mtn_fauna_init(&g->fauna, 1);
    mtn_weather_init(&g->weather, 1);
    mtn_house_init(&g->house);
    mtn_input_init(&g->input);
    mtn_touch_init(&g->touch);
    mtn_ui_init(&g->ui);
    mtn_mixer_init(&g->mixer, 44100);
    for (int i = 0; i < MTN_MAX_ROOMS; i++) {
        mtn_mesh_init(&g->room_solid[i]);
        mtn_mesh_init(&g->room_sprites[i]);
        mtn_mesh_init(&g->room_glow[i]);
    }
    mtn_mesh_init(&g->house_solid);
    mtn_mesh_init(&g->house_sprites);
    mtn_mesh_init(&g->house_glow);
    mtn_mesh_init(&g->door_mesh);
    mtn_house_build_door(&g->door_mesh);
    g->settings.mouse_sensitivity = 1.0f;
    g->settings.volume = 0.8f;
    g->settings.pixel_lines = 216;
    g->settings.fov = 70.0f;
    g->photo_fov = 60.0f;
    g->download_request = -1;
    g->delete_request = -1;
    g->season = mtn_season_blend_at(0.0f);
    mtn_player_init(&g->player, v3(0, 0, 0), 0.0f);
}

void mtn_game_shutdown(mtn_game *g) {
    mtn_script_shutdown(g);
    mtn_terrain_free(&g->terrain);
    mtn_flora_free(&g->flora);
    mtn_ui_free(&g->ui);
    mtn_mixer_free(&g->mixer);
    for (int i = 0; i < MTN_MAX_ROOMS; i++) {
        mtn_mesh_free(&g->room_solid[i]);
        mtn_mesh_free(&g->room_sprites[i]);
        mtn_mesh_free(&g->room_glow[i]);
    }
    mtn_mesh_free(&g->house_solid);
    mtn_mesh_free(&g->house_sprites);
    mtn_mesh_free(&g->house_glow);
    mtn_mesh_free(&g->door_mesh);
}

void mtn_game_message(mtn_game *g, const char *msg, float seconds) {
    snprintf(g->message, sizeof g->message, "%s", msg);
    g->message_time = seconds;
}

void mtn_game_play(mtn_game *g, const char *sound, float vol) {
    int s = mtn_mixer_find(&g->mixer, sound);
    if (s >= 0) mtn_mixer_play(&g->mixer, s, vol, 0.0f, 1.0f);
}

void mtn_game_play_at(mtn_game *g, const char *sound, vec3 pos, float vol) {
    int s = mtn_mixer_find(&g->mixer, sound);
    if (s < 0) return;
    vec3 rel = v3_sub(pos, g->player.pos);
    float d = v3_len(rel);
    vec3 right = v3(cosf(g->player.yaw), 0, sinf(g->player.yaw));
    float pan = d > 0.01f ? mtn_clampf(v3_dot(rel, right) / d, -1.0f, 1.0f) * 0.8f : 0.0f;
    float att = vol / (1.0f + d / 20.0f);
    mtn_mixer_play(&g->mixer, s, att, pan, 1.0f);
}

int mtn_game_load_sounds(mtn_game *g) {
    static const char *names[] = {"wind_loop", "rain_loop", "birds_loop", "crickets_loop", "stream_loop", "fire_loop",
                                  "step_grass_1", "step_grass_2", "step_rock_1", "step_rock_2", "step_snow_1",
                                  "step_snow_2", "step_wood_1", "step_wood_2", "door_open", "door_close", "shutter",
                                  "summon", "unsummon", "thunder", "owl", "marmot", "raven", "ui_click", "chime"};
    int loaded = 0;
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        char rel[96];
        size_t size;
        snprintf(rel, sizeof rel, "sounds/%s.wav", names[i]);
        char *data = mtn_vfs_read(rel, &size);
        if (!data) continue;
        mtn_sound s;
        if (mtn_wav_parse((const uint8_t *)data, size, &s) && mtn_mixer_add(&g->mixer, names[i], &s) >= 0) loaded++;
        free(data);
    }
    return loaded;
}

static void rebuild_house_meshes(mtn_game *g) {
    mtn_mesh_clear(&g->house_solid);
    mtn_mesh_clear(&g->house_sprites);
    mtn_mesh_clear(&g->house_glow);
    mtn_house_build_exterior(&g->house, &g->terrain, &g->flora, &g->house_solid, &g->house_sprites, &g->house_glow);
    g->house_mesh_version++;
}

void mtn_game_teleport_trailhead(mtn_game *g) {
    vec3 p = g->terrain.trailhead;
    float yaw = 0.0f;
    if (g->terrain.trail_len > 3) {
        vec3 a = g->terrain.trail[0], b = g->terrain.trail[3];
        yaw = atan2f(b.x - a.x, -(b.z - a.z));
        p = a;
    }
    p.y = fmaxf(mtn_terrain_height(&g->terrain, p.x, p.z), p.y);
    mtn_player prev = g->player;
    mtn_player_init(&g->player, p, yaw);
    g->player.walk_speed = prev.walk_speed > 0 ? prev.walk_speed : g->player.walk_speed;
    g->player.run_speed = prev.run_speed > 0 ? prev.run_speed : g->player.run_speed;
    g->interior = 0;
    g->house.inside = 0;
}

void mtn_game_new_world(mtn_game *g, uint32_t seed) {
    g->seed = seed;
    g->tparams.seed = seed;
    mtn_terrain_free(&g->terrain);
    mtn_terrain_generate(&g->terrain, &g->tparams);
    mtn_flora_populate(&g->flora, &g->terrain, seed);
    mtn_fauna_init(&g->fauna, seed);
    mtn_weather_init(&g->weather, seed);
    {
        int tc = g->house.type_count;
        mtn_room_type types[MTN_MAX_ROOM_TYPES];
        memcpy(types, g->house.types, sizeof types);
        int ver = g->house.version;
        mtn_house_init(&g->house);
        memcpy(g->house.types, types, sizeof types);
        g->house.type_count = tc;
        g->house.version = ver + 1;
    }
    rebuild_house_meshes(g);
    g->has_world = 1;
    g->world_version++;
    mtn_game_teleport_trailhead(g);
}

float mtn_game_snowline(const mtn_game *g) {
    static const float lines[MTN_SEASON_COUNT] = {0.66f, 0.82f, 0.74f, 0.36f};
    float base = 0.0f;
    for (int k = 0; k < MTN_SEASON_COUNT; k++) base += lines[k] * g->season.w[k];
    base += g->terrain.p.snow_line - 0.80f; /* scripts may move the summer snow line */
    return mtn_lerpf(base, -0.05f, g->weather.snow_ground);
}

float mtn_game_current_fov(const mtn_game *g) { return g->camera_mode ? g->photo_fov : g->settings.fov; }

/* ------------------------------------------------------------------ colliders */

static float world_ground(void *ctx, float x, float z, vec3 *n, int *surface) {
    mtn_game *g = ctx;
    float h = mtn_terrain_height(&g->terrain, x, z);
    *n = mtn_terrain_normal(&g->terrain, x, z);
    mtn_biome b = mtn_terrain_biome(&g->terrain, x, z);
    float alt = mtn_terrain_altitude_frac(&g->terrain, h);
    if (alt > g->snowline) *surface = MTN_SURF_SNOW;
    else if (b == MTN_BIOME_ROCK || b == MTN_BIOME_SCREE || mtn_terrain_trail_distance(&g->terrain, x, z) < 1.2f)
        *surface = MTN_SURF_ROCK;
    else *surface = MTN_SURF_GRASS;
    float w = mtn_terrain_water(&g->terrain, x, z);
    if (w != MTN_NO_WATER && w > h + 0.05f) *surface = MTN_SURF_WATER;
    float hs;
    if (mtn_terrain_trail_surface(&g->terrain, x, z, &hs) && hs > h && hs <= g->player.pos.y + 0.75f) {
        h = hs; /* the built-up path is level across */
        *n = v3(0, 1, 0);
        *surface = MTN_SURF_ROCK;
    }
    if (mtn_house_surface(&g->house, x, z, &hs) && hs > h - 0.05f && hs <= g->player.pos.y + 0.75f) {
        h = hs;
        *n = v3(0, 1, 0);
        *surface = MTN_SURF_WOOD;
    }
    return h;
}

static int world_push(void *ctx, vec3 *pos, float radius, float height) {
    mtn_game *g = ctx;
    (void)height;
    int hit = mtn_flora_collide(&g->flora, pos, radius);
    hit |= mtn_house_collide(&g->house, pos, radius);
    { /* the retaining wall of a built-up stretch of path is solid from below */
        vec3 c;
        if (mtn_terrain_trail_nearest(&g->terrain, pos->x, pos->z, &c) && c.y > pos->y + 0.75f) {
            float d = v3_dist_xz(c, *pos), min_d = MTN_TRAIL_WIDTH * 0.5f + radius;
            if (d < min_d && d > 1e-4f) {
                pos->x = c.x + (pos->x - c.x) / d * min_d;
                pos->z = c.z + (pos->z - c.z) / d * min_d;
                hit = 1;
            }
        }
    }
    for (int i = 0; i < g->terrain.cairn_count; i++) { /* cairns are solid */
        vec3 c = g->terrain.cairns[i];
        float d = v3_dist_xz(*pos, c), min_d = 0.55f + radius;
        if (d < min_d && pos->y < c.y + 1.4f) {
            vec3 dir = d > 1e-4f ? v3_scale(v3(pos->x - c.x, 0, pos->z - c.z), 1.0f / d) : v3(1, 0, 0);
            pos->x = c.x + dir.x * min_d;
            pos->z = c.z + dir.z * min_d;
            hit = 1;
        }
    }
    return hit;
}

/* Near the edge of the playable area outward movement is slowed down
 * progressively over the last 50 m, then clamped at the edge. */
static void soft_boundary(mtn_game *g, vec3 prev) {
    mtn_terrain *t = &g->terrain;
    float r = t->p.playable_radius * t->world_size * 0.5f;
    float d = v3_dist_xz(g->player.pos, t->center);
    if (d > r - 50.0f && d > 1e-3f) {
        vec3 out = v3_norm(v3(g->player.pos.x - t->center.x, 0, g->player.pos.z - t->center.z));
        float outward = v3_dot(v3_sub(g->player.pos, prev), out);
        if (outward > 0.0f) {
            float keep = mtn_clampf((r - d) / 50.0f, 0.0f, 1.0f);
            g->player.pos = v3_sub(g->player.pos, v3_scale(out, outward * (1.0f - keep)));
        }
    }
    mtn_terrain_clamp_playable(t, &g->player.pos);
}

static int world_walkable(void *ctx, float x, float z) {
    mtn_game *g = ctx;
    float w = mtn_terrain_water(&g->terrain, x, z);
    if (w != MTN_NO_WATER) {
        float hs;
        if (mtn_house_surface(&g->house, x, z, &hs)) return 1;
        if (w - mtn_terrain_height(&g->terrain, x, z) > 0.9f) return 0; /* wade, but never deep in */
    }
    return 1;
}

static float interior_ground(void *ctx, float x, float z, vec3 *n, int *surface) {
    (void)ctx; (void)x; (void)z;
    *n = v3(0, 1, 0);
    *surface = MTN_SURF_WOOD;
    return 0.0f;
}

static int interior_push(void *ctx, vec3 *pos, float radius, float height) {
    mtn_game *g = ctx;
    (void)height;
    int hit = 0;
    for (int i = 0; i < MTN_MAX_ROOMS; i++)
        if (g->rooms[i].built) hit |= mtn_room_collide(&g->rooms[i], pos, radius);
    /* the front door: closed it is a wall, open it lets you out */
    float dw = MTN_DOOR_WIDTH * 0.5f;
    int open = g->house.door_open && g->house.door_angle > 0.8f;
    if (pos->z < radius && pos->z > -0.5f && fabsf(pos->x) < dw + radius && (!open || fabsf(pos->x) > dw - 0.05f)) {
        pos->z = radius;
        hit = 1;
    }
    return hit;
}

const mtn_room *mtn_game_room_at(const mtn_game *g, vec3 p) {
    for (int i = 0; i < MTN_MAX_ROOMS; i++)
        if (g->rooms[i].built && mtn_room_contains(&g->rooms[i], p)) return &g->rooms[i];
    return NULL;
}

static int interior_walkable(void *ctx, float x, float z) {
    mtn_game *g = ctx;
    if (mtn_game_room_at(g, v3(x, 0, z))) return 1;
    return z < 0.5f && z > -1.0f && fabsf(x) < MTN_DOOR_WIDTH * 0.5f; /* stepping out of the front door */
}

/* ------------------------------------------------------------------ house */

void mtn_game_refresh_rooms(mtn_game *g, int force) {
    int ci = 0, cj = 0;
    if (g->interior) mtn_house_cell_at(&g->house, g->player.pos, &ci, &cj);
    int any = 0;
    for (int i = 0; i < MTN_MAX_ROOMS; i++) any |= g->rooms[i].built;
    if (!force && any && ci == g->room_ci && cj == g->room_cj) return;
    g->room_ci = ci;
    g->room_cj = cj;
    /* the cells within two steps; keep the rooms already built there */
    int need[MTN_MAX_ROOMS][2], nn = 0, have[MTN_MAX_ROOMS] = {0};
    for (int dj = -2; dj <= 2; dj++)
        for (int di = -2; di <= 2; di++)
            if (abs(di) + abs(dj) <= 2) {
                need[nn][0] = ci + di;
                need[nn][1] = cj + dj;
                nn++;
            }
    int keep[MTN_MAX_ROOMS] = {0};
    for (int s = 0; s < MTN_MAX_ROOMS && !force; s++) {
        if (!g->rooms[s].built) continue;
        for (int k = 0; k < nn; k++)
            if (!have[k] && g->rooms[s].ci == need[k][0] && g->rooms[s].cj == need[k][1]) {
                have[k] = 1;
                keep[s] = 1;
                break;
            }
    }
    for (int k = 0; k < nn; k++) {
        if (have[k]) continue;
        int s = 0;
        while (s < MTN_MAX_ROOMS && keep[s]) s++;
        if (s >= MTN_MAX_ROOMS) break;
        keep[s] = 1;
        mtn_mesh_clear(&g->room_solid[s]);
        mtn_mesh_clear(&g->room_sprites[s]);
        mtn_mesh_clear(&g->room_glow[s]);
        mtn_house_build_room(&g->house, &g->terrain, need[k][0], need[k][1], &g->rooms[s], &g->room_solid[s], &g->room_sprites[s],
                             &g->room_glow[s]);
        g->room_slot_version[s]++;
    }
    g->rooms_version++;
}

int mtn_game_summon_house(mtn_game *g) {
    if (!g->has_world) return 0;
    if (g->interior) {
        mtn_game_message(g, "You are already inside the house.", 3.0f);
        return 0;
    }
    if (mtn_house_exists(&g->house) && v3_dist_xz(g->player.pos, g->house.origin) <= MTN_HOUSE_UNSUMMON_DIST) {
        mtn_game_message(g, "The house is right here.", 2.5f);
        return 0;
    }
    mtn_house_site site;
    if (!mtn_house_find_site(&g->terrain, &g->flora, g->player.pos, g->player.yaw, &site)) {
        char msg[128];
        snprintf(msg, sizeof msg, "The house can't settle here: %s.", site.reason);
        mtn_game_message(g, msg, 4.0f);
        return 0;
    }
    mtn_flora_unhide_all(&g->flora);
    uint32_t seed = g->seed * 31u + (uint32_t)g->house.version * 7u + 1u;
    mtn_house_summon(&g->house, &site, seed);
    mtn_flora_hide_radius(&g->flora, site.pos, 6.5f, site.variant == MTN_HV_TREE ? site.tree : -1);
    rebuild_house_meshes(g);
    mtn_game_refresh_rooms(g, 1);
    mtn_game_play(g, "summon", 0.8f);
    return 1;
}

int mtn_game_unsummon_house(mtn_game *g) {
    if (g->interior) {
        mtn_game_message(g, "The house can only be sent away from outside.", 3.0f);
        return 0;
    }
    if (!mtn_house_can_unsummon(&g->house, g->player.pos)) return 0;
    mtn_house_unsummon(&g->house);
    mtn_game_play(g, "unsummon", 0.8f);
    return 1;
}

int mtn_game_interact(mtn_game *g) {
    if (!mtn_house_exists(&g->house) || g->house.state != MTN_HOUSE_PRESENT) return 0;
    float d;
    if (g->interior) {
        vec3 p = g->player.pos;
        d = sqrtf(p.x * p.x + p.z * p.z);
    } else {
        d = mtn_house_door_distance(&g->house, g->player.pos);
    }
    if (d > 3.0f) return 0;
    mtn_house_toggle_door(&g->house);
    mtn_game_play(g, g->house.door_open ? "door_open" : "door_close", 0.7f);
    return 1;
}

int mtn_game_house_entrance(mtn_game *g) {
    if (!g->interior) return 0;
    g->player.pos = v3(0, 0, 1.5f);
    g->player.vel = v3(0, 0, 0);
    g->player.yaw = 0.0f; /* facing -z: the front door */
    g->player.pitch = 0.0f;
    mtn_game_refresh_rooms(g, 0);
    mtn_game_play(g, "summon", 0.4f);
    return 1;
}

static void enter_house(mtn_game *g) {
    mat4 m = mtn_house_ext_to_int(&g->house);
    g->house_spot = g->player.pos;
    g->player.pos = m4_mul_point(m, g->player.pos);
    g->player.pos.y = 0.0f;
    vec3 f = m4_mul_dir(m, v3(sinf(g->player.yaw), 0, -cosf(g->player.yaw)));
    g->player.yaw = atan2f(f.x, -f.z);
    g->player.vel = m4_mul_dir(m, g->player.vel);
    g->interior = 1;
    g->house.inside = 1;
    mtn_game_refresh_rooms(g, 1);
}

static void leave_house(mtn_game *g) {
    mat4 m = m4_inverse(mtn_house_ext_to_int(&g->house));
    g->player.pos = m4_mul_point(m, g->player.pos);
    g->player.pos.y = g->house.floor_y;
    vec3 f = m4_mul_dir(m, v3(sinf(g->player.yaw), 0, -cosf(g->player.yaw)));
    g->player.yaw = atan2f(f.x, -f.z);
    g->player.vel = m4_mul_dir(m, g->player.vel);
    g->interior = 0;
    g->house.inside = 0;
}

/* ------------------------------------------------------------------ update */

static void handle_weather_events(mtn_game *g) {
    mtn_weather_event ev;
    while ((ev = mtn_weather_pop_event(&g->weather)) != MTN_WX_EVENT_NONE)
        if (ev == MTN_WX_EVENT_THUNDER) {
            /* thunder rolls in from a random direction */
            float a = (float)(g->frame % 360) * MTN_DEG2RAD;
            vec3 at = v3_add(g->player.pos, v3(cosf(a) * 15.0f, 0, sinf(a) * 15.0f));
            mtn_game_play_at(g, "thunder", at, g->interior ? 0.6f : 1.4f);
        }
}

static void footstep(mtn_game *g) {
    static const char *names[] = {"step_grass", "step_rock", "step_snow", "step_wood", "step_grass"};
    char snd[32];
    snprintf(snd, sizeof snd, "%s_%d", names[mtn_clampi(g->player.surface, 0, 4)], 1 + (g->frame & 1));
    mtn_game_play(g, snd, 0.35f);
}

void mtn_game_update(mtn_game *g, float dt) {
    if (dt > 0.1f) dt = 0.1f; /* hitches never launch the player */
    g->dt = dt;
    g->real_time += dt;
    g->frame++;
    if (dt > 0.0f) g->fps = g->fps * 0.95f + 0.05f / dt;

    for (int a = 0; a < MTN_ACT_COUNT; a++) {
        if (mtn_input_pressed(&g->input, a)) mtn_script_action(g, mtn_action_name(a), 1);
        if (mtn_input_released(&g->input, a)) mtn_script_action(g, mtn_action_name(a), 0);
    }
    if (g->reload_request) {
        g->reload_request = 0;
        mtn_script_reload(g);
    }
    mtn_script_update(g, dt);

    if (g->has_world) {
        mtn_clock_update(&g->clock, &g->planet, dt);
        float yf = mtn_clock_year_frac(&g->clock, &g->planet);
        float hour = mtn_clock_hour(&g->clock);
        g->season = mtn_season_blend_at(yf);
        float gs = mtn_clock_game_seconds(&g->clock, &g->planet, dt);
        mtn_weather_update(&g->weather, gs, dt, g->season, hour);
        handle_weather_events(g);
        mtn_sky_compute(&g->sky, &g->planet, &g->clock, &g->weather);
        g->snowline = mtn_game_snowline(g);
        float snow_cover = mtn_maxf(g->weather.snow_ground, 1.0f - mtn_smoothstep(0.15f, 0.45f, g->snowline));
        mtn_flora_season_eval(&g->flora, g->season, snow_cover, &g->fseason);

        int was_state = g->house.state;
        mtn_house_update(&g->house, dt);
        if (was_state != MTN_HOUSE_NONE && g->house.state == MTN_HOUSE_NONE) {
            mtn_flora_unhide_all(&g->flora);
            rebuild_house_meshes(g);
        }

        mtn_move_input mi;
        memset(&mi, 0, sizeof mi);
        if (!g->ui_modal) {
            mtn_input_move(&g->input, &mi.move_x, &mi.move_y);
            mi.look_x = g->input.look_x;
            mi.look_y = g->settings.invert_y ? -g->input.look_y : g->input.look_y;
            mi.run = mtn_input_down(&g->input, MTN_ACT_RUN) || g->input.analog_run;
            mi.jump = mtn_input_pressed(&g->input, MTN_ACT_JUMP);
            if (mtn_input_pressed(&g->input, MTN_ACT_TURN_LEFT)) mi.look_x -= MTN_PI / 6.0f;
            if (mtn_input_pressed(&g->input, MTN_ACT_TURN_RIGHT)) mi.look_x += MTN_PI / 6.0f;
        }
        mtn_collider col;
        col.ctx = g;
        if (g->interior) {
            col.ground = interior_ground;
            col.push_out = interior_push;
            col.walkable = interior_walkable;
        } else {
            col.ground = world_ground;
            col.push_out = world_push;
            col.walkable = world_walkable;
        }
        vec3 prev = g->player.pos;
        mtn_player_update(&g->player, &mi, &col, dt);
        if (!g->interior) {
            soft_boundary(g, prev);
            if (mtn_house_crossed_in(&g->house, prev, g->player.pos)) enter_house(g);
        } else {
            vec3 p = g->player.pos;
            if (g->house.door_open && prev.z >= -0.25f && p.z < -0.25f && fabsf(p.x) < MTN_DOOR_WIDTH * 0.5f + 0.2f) leave_house(g);
            else mtn_game_refresh_rooms(g, 0);
        }
        if (g->player.footstep) footstep(g);

        if (!g->interior) {
            float precip = g->weather.precipitation;
            mtn_fauna_update(&g->fauna, &g->terrain, g->player.pos, hour, g->season, precip, dt);
            const char *call;
            while ((call = mtn_fauna_pop_call(&g->fauna))[0]) mtn_game_play_at(g, call, g->fauna.last_call_pos, 0.9f);
        }
        float alt = g->interior ? mtn_terrain_altitude_frac(&g->terrain, g->house.floor_y)
                                : mtn_terrain_altitude_frac(&g->terrain, g->player.pos.y);
        g->temperature = mtn_temperature(g->season, hour, alt);
    }
    g->photo_flash = mtn_maxf(0.0f, g->photo_flash - dt * 3.0f);
    if (g->message_time > 0.0f) g->message_time -= dt;
    g->mixer.master = g->settings.volume;
}
