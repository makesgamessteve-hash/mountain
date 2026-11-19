/* Mountain engine - abstract input actions, touch controls layout, immediate mode UI. MIT License. */
#ifndef MTN_INPUT_H
#define MTN_INPUT_H

#include <stdint.h>

typedef enum {
    MTN_ACT_FORWARD = 0, MTN_ACT_BACK, MTN_ACT_LEFT, MTN_ACT_RIGHT,
    MTN_ACT_RUN, MTN_ACT_JUMP, MTN_ACT_INTERACT, MTN_ACT_HOUSE, MTN_ACT_CAMERA, MTN_ACT_SHOOT,
    MTN_ACT_GALLERY, MTN_ACT_MENU, MTN_ACT_ZOOM_IN, MTN_ACT_ZOOM_OUT, MTN_ACT_FILTER,
    MTN_ACT_PREV, MTN_ACT_NEXT, MTN_ACT_DELETE, MTN_ACT_DEBUG, MTN_ACT_FULLSCREEN, MTN_ACT_RELOAD,
    MTN_ACT_TURN_LEFT, MTN_ACT_TURN_RIGHT, MTN_ACT_DOWNLOAD,
    MTN_ACT_COUNT
} mtn_action;

typedef enum { MTN_MODE_KEYBOARD = 0, MTN_MODE_TOUCH, MTN_MODE_GAMEPAD, MTN_MODE_XR } mtn_input_mode;

typedef struct {
    uint32_t down, pressed, released;
    float move_x, move_y;   /* analog movement (stick / touch), -1..1 */
    float look_x, look_y;   /* radians accumulated this frame */
    int mode;
    float pointer_x, pointer_y;  /* screen pixels */
    int pointer_down, pointer_pressed, pointer_released;
    float wheel;
    int analog_run;
} mtn_input;

const char *mtn_action_name(int a);
int mtn_action_from_name(const char *name);
void mtn_input_init(mtn_input *in);
void mtn_input_begin_frame(mtn_input *in);
void mtn_input_set(mtn_input *in, int action, int down);
int mtn_input_down(const mtn_input *in, int action);
int mtn_input_pressed(const mtn_input *in, int action);
int mtn_input_released(const mtn_input *in, int action);
/* combined digital + analog movement, length <= 1 */
void mtn_input_move(const mtn_input *in, float *x, float *y);
void mtn_input_pointer(mtn_input *in, float x, float y, int down);

/* ---------------------------------------------------------------- touch */

#define MTN_TOUCH_MAX_BUTTONS 12

enum { MTN_TOUCH_CTX_CAMERA = 1, MTN_TOUCH_CTX_NEAR_DOOR = 2, MTN_TOUCH_CTX_XR = 4 };

typedef struct {
    float x, y, r;
    int action;
    int icon;
    int64_t finger;
} mtn_touch_button;

typedef struct {
    float w, h;
    float stick_cx, stick_cy, stick_r;
    float stick_x, stick_y;      /* knob offset -1..1 */
    int64_t stick_finger;
    int64_t look_finger;
    float look_lx, look_ly;
    float look_sensitivity;      /* radians per pixel */
    mtn_touch_button buttons[MTN_TOUCH_MAX_BUTTONS];
    int button_count;
    int context;
} mtn_touch;

void mtn_touch_init(mtn_touch *t);
void mtn_touch_layout(mtn_touch *t, float w, float h, int context);
int mtn_touch_down(mtn_touch *t, mtn_input *in, int64_t finger, float x, float y);
void mtn_touch_move(mtn_touch *t, mtn_input *in, int64_t finger, float x, float y);
void mtn_touch_up(mtn_touch *t, mtn_input *in, int64_t finger);
void mtn_touch_reset(mtn_touch *t, mtn_input *in);

/* ---------------------------------------------------------------- UI */

enum { MTN_UI_TEX_WHITE = 0, MTN_UI_TEX_FONT = 1, MTN_UI_TEX_ICONS = 2, MTN_UI_TEX_PHOTO = 3 };

typedef struct {
    float x, y, w, h;
    float u0, v0, u1, v1;
    uint32_t color;
    int tex;
} mtn_ui_quad;

typedef struct {
    mtn_ui_quad *q;
    int count, cap;
    float w, h, scale;
    float px, py;
    int down, pressed, released;
    uint32_t active;
    int consumed;    /* a widget used the pointer this frame */
} mtn_ui;

void mtn_ui_init(mtn_ui *ui);
void mtn_ui_free(mtn_ui *ui);
float mtn_ui_scale_for(float screen_w, float screen_h);
void mtn_ui_begin(mtn_ui *ui, float screen_w, float screen_h, const mtn_input *in);
void mtn_ui_image(mtn_ui *ui, float x, float y, float w, float h, int tex, float u0, float v0, float u1, float v1,
                  uint32_t color);
void mtn_ui_rect(mtn_ui *ui, float x, float y, float w, float h, uint32_t color);
float mtn_ui_text(mtn_ui *ui, float x, float y, float size, const char *s, uint32_t color);
float mtn_ui_text_width(const char *s, float size);
void mtn_ui_icon(mtn_ui *ui, float x, float y, float size, int icon, uint32_t color);
int mtn_ui_hover(const mtn_ui *ui, float x, float y, float w, float h);
int mtn_ui_button(mtn_ui *ui, const char *id, float x, float y, float w, float h, const char *label, int icon);

#endif
