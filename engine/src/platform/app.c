/* Mountain engine - SDL3 platform layer: window, OpenGL context, input
 * (keyboard, mouse, touch, gamepads), audio, photo files, the web/WebXR
 * glue and the automated game-test driver. MIT License. */
#include "platform/app.h"
#include "render/gl.h"
#include "render/renderer.h"

#include "mtn/game.h"
#include "mtn/vfs.h"

#include <SDL3/SDL.h>

#include "lauxlib.h"
#include "lua.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
extern int mtn_js_is_touch(void);
extern void mtn_js_download(const char *name, const void *data, int size);
extern void mtn_js_sync_fs(void);
extern void mtn_js_xr_init(void);
extern int mtn_js_xr_supported(void);
extern void mtn_js_set_title(const char *t);
extern void mtn_js_ready(void);
#define PLATFORM_NAME "web"
#elif defined(_WIN32)
#define PLATFORM_NAME "windows"
#else
#define PLATFORM_NAME "linux"
#endif

typedef struct {
    SDL_Scancode sc;
    int action;
} keymap;

typedef struct {
    lua_State *co;
    int ref;
    int wait;
    int done;
    int failures;
    int checks;
    char first_failure[256];
    char screenshot[128];
    int max_frames;
    SDL_Joystick *pad;
} test_state;

typedef struct {
    SDL_Window *win;
    SDL_GLContext gl;
    SDL_AudioStream *audio;
    mtn_renderer *r;
    mtn_game g;
    int running;
    int win_w, win_h;
    Uint64 last;
    keymap keys[96];
    int key_count;
    SDL_Gamepad *pad;
    float axes[SDL_GAMEPAD_AXIS_COUNT];
    int trigger_down;
    int run_toggle;
    int stick_nav;
    int fixed_dt;
    int frame_limit;
    int exit_code;
    test_state test;
    char test_path[512];
    char data_dir[512];
    uint32_t seed;
    int seed_set;
} app;

static app *g_app;

/* ------------------------------------------------------------------ keys */

static void add_key(app *a, SDL_Scancode sc, int action) {
    if (a->key_count < (int)(sizeof a->keys / sizeof a->keys[0])) {
        a->keys[a->key_count].sc = sc;
        a->keys[a->key_count].action = action;
        a->key_count++;
    }
}

static void default_keys(app *a) {
    a->key_count = 0;
    add_key(a, SDL_SCANCODE_W, MTN_ACT_FORWARD);
    add_key(a, SDL_SCANCODE_UP, MTN_ACT_FORWARD);
    add_key(a, SDL_SCANCODE_S, MTN_ACT_BACK);
    add_key(a, SDL_SCANCODE_DOWN, MTN_ACT_BACK);
    add_key(a, SDL_SCANCODE_A, MTN_ACT_LEFT);
    add_key(a, SDL_SCANCODE_LEFT, MTN_ACT_LEFT);
    add_key(a, SDL_SCANCODE_LEFT, MTN_ACT_PREV);
    add_key(a, SDL_SCANCODE_D, MTN_ACT_RIGHT);
    add_key(a, SDL_SCANCODE_RIGHT, MTN_ACT_RIGHT);
    add_key(a, SDL_SCANCODE_RIGHT, MTN_ACT_NEXT);
    add_key(a, SDL_SCANCODE_LSHIFT, MTN_ACT_RUN);
    add_key(a, SDL_SCANCODE_RSHIFT, MTN_ACT_RUN);
    add_key(a, SDL_SCANCODE_SPACE, MTN_ACT_JUMP);
    add_key(a, SDL_SCANCODE_E, MTN_ACT_INTERACT);
    add_key(a, SDL_SCANCODE_H, MTN_ACT_HOUSE);
    add_key(a, SDL_SCANCODE_C, MTN_ACT_CAMERA);
    add_key(a, SDL_SCANCODE_RETURN, MTN_ACT_SHOOT);
    add_key(a, SDL_SCANCODE_G, MTN_ACT_GALLERY);
    add_key(a, SDL_SCANCODE_ESCAPE, MTN_ACT_MENU);
    add_key(a, SDL_SCANCODE_EQUALS, MTN_ACT_ZOOM_IN);
    add_key(a, SDL_SCANCODE_KP_PLUS, MTN_ACT_ZOOM_IN);
    add_key(a, SDL_SCANCODE_MINUS, MTN_ACT_ZOOM_OUT);
    add_key(a, SDL_SCANCODE_KP_MINUS, MTN_ACT_ZOOM_OUT);
    add_key(a, SDL_SCANCODE_F, MTN_ACT_FILTER);
    add_key(a, SDL_SCANCODE_DELETE, MTN_ACT_DELETE);
    add_key(a, SDL_SCANCODE_F3, MTN_ACT_DEBUG);
    add_key(a, SDL_SCANCODE_F11, MTN_ACT_FULLSCREEN);
    add_key(a, SDL_SCANCODE_F5, MTN_ACT_RELOAD);
    add_key(a, SDL_SCANCODE_Q, MTN_ACT_TURN_LEFT);
    add_key(a, SDL_SCANCODE_X, MTN_ACT_DOWNLOAD);
}

static void apply_bindings(app *a) {
    if (a->g.clear_bindings) {
        a->key_count = 0;
        a->g.clear_bindings = 0;
    }
    for (int i = 0; i < a->g.binding_count; i++) {
        SDL_Scancode sc = SDL_GetScancodeFromName(a->g.bindings[i].key);
        int act = mtn_action_from_name(a->g.bindings[i].action);
        if (sc != SDL_SCANCODE_UNKNOWN && act >= 0) add_key(a, sc, act);
    }
    a->g.binding_count = 0;
}

/* ------------------------------------------------------------------ files */

static void ensure_dir(const char *path) { SDL_CreateDirectory(path); }

static void scan_gallery(app *a) {
    char dir[600];
    snprintf(dir, sizeof dir, "%sphotos", a->g.user_dir);
    mtn_gallery_clear(&a->g.gallery);
    int count = 0;
    char **files = SDL_GlobDirectory(dir, "*.png", 0, &count);
    for (int i = 0; files && i < count; i++) mtn_gallery_add(&a->g.gallery, files[i]);
    SDL_free(files);
    mtn_gallery_sort(&a->g.gallery);
}

static void save_photo(app *a) {
    int w, h;
    uint8_t *px = mtn_render_capture(a->r, &a->g, &w, &h);
    if (!px) return;
    mtn_photo_filter_apply(px, w, h, a->g.photo_filter);
    int k = mtn_photo_scale_for(h, 720);
    uint8_t *big = mtn_photo_upscale(px, w, h, k);
    free(px);
    size_t size;
    uint8_t *png = mtn_png_encode(big, w * k, h * k, &size);
    free(big);
    if (!png) return;
    SDL_Time now = 0;
    SDL_GetCurrentTime(&now);
    SDL_DateTime dt;
    SDL_TimeToDateTime(now, &dt, true);
    char name[96], path[700];
    for (int n = 0; n < 1000; n++) {
        mtn_photo_name(name, sizeof name, a->g.seed, dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second, n);
        snprintf(path, sizeof path, "%sphotos/%s", a->g.user_dir, name);
        if (!mtn_file_exists(path)) break;
    }
    if (mtn_file_write(path, png, size)) {
        mtn_gallery_add(&a->g.gallery, name);
        a->g.photo_flash = 1.0f;
        mtn_game_play(&a->g, "shutter", 0.8f);
        a->g.storage_dirty = 1;
        mtn_script_action(&a->g, "photo_saved", 1);
    } else {
        mtn_game_message(&a->g, "Could not save the photo.", 3.0f);
    }
    free(png);
}

static void handle_requests(app *a) {
    mtn_game *g = &a->g;
    if (g->photo_request) {
        g->photo_request = 0;
        save_photo(a);
    }
    if (g->download_request >= 0) {
        int i = g->download_request;
        g->download_request = -1;
        if (i < g->gallery.count) {
#ifdef __EMSCRIPTEN__
            char path[700];
            snprintf(path, sizeof path, "%sphotos/%s", g->user_dir, g->gallery.names[i]);
            size_t size;
            char *d = mtn_file_read(path, &size);
            if (d) {
                mtn_js_download(g->gallery.names[i], d, (int)size);
                free(d);
            }
#else
            char msg[600];
            snprintf(msg, sizeof msg, "Saved in %sphotos", g->user_dir);
            mtn_game_message(g, msg, 4.0f);
#endif
        }
    }
    if (g->delete_request >= 0) {
        int i = g->delete_request;
        g->delete_request = -1;
        if (i < g->gallery.count) {
            char path[700];
            snprintf(path, sizeof path, "%sphotos/%s", g->user_dir, g->gallery.names[i]);
            SDL_RemovePath(path);
            mtn_render_forget_photo(a->r, g->gallery.names[i]);
            mtn_gallery_remove(&g->gallery, i);
            g->storage_dirty = 1;
        }
    }
    if (g->fullscreen_request) {
        SDL_SetWindowFullscreen(a->win, g->fullscreen_request == 1);
        g->fullscreen_request = 0;
    }
    if (g->storage_dirty) {
        g->storage_dirty = 0;
#ifdef __EMSCRIPTEN__
        mtn_js_sync_fs();
#endif
    }
    if (g->quit_request) a->running = 0;
    if (g->binding_count || g->clear_bindings) apply_bindings(a);
}

/* ------------------------------------------------------------------ events */

static void set_mode(app *a, int mode) {
    if (a->g.input.mode == mode) return;
    if (a->g.input.mode == MTN_MODE_TOUCH) mtn_touch_reset(&a->g.touch, &a->g.input);
    a->g.input.mode = mode;
    if (mode == MTN_MODE_TOUCH) SDL_SetWindowRelativeMouseMode(a->win, false);
}

static void gamepad_button(app *a, int button, int down) {
    mtn_input *in = &a->g.input;
    set_mode(a, MTN_MODE_GAMEPAD);
    int act = -1;
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: act = MTN_ACT_JUMP; break;
    case SDL_GAMEPAD_BUTTON_WEST: act = MTN_ACT_INTERACT; break;
    case SDL_GAMEPAD_BUTTON_NORTH: act = MTN_ACT_HOUSE; break;
    case SDL_GAMEPAD_BUTTON_EAST: act = a->g.camera_mode ? MTN_ACT_CAMERA : MTN_ACT_MENU; break;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: act = MTN_ACT_CAMERA; break;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: act = MTN_ACT_FILTER; break;
    case SDL_GAMEPAD_BUTTON_START: act = MTN_ACT_MENU; break;
    case SDL_GAMEPAD_BUTTON_BACK: act = MTN_ACT_GALLERY; break;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: act = MTN_ACT_ZOOM_IN; break;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: act = MTN_ACT_ZOOM_OUT; break;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: act = MTN_ACT_PREV; break;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: act = MTN_ACT_NEXT; break;
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
        if (down) a->run_toggle = !a->run_toggle;
        break;
    default: break;
    }
    if (act >= 0) mtn_input_set(in, act, down);
}

static void handle_event(app *a, const SDL_Event *e) {
    mtn_game *g = &a->g;
    mtn_input *in = &g->input;
    switch (e->type) {
    case SDL_EVENT_QUIT: a->running = 0; break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        if (e->key.repeat) break;
        set_mode(a, MTN_MODE_KEYBOARD);
        int down = e->type == SDL_EVENT_KEY_DOWN;
        for (int i = 0; i < a->key_count; i++)
            if (a->keys[i].sc == e->key.scancode) mtn_input_set(in, a->keys[i].action, down);
        if (down && e->key.scancode == SDL_SCANCODE_ESCAPE) SDL_SetWindowRelativeMouseMode(a->win, false);
        break;
    }
    case SDL_EVENT_MOUSE_MOTION:
        if (e->motion.which == SDL_TOUCH_MOUSEID) break;
        mtn_input_pointer(in, e->motion.x, e->motion.y, in->pointer_down);
        if (SDL_GetWindowRelativeMouseMode(a->win) || a->g.test_mode) {
            float k = 0.0024f * g->settings.mouse_sensitivity;
            in->look_x += e->motion.xrel * k;
            in->look_y -= e->motion.yrel * k;
            set_mode(a, MTN_MODE_KEYBOARD);
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        if (e->button.which == SDL_TOUCH_MOUSEID) break;
        int down = e->type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        set_mode(a, MTN_MODE_KEYBOARD);
        if (e->button.button == SDL_BUTTON_LEFT) {
            mtn_input_pointer(in, e->button.x, e->button.y, down);
            if (!g->ui_modal) {
                if (g->camera_mode && SDL_GetWindowRelativeMouseMode(a->win)) mtn_input_set(in, MTN_ACT_SHOOT, down);
                else if (down && !SDL_GetWindowRelativeMouseMode(a->win) && !a->g.test_mode)
                    SDL_SetWindowRelativeMouseMode(a->win, true);
            }
        }
        break;
    }
    case SDL_EVENT_MOUSE_WHEEL:
        in->wheel += e->wheel.y;
        if (e->wheel.y > 0) {
            mtn_input_set(in, MTN_ACT_ZOOM_IN, 1);
            mtn_input_set(in, MTN_ACT_ZOOM_IN, 0);
        } else if (e->wheel.y < 0) {
            mtn_input_set(in, MTN_ACT_ZOOM_OUT, 1);
            mtn_input_set(in, MTN_ACT_ZOOM_OUT, 0);
        }
        break;
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP: {
        set_mode(a, MTN_MODE_TOUCH);
        float x = e->tfinger.x * (float)a->win_w, y = e->tfinger.y * (float)a->win_h;
        int64_t id = (int64_t)e->tfinger.fingerID;
        if (g->ui_modal) {
            mtn_input_pointer(in, x, y, e->type != SDL_EVENT_FINGER_UP);
        } else if (e->type == SDL_EVENT_FINGER_DOWN) {
            mtn_touch_down(&g->touch, in, id, x, y);
        } else if (e->type == SDL_EVENT_FINGER_MOTION) {
            mtn_touch_move(&g->touch, in, id, x, y);
        } else {
            mtn_touch_up(&g->touch, in, id);
        }
        break;
    }
    case SDL_EVENT_GAMEPAD_ADDED:
        if (!a->pad) a->pad = SDL_OpenGamepad(e->gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_REMOVED:
        if (a->pad && SDL_GetGamepadID(a->pad) == e->gdevice.which) {
            SDL_CloseGamepad(a->pad);
            a->pad = NULL;
            memset(a->axes, 0, sizeof a->axes);
        }
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        gamepad_button(a, e->gbutton.button, e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (e->gaxis.axis < SDL_GAMEPAD_AXIS_COUNT) {
            a->axes[e->gaxis.axis] = (float)e->gaxis.value / 32767.0f;
            if (fabsf(a->axes[e->gaxis.axis]) > 0.3f) set_mode(a, MTN_MODE_GAMEPAD);
        }
        break;
    default: break;
    }
}

static float deadzone(float v) {
    float a = fabsf(v);
    if (a < 0.18f) return 0.0f;
    return (v > 0 ? 1.0f : -1.0f) * (a - 0.18f) / 0.82f;
}

static void gamepad_frame(app *a, float dt) {
    mtn_input *in = &a->g.input;
    if (in->mode != MTN_MODE_GAMEPAD) return;
    in->move_x = deadzone(a->axes[SDL_GAMEPAD_AXIS_LEFTX]);
    in->move_y = -deadzone(a->axes[SDL_GAMEPAD_AXIS_LEFTY]);
    in->look_x += deadzone(a->axes[SDL_GAMEPAD_AXIS_RIGHTX]) * 2.6f * dt;
    in->look_y -= deadzone(a->axes[SDL_GAMEPAD_AXIS_RIGHTY]) * 2.0f * dt;
    in->analog_run = a->run_toggle;
    /* in menus the left stick moves the selection like the D-pad */
    int sy = a->axes[SDL_GAMEPAD_AXIS_LEFTY] < -0.6f ? -1 : (a->axes[SDL_GAMEPAD_AXIS_LEFTY] > 0.6f ? 1 : 0);
    if (a->g.ui_modal && sy != a->stick_nav) {
        if (sy < 0) mtn_input_set(in, MTN_ACT_ZOOM_IN, 1);
        if (sy > 0) mtn_input_set(in, MTN_ACT_ZOOM_OUT, 1);
        if (sy == 0) {
            mtn_input_set(in, MTN_ACT_ZOOM_IN, 0);
            mtn_input_set(in, MTN_ACT_ZOOM_OUT, 0);
        }
    }
    a->stick_nav = sy;
    int trig = a->axes[SDL_GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.5f;
    if (trig != a->trigger_down) {
        a->trigger_down = trig;
        mtn_input_set(in, MTN_ACT_SHOOT, trig);
    }
}

/* ------------------------------------------------------------------ audio */

static void SDLCALL audio_callback(void *ud, SDL_AudioStream *stream, int additional, int total) {
    (void)total;
    app *a = ud;
    float buf[2048];
    int frames = additional / (int)(sizeof(float) * 2);
    while (frames > 0) {
        int n = frames > 1024 ? 1024 : frames;
        mtn_mixer_mix(&a->g.mixer, buf, n);
        SDL_PutAudioStreamData(stream, buf, n * (int)sizeof(float) * 2);
        frames -= n;
    }
}

static void open_audio(app *a) {
    SDL_AudioSpec spec;
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;
    spec.freq = 44100;
    a->audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_callback, a);
    if (a->audio) SDL_ResumeAudioStreamDevice(a->audio);
    else SDL_Log("audio unavailable: %s", SDL_GetError());
}

static void lock_audio(app *a) {
    if (a->audio) SDL_LockAudioStream(a->audio);
}
static void unlock_audio(app *a) {
    if (a->audio) SDL_UnlockAudioStream(a->audio);
}

/* ------------------------------------------------------------------ game tests */

static SDL_WindowID wid(void) { return SDL_GetWindowID(g_app->win); }

static int t_wait(lua_State *L) {
    double s = luaL_optnumber(L, 1, 0.0);
    lua_pushinteger(L, (lua_Integer)ceil(s * 60.0));
    return lua_yield(L, 1);
}
static int t_frames(lua_State *L) {
    lua_pushinteger(L, luaL_optinteger(L, 1, 1));
    return lua_yield(L, 1);
}
static int t_key(lua_State *L) {
    SDL_Scancode sc = SDL_GetScancodeFromName(luaL_checkstring(L, 1));
    if (sc == SDL_SCANCODE_UNKNOWN) return luaL_error(L, "unknown key %s", lua_tostring(L, 1));
    SDL_Event e;
    SDL_zero(e);
    e.type = lua_toboolean(L, 2) ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    e.key.windowID = wid();
    e.key.scancode = sc;
    e.key.key = SDL_GetKeyFromScancode(sc, SDL_KMOD_NONE, false);
    e.key.down = lua_toboolean(L, 2);
    SDL_PushEvent(&e);
    return 0;
}
static int t_mouse_move(lua_State *L) {
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_MOUSE_MOTION;
    e.motion.windowID = wid();
    e.motion.xrel = (float)luaL_checknumber(L, 1);
    e.motion.yrel = (float)luaL_checknumber(L, 2);
    e.motion.x = (float)luaL_optnumber(L, 3, g_app->win_w / 2.0);
    e.motion.y = (float)luaL_optnumber(L, 4, g_app->win_h / 2.0);
    SDL_PushEvent(&e);
    return 0;
}
static int t_mouse_button(lua_State *L) {
    SDL_Event e;
    SDL_zero(e);
    int down = lua_toboolean(L, 1);
    e.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    e.button.windowID = wid();
    e.button.button = (Uint8)luaL_optinteger(L, 4, SDL_BUTTON_LEFT);
    e.button.down = down;
    e.button.x = (float)luaL_optnumber(L, 2, g_app->win_w / 2.0);
    e.button.y = (float)luaL_optnumber(L, 3, g_app->win_h / 2.0);
    SDL_PushEvent(&e);
    return 0;
}
static int t_wheel(lua_State *L) {
    SDL_Event e;
    SDL_zero(e);
    e.type = SDL_EVENT_MOUSE_WHEEL;
    e.wheel.windowID = wid();
    e.wheel.y = (float)luaL_checknumber(L, 1);
    SDL_PushEvent(&e);
    return 0;
}
static int t_touch(lua_State *L) {
    const char *phase = luaL_checkstring(L, 1);
    SDL_Event e;
    SDL_zero(e);
    e.type = strcmp(phase, "down") == 0 ? SDL_EVENT_FINGER_DOWN : (strcmp(phase, "up") == 0 ? SDL_EVENT_FINGER_UP : SDL_EVENT_FINGER_MOTION);
    e.tfinger.touchID = 1;
    e.tfinger.fingerID = (SDL_FingerID)luaL_checkinteger(L, 2);
    e.tfinger.x = (float)luaL_checknumber(L, 3);
    e.tfinger.y = (float)luaL_checknumber(L, 4);
    e.tfinger.pressure = 1.0f;
    e.tfinger.windowID = wid();
    SDL_PushEvent(&e);
    return 0;
}
static int t_gamepad(lua_State *L) {
    test_state *t = &g_app->test;
    if (!t->pad) {
        SDL_VirtualJoystickDesc desc;
        SDL_INIT_INTERFACE(&desc);
        desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
        desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
        desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
        desc.name = "Mountain test pad";
        SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
        t->pad = id ? SDL_OpenJoystick(id) : NULL;
    }
    lua_pushboolean(L, t->pad != NULL);
    return 1;
}
static int t_gamepad_button(lua_State *L) {
    SDL_GamepadButton b = SDL_GetGamepadButtonFromString(luaL_checkstring(L, 1));
    if (g_app->test.pad && b != SDL_GAMEPAD_BUTTON_INVALID) SDL_SetJoystickVirtualButton(g_app->test.pad, b, lua_toboolean(L, 2));
    return 0;
}
static int t_gamepad_axis(lua_State *L) {
    SDL_GamepadAxis ax = SDL_GetGamepadAxisFromString(luaL_checkstring(L, 1));
    float v = mtn_clampf((float)luaL_checknumber(L, 2), -1.0f, 1.0f);
    if (g_app->test.pad && ax != SDL_GAMEPAD_AXIS_INVALID) SDL_SetJoystickVirtualAxis(g_app->test.pad, ax, (Sint16)(v * 32767.0f));
    return 0;
}
static int t_gamepad_detach(lua_State *L) {
    (void)L;
    test_state *t = &g_app->test;
    if (t->pad) {
        SDL_JoystickID id = SDL_GetJoystickID(t->pad);
        SDL_CloseJoystick(t->pad);
        SDL_DetachVirtualJoystick(id);
        t->pad = NULL;
    }
    return 0;
}
static int t_check(lua_State *L) {
    test_state *t = &g_app->test;
    t->checks++;
    if (!lua_toboolean(L, 1)) {
        t->failures++;
        const char *msg = luaL_optstring(L, 2, "check failed");
        lua_Debug ar;
        int line = 0;
        if (lua_getstack(L, 1, &ar) && lua_getinfo(L, "l", &ar)) line = ar.currentline;
        fprintf(stderr, "[test] FAIL line %d: %s\n", line, msg);
        if (!t->first_failure[0]) snprintf(t->first_failure, sizeof t->first_failure, "line %d: %s", line, msg);
    }
    lua_pushboolean(L, lua_toboolean(L, 1));
    return 1;
}
static int t_screenshot(lua_State *L) {
    snprintf(g_app->test.screenshot, sizeof g_app->test.screenshot, "%s", luaL_checkstring(L, 1));
    return 0;
}
static int t_resize(lua_State *L) {
    SDL_SetWindowSize(g_app->win, (int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2));
    return 0;
}
static int t_stats(lua_State *L) {
    mtn_render_stats s = mtn_render_get_stats(g_app->r);
    lua_newtable(L);
    lua_pushinteger(L, s.draw_calls); lua_setfield(L, -2, "draw_calls");
    lua_pushinteger(L, s.triangles); lua_setfield(L, -2, "triangles");
    lua_pushinteger(L, s.flora_chunks); lua_setfield(L, -2, "flora_chunks");
    lua_pushinteger(L, s.portals); lua_setfield(L, -2, "portals");
    lua_pushinteger(L, s.lw); lua_setfield(L, -2, "width");
    lua_pushinteger(L, s.lh); lua_setfield(L, -2, "height");
    return 1;
}
static int t_touch_layout(lua_State *L) {
    const mtn_touch *t = &g_app->g.touch;
    const char *action = luaL_checkstring(L, 1);
    int act = mtn_action_from_name(action);
    for (int i = 0; i < t->button_count; i++)
        if (t->buttons[i].action == act) {
            lua_pushnumber(L, t->buttons[i].x / (float)g_app->win_w);
            lua_pushnumber(L, t->buttons[i].y / (float)g_app->win_h);
            return 2;
        }
    return 0;
}
static int t_pixel(lua_State *L) {
    /* average colour of the window (cheap "is something drawn" probe) */
    int w = g_app->win_w, h = g_app->win_h;
    uint8_t *px = mtn_render_read_window(w, h);
    double acc[3] = {0, 0, 0};
    for (int i = 0; i < w * h; i++)
        for (int c = 0; c < 3; c++) acc[c] += px[i * 4 + c];
    free(px);
    for (int c = 0; c < 3; c++) lua_pushnumber(L, acc[c] / (w * h * 255.0));
    return 3;
}
static int t_render_view(lua_State *L) {
    /* renders the left half of the window through the stereo/XR view path */
    mtn_game *g = &g_app->g;
    float aspect = (float)(g_app->win_w / 2) / (float)(g_app->win_h > 0 ? g_app->win_h : 1);
    mat4 proj = m4_perspective(70.0f * MTN_DEG2RAD, aspect, 0.08f, 5000.0f);
    mtn_render_view(g_app->r, g, mtn_player_view(&g->player), proj, mtn_player_eye(&g->player), 0, 0, 0, g_app->win_w / 2,
                    g_app->win_h, lua_toboolean(L, 1));
    return 0;
}
static int t_data_dir(lua_State *L) { lua_pushstring(L, g_app->g.user_dir); return 1; }
static int t_finish(lua_State *L) {
    test_state *t = &g_app->test;
    if (!lua_toboolean(L, 1)) t->failures++;
    t->done = 1;
    return lua_yield(L, 0);
}

static void test_register(app *a) {
    static const luaL_Reg fns[] = {{"wait", t_wait}, {"frames", t_frames}, {"key", t_key}, {"mouse_move", t_mouse_move},
        {"mouse_button", t_mouse_button}, {"wheel", t_wheel}, {"touch", t_touch}, {"gamepad", t_gamepad},
        {"gamepad_button", t_gamepad_button}, {"gamepad_axis", t_gamepad_axis}, {"gamepad_detach", t_gamepad_detach},
        {"check", t_check}, {"screenshot", t_screenshot}, {"resize", t_resize}, {"stats", t_stats},
        {"touch_button", t_touch_layout}, {"average_color", t_pixel}, {"data_dir", t_data_dir}, {"render_view", t_render_view}, {"finish", t_finish}, {NULL, NULL}};
    lua_State *L = a->g.L;
    lua_newtable(L);
    luaL_setfuncs(L, fns, 0);
    lua_setglobal(L, "test");
}

static int test_start(app *a) {
    if (!a->g.L) return 0;
    test_register(a);
    char *src = mtn_file_read(a->test_path, NULL);
    if (!src) {
        fprintf(stderr, "[test] cannot read %s\n", a->test_path);
        return 0;
    }
    lua_State *L = a->g.L;
    a->test.co = lua_newthread(L);
    a->test.ref = luaL_ref(L, LUA_REGISTRYINDEX);
    if (luaL_loadbuffer(a->test.co, src, strlen(src), a->test_path) != LUA_OK) {
        fprintf(stderr, "[test] %s\n", lua_tostring(a->test.co, -1));
        free(src);
        return 0;
    }
    free(src);
    return 1;
}

static void test_step(app *a) {
    test_state *t = &a->test;
    if (!t->co || t->done) return;
    if (t->wait > 0) {
        t->wait--;
        return;
    }
    int nres = 0;
    int st = lua_resume(t->co, a->g.L, 0, &nres);
    if (st == LUA_YIELD) {
        t->wait = nres > 0 && lua_isinteger(t->co, -1) ? (int)lua_tointeger(t->co, -1) : 0;
        lua_pop(t->co, nres);
    } else if (st == LUA_OK) {
        t->done = 1;
    } else {
        fprintf(stderr, "[test] error: %s\n", lua_tostring(t->co, -1));
        luaL_traceback(a->g.L, t->co, NULL, 0);
        fprintf(stderr, "%s\n", lua_tostring(a->g.L, -1));
        lua_pop(a->g.L, 1);
        t->failures++;
        if (!t->first_failure[0]) snprintf(t->first_failure, sizeof t->first_failure, "%s", lua_tostring(t->co, -1));
        t->done = 1;
    }
}

static void test_screenshot(app *a) {
    if (!a->test.screenshot[0]) return;
    uint8_t *px = mtn_render_read_window(a->win_w, a->win_h);
    size_t size;
    uint8_t *png = mtn_png_encode(px, a->win_w, a->win_h, &size);
    char dir[600], path[800];
    snprintf(dir, sizeof dir, "%sscreens", a->g.user_dir);
    ensure_dir(dir);
    snprintf(path, sizeof path, "%s/%s.png", dir, a->test.screenshot);
    if (png) mtn_file_write(path, png, size);
    free(png);
    free(px);
    a->test.screenshot[0] = 0;
}

/* ------------------------------------------------------------------ frame */

static void frame(app *a, float dt) {
    mtn_game *g = &a->g;
    mtn_input_begin_frame(&g->input);
    SDL_Event e;
    while (SDL_PollEvent(&e)) handle_event(a, &e);
    SDL_GetWindowSizeInPixels(a->win, &a->win_w, &a->win_h);
    gamepad_frame(a, dt);
    if (g->input.mode == MTN_MODE_TOUCH) {
        int ctx = (g->camera_mode ? MTN_TOUCH_CTX_CAMERA : 0) |
                  ((mtn_house_exists(&g->house) && (g->interior || mtn_house_door_distance(&g->house, g->player.pos) < 3.0f))
                       ? MTN_TOUCH_CTX_NEAR_DOOR : 0);
        mtn_touch_layout(&g->touch, (float)a->win_w, (float)a->win_h, ctx);
    }
    if (g->test_mode) test_step(a);
    lock_audio(a);
    mtn_game_update(g, dt);
    g->ui.nav_enabled = g->ui_modal;
    mtn_ui_begin(&g->ui, (float)a->win_w, (float)a->win_h, &g->input);
    mtn_script_ui(g);
    unlock_audio(a);
    if (g->ui_modal && SDL_GetWindowRelativeMouseMode(a->win)) SDL_SetWindowRelativeMouseMode(a->win, false);
    mtn_render_frame(a->r, g, a->win_w, a->win_h);
    lock_audio(a);
    handle_requests(a);
    unlock_audio(a);
    if (g->test_mode) {
        test_screenshot(a);
        if (a->test.done || (a->frame_limit > 0 && g->frame >= a->frame_limit) || g->script_error[0]) {
            if (!a->test.done) {
                a->test.failures++;
                fprintf(stderr, "[test] %s\n", g->script_error[0] ? g->script_error : "frame limit reached before test.finish()");
            }
            a->running = 0;
        }
    }
    SDL_GL_SwapWindow(a->win);
}

static float frame_dt(app *a) {
    Uint64 now = SDL_GetTicksNS();
    float dt = a->last ? (float)((double)(now - a->last) / 1e9) : 1.0f / 60.0f;
    a->last = now;
    if (a->fixed_dt) dt = 1.0f / 60.0f;
    return dt;
}

/* ------------------------------------------------------------------ web / WebXR */

#ifdef __EMSCRIPTEN__
static void web_frame(void *ud) {
    app *a = ud;
    if (!a->running) {
        emscripten_cancel_main_loop();
        return;
    }
    a->g.xr_supported = mtn_js_xr_supported();
    frame(a, frame_dt(a));
    if (a->g.frame == 2) mtn_js_ready();
}

EMSCRIPTEN_KEEPALIVE void mtn_web_touch_hint(int is_touch) {
    if (g_app && is_touch) set_mode(g_app, MTN_MODE_TOUCH);
}

/* Called from JavaScript for every XR frame. data layout (floats):
 * [0..1] left stick, [2..3] right stick, [4] buttons bitmask, then per view: proj[16], view[16], viewport[4] */
EMSCRIPTEN_KEEPALIVE void mtn_web_xr_frame(const float *data, int views, int fbo, int width, int height) {
    app *a = g_app;
    if (!a) return;
    mtn_game *g = &a->g;
    static int prev_buttons, turned;
    float dt = frame_dt(a);
    g->xr_active = 1;
    mtn_input_begin_frame(&g->input);
    SDL_Event e;
    while (SDL_PollEvent(&e)) handle_event(a, &e);
    g->input.mode = MTN_MODE_XR;
    /* head yaw from the first view (inverse of the view matrix gives the eye transform) */
    mat4 v0;
    memcpy(v0.m, data + 5 + 16, sizeof v0.m);
    mat4 eye0 = m4_inverse(v0);
    vec3 hf = m4_mul_dir(eye0, v3(0, 0, -1));
    float head_yaw = atan2f(hf.x, -hf.z);
    float d = head_yaw;
    float mx = data[0], my = -data[1];
    g->input.move_x = my * sinf(d) + mx * cosf(d);
    g->input.move_y = my * cosf(d) - mx * sinf(d);
    float rx = data[2];
    if (fabsf(rx) > 0.7f && !turned) {
        mtn_input_set(&g->input, rx > 0 ? MTN_ACT_TURN_RIGHT : MTN_ACT_TURN_LEFT, 1);
        turned = 1;
    } else if (fabsf(rx) < 0.3f && turned) {
        mtn_input_set(&g->input, MTN_ACT_TURN_RIGHT, 0);
        mtn_input_set(&g->input, MTN_ACT_TURN_LEFT, 0);
        turned = 0;
    }
    int b = (int)data[4];
    static const int map[7] = {MTN_ACT_INTERACT, MTN_ACT_INTERACT, MTN_ACT_JUMP, MTN_ACT_SHOOT, MTN_ACT_MENU, MTN_ACT_HOUSE, MTN_ACT_RUN};
    for (int i = 0; i < 7; i++)
        if (((b ^ prev_buttons) >> i) & 1) mtn_input_set(&g->input, map[i], (b >> i) & 1);
    prev_buttons = b;
    lock_audio(a);
    mtn_game_update(g, dt);
    mtn_ui_begin(&g->ui, (float)width, (float)height, &g->input);
    mtn_script_ui(g);
    unlock_audio(a);
    /* the player's feet and body yaw anchor the XR local-floor space */
    mat4 world_from_local = m4_mul(m4_translate(g->player.pos), m4_rotate_y(-g->player.yaw));
    mat4 local_from_world = m4_inverse(world_from_local);
    for (int i = 0; i < views; i++) {
        const float *vd = data + 5 + i * 36;
        mat4 proj, view;
        memcpy(proj.m, vd, sizeof proj.m);
        memcpy(view.m, vd + 16, sizeof view.m);
        mat4 v = m4_mul(view, local_from_world);
        vec3 eye = m4_mul_point(m4_inverse(v), v3(0, 0, 0));
        mtn_render_view(a->r, g, v, proj, eye, (unsigned)fbo, (int)vd[32], (int)vd[33], (int)vd[34], (int)vd[35], 0);
    }
    lock_audio(a);
    handle_requests(a);
    unlock_audio(a);
}

EMSCRIPTEN_KEEPALIVE void mtn_web_xr_ended(void) {
    if (g_app) {
        g_app->g.xr_active = 0;
        g_app->g.input.mode = MTN_MODE_KEYBOARD;
    }
}
#endif

/* ------------------------------------------------------------------ main */

static void parse_args(app *a, int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        const char *s = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (strcmp(s, "--test") == 0 && v) {
            snprintf(a->test_path, sizeof a->test_path, "%s", v);
            a->g.test_mode = 1;
            a->fixed_dt = 1;
            i++;
        } else if (strcmp(s, "--data-dir") == 0 && v) {
            snprintf(a->data_dir, sizeof a->data_dir, "%s", v);
            i++;
        } else if (strcmp(s, "--assets") == 0 && v) {
            mtn_vfs_init(v);
            i++;
        } else if (strcmp(s, "--frames") == 0 && v) {
            a->frame_limit = atoi(v);
            i++;
        } else if (strcmp(s, "--fixed-dt") == 0) {
            a->fixed_dt = 1;
        }
    }
}

static int init(app *a, int argc, char **argv) {
    const char *base = SDL_GetBasePath();
    mtn_vfs_init(base ? base : "");
    parse_args(a, argc, argv);
    char user[600];
#ifdef __EMSCRIPTEN__
    snprintf(user, sizeof user, "/user/");
#else
    if (a->data_dir[0]) {
        size_t l = strlen(a->data_dir);
        snprintf(user, sizeof user, "%s%s", a->data_dir, (l && a->data_dir[l - 1] != '/') ? "/" : "");
    } else {
        char *pref = SDL_GetPrefPath("makesgames", "mountain");
        snprintf(user, sizeof user, "%s", pref ? pref : "./");
        SDL_free(pref);
    }
#endif
    ensure_dir(user);
    char photos[700];
    snprintf(photos, sizeof photos, "%sphotos", user);
    ensure_dir(photos);

    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 0;
    }
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) SDL_Log("no audio: %s", SDL_GetError());
#ifdef __EMSCRIPTEN__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#if defined(__EMSCRIPTEN__) && defined(SDL_WINDOW_FILL_DOCUMENT)
    flags |= SDL_WINDOW_FILL_DOCUMENT;
#endif
    int w = a->g.test_mode ? 640 : 1280, h = a->g.test_mode ? 360 : 720;
    a->win = SDL_CreateWindow("Mountain", w, h, flags);
    if (!a->win) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 0;
    }
    a->gl = SDL_GL_CreateContext(a->win);
    if (!a->gl) {
        fprintf(stderr, "OpenGL context failed: %s\n", SDL_GetError());
        return 0;
    }
    SDL_GL_MakeCurrent(a->win, a->gl);
    SDL_GL_SetSwapInterval(a->g.test_mode ? 0 : 1);
    if (!mtn_gl_load(SDL_GL_GetProcAddress)) {
        fprintf(stderr, "OpenGL 3.3 functions missing\n");
        return 0;
    }
    char err[1100];
    char probe[1024];
    if (!mtn_vfs_resolve("scripts/main.lua", probe, sizeof probe)) {
        snprintf(err, sizeof err, "Mountain cannot find its game files.\nExpected an 'assets' folder here:\n%sassets/",
                 mtn_vfs_base());
        fprintf(stderr, "%s\n", err);
        if (!a->g.test_mode) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Mountain", err, a->win);
        return 0;
    }
    a->r = mtn_render_create();
    if (!mtn_render_load(a->r, err, sizeof err)) {
        fprintf(stderr, "renderer: %s\n", err);
        if (!a->g.test_mode) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Mountain", err, a->win);
        return 0;
    }
    int test_mode = a->g.test_mode;
    mtn_game_init(&a->g, PLATFORM_NAME, user);
    a->g.test_mode = test_mode;
    default_keys(a);
    mtn_game_load_sounds(&a->g);
    scan_gallery(a);
    open_audio(a);
#ifdef __EMSCRIPTEN__
    mtn_js_xr_init();
    if (mtn_js_is_touch()) a->g.input.mode = MTN_MODE_TOUCH;
#endif
    lock_audio(a);
    mtn_script_init(&a->g);
    unlock_audio(a);
    apply_bindings(a);
    if (a->g.test_mode && !test_start(a)) return 0;
    a->running = 1;
    return 1;
}

static void shutdown_app(app *a) {
    if (a->audio) SDL_DestroyAudioStream(a->audio);
    a->audio = NULL;
    if (a->test.pad) {
        SDL_JoystickID id = SDL_GetJoystickID(a->test.pad);
        SDL_CloseJoystick(a->test.pad);
        SDL_DetachVirtualJoystick(id);
    }
    if (a->pad) SDL_CloseGamepad(a->pad);
    mtn_game_shutdown(&a->g);
    mtn_render_destroy(a->r);
    if (a->gl) SDL_GL_DestroyContext(a->gl);
    if (a->win) SDL_DestroyWindow(a->win);
    SDL_Quit();
}

int mtn_app_main(int argc, char **argv) {
    static app a;
    memset(&a, 0, sizeof a);
    g_app = &a;
    if (!init(&a, argc, argv)) {
        shutdown_app(&a);
        return 2;
    }
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(web_frame, &a, 0, 1);
    return 0;
#else
    while (a.running) frame(&a, frame_dt(&a));
    int code = 0;
    if (a.g.test_mode) {
        code = a.test.failures ? 1 : 0;
        printf("[test] %s: %d checks, %d failures%s%s\n", a.test_path, a.test.checks, a.test.failures,
               a.test.first_failure[0] ? " - first: " : "", a.test.first_failure);
    }
    shutdown_app(&a);
    return code;
#endif
}
