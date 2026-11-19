/* Unit tests for the Lua scripting layer (engine/src/script/lua_api.c):
 * every API function is called from Lua with assertions, plus error
 * handling, the restricted environment, storage and reloading. */
#include "test.h"
#include "mtn/game.h"
#include "mtn/vfs.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef MTN_BINARY_DIR
#define MTN_BINARY_DIR "."
#endif

mtn_game *mtn_test_game(void);
void mtn_test_game_free(mtn_game *g);

static int run(mtn_game *g, const char *code) {
    g->script_error[0] = 0;
    int ok = mtn_script_run_string(g, code, "=test");
    if (!ok) fprintf(stderr, "  lua: %s\n", g->script_error);
    return ok;
}

static void test_world_time_weather(void) {
    mtn_game *g = mtn_test_game();
    CHECK(run(g, "assert(engine.platform() == 'test'); assert(engine.version():match('%d+%.%d+'))\n"
                 "assert(engine.test_mode()); assert(engine.frame() >= 0); assert(engine.time() >= 0)\n"
                 "assert(engine.fps() >= 0); local s, a = engine.xr(); assert(s == false and a == false)\n"
                 "print('log', 1, true, nil)\n"
                 "assert(world.exists() and world.seed() == 20261119)\n"
                 "local x, y, z = world.summit(); assert(world.height(x, z) > 100)\n"
                 "assert(type(world.biome(x, z)) == 'string'); assert(world.slope(x, z) >= 0)\n"
                 "local tx, ty, tz = world.trailhead(); assert(ty < y)\n"
                 "local i = world.info(); assert(i.plants > 0 and i.viewpoints > 0 and i.size > 1000)\n"
                 "assert(world.altitude(y) > 0.8); local vx, vy, vz, vyaw = world.viewpoint(1); assert(vx and vyaw)\n"
                 "assert(world.viewpoint(999) == nil)\n"
                 "local w = nil; for zz = 0, i.size, 16 do for xx = 0, i.size, 16 do w = w or world.water(xx, zz) end end\n"));
    CHECK(run(g, "time.set_hour(13.5); assert(math.abs(time.hour() - 13.5) < 0.01)\n"
                 "assert(time.day() >= 1); assert(time.speed(10) == 10); assert(time.speed() == 10)\n"
                 "assert(time.pause(true) == true); assert(time.pause() == true); time.pause(false)\n"
                 "assert(time.set_season('winter')); local s, b = time.season(); assert(s == 'winter' and b.winter > 0.5)\n"
                 "assert(not time.set_season('monsoon')); assert(time.year() >= 0 and time.year() < 1)\n"
                 "assert(time.daylight() >= 0); local ph, lit = time.moon(); assert(ph >= 0 and lit >= 0)\n"
                 "assert(time.day_length() > 6); local sx, sy, sz = time.sun(); assert(sx and sy and sz)\n"
                 "planet.set{ name = 'Twin', day_length = 600, days_per_season = 3, latitude = 10, axial_tilt = 30,\n"
                 "  sun_size = 0.03, sun_color = {255, 240, 220}, day_zenith = {0.1, 0.2, 0.9}, day_horizon = {0.5, 0.6, 0.9},\n"
                 "  sunset_horizon = {1, 0.4, 0.2}, night_zenith = {0, 0, 0.05}, night_horizon = {0, 0, 0.1},\n"
                 "  moons = { {period = 5, phase = 0.5, size = 0.05, color = {1, 0.8, 0.8}}, {period = 3} } }\n"
                 "assert(planet.name() == 'Twin')\n"));
    CHECK(g->planet.moon_count == 2);
    CHECK_NEAR(g->planet.sun_color.x, 1.0f, 1e-6);
    CHECK(run(g, "weather.set('rain', true, true); local n, w = weather.get(); assert(n == 'rain' and w.locked)\n"
                 "weather.unlock(); weather.snow(0.5); assert(select(2, weather.get()).snow_ground == 0.5)\n"
                 "weather.set('fog')\n"));
    CHECK(!run(g, "weather.set('hail')"));
    CHECK(strstr(g->script_error, "unknown weather") != NULL);
    mtn_test_game_free(g);
}

static void test_species_player_house(void) {
    mtn_game *g = mtn_test_game();
    int species = g->flora.species_count;
    CHECK(run(g, "local i = flora.species{ name = 'palm', kind = 'tree', crown = 'umbrella', tiles = {31}, biomes = {'meadow'},\n"
                 "  colors = { summer = {0.2, 0.6, 0.2} }, bloom = {'summer'}, blossom_tile = 28 }\n"
                 "assert(i >= 0); local n, sp = flora.count(); assert(sp > 0)\n"
                 "local x, y, z = player.pos(); assert(flora.near(x, z, 50) >= 0)\n"
                 "flora.species{ name = 'shrub', kind = 'bush', crown = 'cone' }\n"));
    CHECK(g->flora.species_count == species + 2);
    CHECK(g->flora.species[species].crown == MTN_CROWN_UMBRELLA);
    CHECK(run(g, "local i = fauna.species{ name = 'yak', kind = 'quadruped', horns = 2, biomes = {'alpine'}, seasons = {'summer'},\n"
                 "  sound = 'marmot', from = 6, to = 20 }\n"
                 "assert(i >= 0); assert(fauna.count() >= 0); assert(fauna.count('yak') == 0); assert(fauna.count('dragon') == 0)\n"
                 "local x, y, z = world.summit(); assert(fauna.spawn('yak', x, z) >= 0); assert(fauna.spawn('dragon', x, z) == 0)\n"
                 "for _, k in ipairs({'bird', 'soaring', 'butterfly', 'firefly'}) do fauna.species{ name = k, kind = k } end\n"));
    CHECK(run(g, "local x, y, z = player.pos(); player.set_pos(x + 1, y, z); local x2 = player.pos(); assert(x2 == x + 1)\n"
                 "local yaw, pitch = player.yaw(1.0, 0.1); assert(yaw == 1.0 and math.abs(pitch - 0.1) < 1e-6)\n"
                 "player.trailhead(); assert(not player.inside()); assert(type(player.surface()) == 'string')\n"
                 "assert(player.altitude() > 0); local w, r = player.speeds(3, 6); assert(w == 3 and r == 6)\n"));
    CHECK(run(g, "assert(house.distance() == nil); assert(house.door_pos() == nil)\n"
                 "local ok = false; for i = 1, 30 do ok = house.summon(); if ok then break end; player.yaw(i * 0.5, 0) end\n"
                 "assert(ok, 'summoned'); assert(house.exists()); local st, v = house.state(); assert(st == 'appearing')\n"
                 "local d, dd = house.distance(); assert(d and dd); local dx, dy, dz, dyaw = house.door_pos(); assert(dyaw)\n"
                 "assert(house.room() == -1); assert(house.interact() == false)\n"
                 "local open, angle = house.door(); assert(open == false and angle == 0)\n"
                 "assert(type(house.can_unsummon()) == 'boolean')\n"));
    for (int i = 0; i < 120; i++) mtn_game_update(g, 1.0f / 60.0f);
    /* inside the house: room names */
    g->interior = 1;
    g->player.pos = v3(0, 0, 4);
    CHECK(run(g, "local i, n = house.room(); assert(i == 0 and n == 'cabin'); local d = house.distance(); assert(d == 0)\n"
                 "assert(house.unsummon() == false)\n"));
    g->interior = 0;
    CHECK(run(g, "house.clear_rooms(); local t = house.define_room{ name = 'attic', width = 30, height = 1, light = {1, 1, 1},\n"
                 "  wall_color = {0.5, 0.5, 0.5}, floor_color = {200, 180, 160}, cabin = true,\n"
                 "  items = { {'bed', x = 1, z = 2, rot = 90}, {prefab = 'lamp', x = 0, z = 1, arg = 2, scale = 1.5} } }\n"
                 "assert(t == 0)\n"));
    CHECK(g->house.type_count == 1);
    CHECK_NEAR(g->house.types[0].width, 16.0f, 1e-6);
    CHECK(g->house.types[0].item_count == 2);
    mtn_test_game_free(g);
}

static void test_camera_photos_ui_audio(void) {
    mtn_game *g = mtn_test_game();
    mtn_game_load_sounds(g);
    mtn_gallery_add(&g->gallery, "mountain-1-20260101-101010.png");
    CHECK(run(g, "assert(camera.mode(true) == true); assert(camera.mode() == true); camera.mode(false)\n"
                 "assert(camera.fov(5) == 20); assert(camera.fov(200) == 90); assert(camera.fov() == 90)\n"
                 "assert(camera.filter('sepia') == 'sepia'); assert(camera.filter('nope') == 'sepia'); camera.shoot()\n"
                 "assert(photos.count() == 1); assert(photos.name(1):match('^mountain')); assert(photos.name(2) == nil)\n"
                 "photos.download(1); photos.delete(1); assert(photos.dir():match('photos$'))\n"));
    CHECK(g->photo_request == 1);
    CHECK(g->download_request == 0);
    CHECK(g->delete_request == 0);
    mtn_ui_begin(&g->ui, 960, 540, &g->input);
    CHECK(run(g, "local w, h = ui.size(); assert(w > 100 and h > 100)\n"
                 "assert(ui.text(1, 1, 'hi', {1, 0, 0, 0.5}, 2) == 32); assert(ui.text(1, 1, 'x', 0xFFFFFFFF) == 8)\n"
                 "assert(ui.text_width('abcd') == 32); ui.rect(0, 0, 1, 1); ui.rect(0, 0, 1, 1, {255, 0, 0, 128})\n"
                 "ui.icon(3, 1, 1); ui.icon(3, 1, 1, 32, {1, 1, 1}); assert(ui.button('b', 0, 0, 10, 10, 'ok', 2) == false)\n"
                 "assert(ui.button('c', 0, 0, 10, 10) == false); ui.photo(1, 0, 0, 16, 9)\n"
                 "assert(ui.modal(true) == true); assert(ui.modal() == true); ui.modal(false)\n"
                 "ui.message('note', 5); local m, t = ui.message(); assert(m == 'note' and t == 5)\n"
                 "local px, py, down = ui.pointer(); assert(px and py and down == false); assert(ui.error() == nil)\n"));
    /* a click on a button: press and release over it */
    mtn_input_pointer(&g->input, 10, 10, 1);
    mtn_ui_begin(&g->ui, 960, 540, &g->input);
    run(g, "ui.button('click', 0, 0, 40, 40, 'go')");
    mtn_input_begin_frame(&g->input);
    mtn_input_pointer(&g->input, 10, 10, 0);
    mtn_ui_begin(&g->ui, 960, 540, &g->input);
    CHECK(run(g, "assert(ui.button('click', 0, 0, 40, 40, 'go') == true)"));
    CHECK(run(g, "assert(input.mode('touch') == 'touch'); assert(input.mode() == 'touch'); input.mode('keyboard')\n"
                 "assert(input.down('jump') == false); input.bind('jump', 'J')\n"
                 "assert(audio.play('chime', 0.5, -0.5, 1.2) == true); assert(audio.play('nothing') == false)\n"
                 "audio.ambient('wind_loop', 0.5, 1); audio.ambient('wind_loop', 0); assert(audio.loaded() > 10)\n"));
    CHECK(g->binding_count == 1);
    CHECK(!run(g, "input.bind('fly', 'F')"));
    for (int i = 0; i < MTN_MAX_BINDINGS + 2; i++) run(g, "input.bind('jump', 'K')");
    CHECK(g->binding_count == MTN_MAX_BINDINGS);
    CHECK(run(g, "local s = engine.settings{ mouse_sensitivity = 2, invert_y = true, volume = 3, pixel_lines = 270, fov = 10, show_fps = true }\n"
                 "assert(s.volume == 1 and s.pixel_lines == 270 and s.fov == 50 and s.invert_y)\n"
                 "engine.settings{ pixel_lines = 123 }; assert(engine.settings().pixel_lines == 270)\n"
                 "engine.fullscreen(true); engine.reload(); engine.quit()\n"));
    CHECK(g->fullscreen_request == 1 && g->reload_request == 1 && g->quit_request == 1);
    run(g, "engine.fullscreen(false)");
    CHECK(g->fullscreen_request == 2);
    mtn_test_game_free(g);
}

static void test_storage_and_restrictions(void) {
    char dir[256];
    snprintf(dir, sizeof dir, "%s/mtn_script_test_%d_", MTN_BINARY_DIR, (int)rand());
    mtn_game *g = mtn_test_game();
    snprintf(g->user_dir, sizeof g->user_dir, "%s", dir);
    CHECK(run(g, "assert(storage.save('settings', 'return 1') == true); assert(storage.load('settings') == 'return 1')\n"
                 "assert(storage.save('../evil', 'x') == false); assert(storage.load('bad name') == nil)\n"
                 "assert(storage.load('missing') == nil)\n"));
    CHECK(g->storage_dirty);
    char path[400];
    snprintf(path, sizeof path, "%ssettings.lua", dir);
    remove(path);
    CHECK(run(g, "assert(dofile == nil and loadfile == nil); assert(io == nil); assert(os.execute == nil)\n"
                 "assert(os.time() > 0); assert(os.clock() >= 0); assert(#os.date('%Y') == 4)\n"
                 "local u = require('util'); assert(u == require('util')); assert(u.clock_string(9.5) == '09:30')\n"
                 "assert(not pcall(require, 'no_such_module'))\n"));
    mtn_test_game_free(g);
}

static void test_errors_and_reload(void) {
    mtn_game *g = mtn_test_game();
    /* a runtime error in a callback is reported, not fatal */
    CHECK(run(g, "function update(dt) error('boom') end"));
    mtn_game_update(g, 0.016f);
    CHECK(strstr(g->script_error, "boom") != NULL);
    mtn_ui_begin(&g->ui, 640, 360, &g->input);
    int before = g->ui.count;
    mtn_script_ui(g); /* shows the error box */
    CHECK(g->ui.count > before);
    /* long errors wrap onto a second line */
    snprintf(g->script_error, sizeof g->script_error, "%s", "x123456789012345678901234567890123456789012345678901234567890123456789");
    mtn_script_ui(g);
    /* syntax errors in run_string */
    CHECK(!run(g, "this is not lua"));
    /* missing callbacks are fine */
    CHECK(run(g, "action = nil; update = nil"));
    mtn_script_action(g, "jump", 1);
    mtn_script_update(g, 0.1f);
    CHECK(g->script_error[0] == 0);
    /* reload re-runs main.lua and calls init(true) */
    g->reload_request = 1;
    mtn_game_update(g, 0.016f);
    CHECK(g->L != NULL);
    CHECK(g->script_error[0] == 0);
    CHECK(strstr(g->message, "reloaded") != NULL);
    /* without scripts: an error, but no crash */
    mtn_script_shutdown(g);
    mtn_script_action(g, "jump", 1);
    mtn_script_update(g, 0.1f);
    CHECK(!mtn_script_call(g, "init", 0));
    CHECK(!mtn_script_run_string(g, "x = 1", "=x"));
    mtn_vfs_init("/nonexistent/");
    CHECK(!mtn_script_reload(g));
    CHECK(strstr(g->script_error, "not found") != NULL);
    mtn_vfs_init(MTN_SOURCE_DIR "/");
    /* a world generated with a random seed */
    CHECK(mtn_script_reload(g));
    CHECK(run(g, "world.params{ size = 65, cell = 20, erosion_drops = 100, sea_level = 50, peak_height = 300,\n"
                 "  range_height = 100, hill_height = 20, ridge_strength = 0.5, lake_min_depth = 1, tree_line = 0.5,\n"
                 "  snow_line = 0.8, playable_radius = 0.8 }; local s = world.generate(); assert(s ~= 20261119)\n"));
    CHECK(g->terrain.n == 65);
    mtn_test_game_free(g);
}

MTN_SUITE(script) {
    TEST_RUN(test_world_time_weather);
    TEST_RUN(test_species_player_house);
    TEST_RUN(test_camera_photos_ui_audio);
    TEST_RUN(test_storage_and_restrictions);
    TEST_RUN(test_errors_and_reload);
}
