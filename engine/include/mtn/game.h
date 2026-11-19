/* Mountain engine - game state and simulation step (platform independent). MIT License. */
#ifndef MTN_GAME_H
#define MTN_GAME_H

#include "mtn/audio.h"
#include "mtn/fauna.h"
#include "mtn/flora.h"
#include "mtn/house.h"
#include "mtn/input.h"
#include "mtn/photo.h"
#include "mtn/player.h"
#include "mtn/sky.h"
#include "mtn/terrain.h"
#include "mtn/weather.h"

#define MTN_VERSION "0.1.0"
#define MTN_MAX_BINDINGS 64

struct lua_State;

typedef struct {
    float mouse_sensitivity;
    int invert_y;
    float volume;
    int pixel_lines;       /* internal render height: 180, 216, 270, 360 */
    float fov;             /* degrees */
    int show_fps;
} mtn_settings;

typedef struct {
    char action[24];
    char key[24];
} mtn_binding;

typedef struct mtn_game {
    /* world */
    mtn_planet planet;
    mtn_clock clock;
    mtn_terrain_params tparams;
    mtn_terrain terrain;
    mtn_flora flora;
    mtn_flora_season fseason;
    mtn_fauna fauna;
    mtn_weather weather;
    mtn_sky_state sky;
    mtn_season_blend season;
    uint32_t seed;
    int has_world;
    int world_version;
    float snowline;          /* altitude fraction where snow starts */
    float temperature;       /* at the player */

    /* player and house */
    mtn_player player;
    mtn_house house;
    mtn_room rooms[3];       /* current room and its neighbours */
    int room_base;           /* index of rooms[0] */
    int rooms_version;
    mtn_mesh room_solid[3], room_sprites[3], room_glow[3];
    mtn_mesh house_solid, house_sprites, house_glow, door_mesh;
    int house_mesh_version;  /* bumped when the exterior meshes are rebuilt */
    int interior;            /* player is inside the house */
    vec3 house_spot;         /* where the player stood outside */

    /* input, ui, audio, photos */
    mtn_input input;
    mtn_touch touch;
    mtn_ui ui;
    mtn_mixer mixer;
    mtn_gallery gallery;
    mtn_binding bindings[MTN_MAX_BINDINGS];
    int binding_count;
    int ui_modal;            /* menus open: no walking */

    int camera_mode;
    float photo_fov;
    int photo_filter;
    int photo_request;
    float photo_flash;
    int download_request;    /* gallery index or -1 */
    int delete_request;
    int quit_request;
    int fullscreen_request;
    int reload_request;
    int storage_dirty;

    char message[160];
    float message_time;
    char script_error[512];
    char user_dir[512];
    char platform[16];

    mtn_settings settings;
    double real_time;
    float dt;
    float fps;
    int frame;
    int xr_active;
    int xr_supported;
    int test_mode;
    struct lua_State *L;
} mtn_game;

void mtn_game_init(mtn_game *g, const char *platform, const char *user_dir);
void mtn_game_shutdown(mtn_game *g);
void mtn_game_new_world(mtn_game *g, uint32_t seed);
void mtn_game_update(mtn_game *g, float dt);
void mtn_game_message(mtn_game *g, const char *msg, float seconds);
/* house helpers used by scripts */
int mtn_game_summon_house(mtn_game *g);
int mtn_game_unsummon_house(mtn_game *g);
int mtn_game_interact(mtn_game *g);
void mtn_game_teleport_trailhead(mtn_game *g);
float mtn_game_current_fov(const mtn_game *g);
void mtn_game_refresh_rooms(mtn_game *g, int force);
void mtn_game_play(mtn_game *g, const char *sound, float vol);
int mtn_game_load_sounds(mtn_game *g);
float mtn_game_snowline(const mtn_game *g);

/* Lua scripting (engine/src/script) */
int mtn_script_init(mtn_game *g);
void mtn_script_shutdown(mtn_game *g);
int mtn_script_reload(mtn_game *g);
int mtn_script_call(mtn_game *g, const char *fn, int nargs_pushed);
void mtn_script_action(mtn_game *g, const char *action, int pressed);
void mtn_script_update(mtn_game *g, float dt);
void mtn_script_ui(mtn_game *g);
int mtn_script_run_string(mtn_game *g, const char *code, const char *name);
void mtn_script_error(mtn_game *g, const char *where, const char *msg);

#endif
