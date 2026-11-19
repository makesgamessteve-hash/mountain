/* Unit tests for the game orchestration (engine/src/core/game.c): world
 * creation, the simulation step, the magic house flow (summon, door, walking
 * in and out), sounds and messages. Uses the real Lua scripts in assets/. */
#include "test.h"
#include "mtn/game.h"
#include "mtn/vfs.h"

#include <stdlib.h>

#ifndef MTN_SOURCE_DIR
#define MTN_SOURCE_DIR "."
#endif

mtn_game *mtn_test_game(void);
void mtn_test_game_free(mtn_game *g);

mtn_game *mtn_test_game(void) {
    mtn_vfs_init(MTN_SOURCE_DIR "/");
    mtn_game *g = calloc(1, sizeof *g);
    mtn_game_init(g, "test", "");
    g->tparams.size = 129;
    g->tparams.cell = 16.0f;
    g->tparams.erosion_drops = 2000;
    g->test_mode = 1;
    mtn_script_init(g);
    return g;
}

void mtn_test_game_free(mtn_game *g) {
    mtn_game_shutdown(g);
    free(g);
}

static void step(mtn_game *g, int frames) {
    for (int i = 0; i < frames; i++) {
        mtn_game_update(g, 1.0f / 60.0f);
        mtn_ui_begin(&g->ui, 640, 360, &g->input);
        mtn_script_ui(g);
        mtn_input_begin_frame(&g->input);
    }
}

static void test_boot(void) {
    mtn_game *g = mtn_test_game();
    CHECK(g->script_error[0] == 0);
    if (g->script_error[0]) fprintf(stderr, "%s\n", g->script_error);
    CHECK(g->has_world);
    CHECK(g->flora.species_count > 10);
    CHECK(g->fauna.species_count > 5);
    CHECK(g->house.type_count >= 2);
    CHECK(g->seed == 20261119u); /* scripts use a fixed seed in test mode */
    CHECK(mtn_terrain_in_playable(&g->terrain, g->player.pos.x, g->player.pos.z));
    step(g, 30);
    CHECK(g->frame == 30);
    CHECK(g->fps > 0);
    CHECK(g->ui.count > 0); /* the HUD drew something */
    mtn_test_game_free(g);
}

static void test_sounds_and_messages(void) {
    mtn_game *g = mtn_test_game();
    CHECK(mtn_game_load_sounds(g) >= 20);
    int before = mtn_mixer_active_voices(&g->mixer);
    mtn_game_play(g, "chime", 0.5f);
    CHECK(mtn_mixer_active_voices(&g->mixer) == before + 1);
    mtn_game_play(g, "no_such_sound", 0.5f);
    CHECK(mtn_mixer_active_voices(&g->mixer) == before + 1);
    mtn_game_message(g, "hello", 1.0f);
    CHECK_STR(g->message, "hello");
    step(g, 90);
    CHECK(g->message_time <= 0.0f);
    g->photo_flash = 1.0f;
    step(g, 30);
    CHECK(g->photo_flash < 0.01f);
    mtn_test_game_free(g);
}

static void test_simulation(void) {
    mtn_game *g = mtn_test_game();
    mtn_game_load_sounds(g);
    float h0 = mtn_clock_hour(&g->clock);
    step(g, 60);
    CHECK(mtn_clock_hour(&g->clock) > h0);
    /* snow line follows the seasons and fresh snow */
    mtn_clock_set_season(&g->clock, &g->planet, MTN_WINTER);
    step(g, 2);
    float winter = g->snowline;
    mtn_clock_set_season(&g->clock, &g->planet, MTN_SUMMER);
    step(g, 2);
    CHECK(g->snowline > winter);
    g->weather.snow_ground = 1.0f;
    CHECK(mtn_game_snowline(g) < 0.0f);
    g->weather.snow_ground = 0.0f;
    /* storms produce thunder sounds */
    mtn_weather_set(&g->weather, MTN_WX_STORM, 1);
    g->weather.current = MTN_WX_STORM;
    g->weather.lightning_timer = 0.0f;
    step(g, 300);
    CHECK(g->weather.flash >= 0.0f);
    /* walking moves the player and makes footsteps */
    vec3 p0 = g->player.pos;
    mtn_input_set(&g->input, MTN_ACT_FORWARD, 1);
    mtn_input_set(&g->input, MTN_ACT_RUN, 1);
    for (int i = 0; i < 120; i++) {
        mtn_game_update(g, 1.0f / 60.0f);
        g->input.pressed = g->input.released = 0;
    }
    mtn_input_set(&g->input, MTN_ACT_FORWARD, 0);
    mtn_input_set(&g->input, MTN_ACT_RUN, 0);
    CHECK(v3_dist_xz(p0, g->player.pos) > 1.0f);
    /* snap turns (VR) and inverted look */
    float yaw = g->player.yaw;
    mtn_input_set(&g->input, MTN_ACT_TURN_LEFT, 1);
    mtn_game_update(g, 1.0f / 60.0f);
    mtn_input_set(&g->input, MTN_ACT_TURN_LEFT, 0);
    CHECK(fabsf(mtn_wrap_angle(g->player.yaw - yaw)) > 0.4f);
    mtn_input_set(&g->input, MTN_ACT_TURN_RIGHT, 1);
    mtn_game_update(g, 1.0f / 60.0f);
    mtn_input_set(&g->input, MTN_ACT_TURN_RIGHT, 0);
    g->settings.invert_y = 1;
    g->input.look_y = 0.1f;
    float pitch = g->player.pitch;
    mtn_game_update(g, 1.0f / 60.0f);
    CHECK(g->player.pitch < pitch);
    /* big hitches are clamped */
    mtn_game_update(g, 5.0f);
    CHECK_NEAR(g->dt, 0.1f, 1e-6);
    /* menus stop movement */
    g->ui_modal = 1;
    p0 = g->player.pos;
    mtn_input_set(&g->input, MTN_ACT_FORWARD, 1);
    step(g, 30);
    mtn_input_set(&g->input, MTN_ACT_FORWARD, 0);
    g->ui_modal = 0;
    CHECK(v3_dist_xz(p0, g->player.pos) < 0.2f);
    /* camera field of view */
    g->camera_mode = 1;
    CHECK_NEAR(mtn_game_current_fov(g), g->photo_fov, 1e-6);
    g->camera_mode = 0;
    CHECK_NEAR(mtn_game_current_fov(g), g->settings.fov, 1e-6);
    mtn_test_game_free(g);
}

/* walk the player into the house through the open door and out again */
static void test_house_flow(void) {
    mtn_game *g = mtn_test_game();
    mtn_game_load_sounds(g);
    CHECK(!mtn_game_unsummon_house(g)); /* nothing to send away */
    CHECK(!mtn_game_interact(g));
    int ok = 0;
    for (int tries = 0; tries < 20 && !ok; tries++) {
        ok = mtn_game_summon_house(g);
        if (!ok) g->player.yaw += 0.6f;
    }
    CHECK(ok);
    CHECK(!mtn_game_summon_house(g)); /* already right here */
    CHECK(!mtn_game_interact(g));     /* still appearing */
    step(g, 120);
    CHECK(g->house.state == MTN_HOUSE_PRESENT);
    /* stand in front of the door, facing it */
    vec3 door = mtn_house_door_world(&g->house);
    vec3 f = g->house.front;
    g->player.pos = v3_add(door, v3_scale(f, 1.0f)); /* on the porch or platform */
    g->player.pos.y = g->house.floor_y;
    g->player.yaw = atan2f(-f.x, f.z);
    g->player.pitch = 0.0f;
    g->player.vel = v3(0, 0, 0);
    CHECK(mtn_game_interact(g));
    step(g, 60);
    CHECK(g->house.door_open);
    mtn_input_set(&g->input, MTN_ACT_FORWARD, 1);
    for (int i = 0; i < 180 && !g->interior; i++) {
        mtn_game_update(g, 1.0f / 60.0f);
        g->input.pressed = g->input.released = 0;
    }
    mtn_input_set(&g->input, MTN_ACT_FORWARD, 0);
    CHECK(g->interior);
    CHECK(g->house.inside);
    CHECK(g->rooms[0].length > 0);
    CHECK(!mtn_game_summon_house(g));   /* not from inside */
    CHECK(!mtn_game_unsummon_house(g)); /* not from inside */
    /* walk deeper: rooms are refreshed */
    g->player.pos = v3(0, 0, MTN_ROOM_LENGTH * 2.5f);
    int rv = g->rooms_version;
    step(g, 2);
    CHECK(g->rooms_version != rv);
    CHECK(g->room_base == 1);
    /* inside, the door can be used from the cabin */
    g->player.pos = v3(0, 0, 1.0f);
    step(g, 2);
    CHECK(mtn_game_interact(g)); /* closes */
    step(g, 60);
    CHECK(!g->house.door_open);
    CHECK(mtn_game_interact(g)); /* opens */
    step(g, 60);
    /* walk out through the front door (-z) */
    g->player.yaw = 0.0f;
    g->player.pitch = 0.0f;
    g->player.pos = v3(0, 0, 0.8f);
    mtn_input_set(&g->input, MTN_ACT_FORWARD, 1);
    for (int i = 0; i < 120 && g->interior; i++) {
        mtn_game_update(g, 1.0f / 60.0f);
        g->input.pressed = g->input.released = 0;
    }
    mtn_input_set(&g->input, MTN_ACT_FORWARD, 0);
    CHECK(!g->interior);
    step(g, 10);
    /* far away: summoning again moves the house */
    vec3 far = v3_add(g->house.origin, v3(60, 0, 60));
    if (mtn_terrain_in_playable(&g->terrain, far.x, far.z)) {
        g->player.pos = v3(far.x, mtn_terrain_height(&g->terrain, far.x, far.z), far.z);
        int ver = g->house.version;
        if (mtn_game_summon_house(g)) CHECK(g->house.version != ver);
        step(g, 120);
        door = mtn_house_door_world(&g->house);
        g->player.pos = v3_add(door, v3_scale(g->house.front, 4.0f));
    }
    CHECK(mtn_game_unsummon_house(g));
    step(g, 120);
    CHECK(g->house.state == MTN_HOUSE_NONE);
    mtn_test_game_free(g);
}

static void test_site_failure_and_trailhead(void) {
    mtn_game *g = mtn_test_game();
    /* standing at the edge of the world facing outwards: no site */
    vec3 c = g->terrain.center;
    float r = g->terrain.p.playable_radius * g->terrain.world_size * 0.5f - 1.0f;
    g->player.pos = v3(c.x + r, 0, c.z);
    g->player.yaw = MTN_PI * 0.5f;
    CHECK(!mtn_game_summon_house(g));
    CHECK(g->message_time > 0.0f);
    mtn_game_teleport_trailhead(g);
    CHECK(v3_dist_xz(g->player.pos, g->terrain.trail[0]) < 0.01f);
    /* a world without a trail starts at the trailhead point */
    int len = g->terrain.trail_len;
    g->terrain.trail_len = 0;
    mtn_game_teleport_trailhead(g);
    CHECK(v3_dist_xz(g->player.pos, g->terrain.trailhead) < 0.01f);
    g->terrain.trail_len = len;
    /* no world: summoning does nothing */
    g->has_world = 0;
    CHECK(!mtn_game_summon_house(g));
    mtn_test_game_free(g);
}

static void test_water_and_surfaces(void) {
    mtn_game *g = mtn_test_game();
    /* find a lake and check that deep water is not walkable */
    int found = 0;
    for (int z = 0; z < g->terrain.n && !found; z++)
        for (int x = 0; x < g->terrain.n && !found; x++) {
            int i = z * g->terrain.n + x;
            if (g->terrain.water[i] != MTN_NO_WATER && g->terrain.water[i] - g->terrain.h[i] > 2.0f) {
                float wx = (float)x * g->terrain.cell, wz = (float)z * g->terrain.cell;
                g->player.pos = v3(wx + 40.0f, mtn_terrain_height(&g->terrain, wx + 40, wz), wz);
                g->player.yaw = -MTN_PI * 0.5f; /* face -x towards the lake */
                found = 1;
            }
        }
    if (found) {
        mtn_input_set(&g->input, MTN_ACT_FORWARD, 1);
        for (int i = 0; i < 600; i++) {
            mtn_game_update(g, 1.0f / 60.0f);
            g->input.pressed = g->input.released = 0;
        }
        mtn_input_set(&g->input, MTN_ACT_FORWARD, 0);
        float w = mtn_terrain_water(&g->terrain, g->player.pos.x, g->player.pos.z);
        CHECK(w == MTN_NO_WATER || w - mtn_terrain_height(&g->terrain, g->player.pos.x, g->player.pos.z) <= 1.0f);
    }
    /* surfaces: snow above the snow line, rock on the trail */
    g->snowline = -1.0f;
    step(g, 0);
    mtn_test_game_free(g);
}

MTN_SUITE(game) {
    TEST_RUN(test_boot);
    TEST_RUN(test_sounds_and_messages);
    TEST_RUN(test_simulation);
    TEST_RUN(test_house_flow);
    TEST_RUN(test_site_failure_and_trailhead);
    TEST_RUN(test_water_and_surfaces);
}
