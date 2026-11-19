/* Mountain engine - abstract input actions, touch controls layout, immediate mode UI. MIT License. */
#include "mtn/input.h"
#include "mtn/math.h"
#include "mtn/rng.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char *ACTIONS[MTN_ACT_COUNT] = {
    "forward", "back", "left", "right", "run", "jump", "interact", "house", "camera", "shoot",
    "gallery", "menu", "zoom_in", "zoom_out", "filter", "prev", "next", "delete", "debug", "fullscreen",
    "reload", "turn_left", "turn_right", "download"};

const char *mtn_action_name(int a) { return (a >= 0 && a < MTN_ACT_COUNT) ? ACTIONS[a] : "unknown"; }

int mtn_action_from_name(const char *name) {
    for (int i = 0; i < MTN_ACT_COUNT; i++)
        if (strcmp(ACTIONS[i], name) == 0) return i;
    return -1;
}

void mtn_input_init(mtn_input *in) { memset(in, 0, sizeof *in); }

void mtn_input_begin_frame(mtn_input *in) {
    in->pressed = in->released = 0;
    in->look_x = in->look_y = 0.0f;
    in->pointer_pressed = in->pointer_released = 0;
    in->wheel = 0.0f;
}

void mtn_input_set(mtn_input *in, int a, int down) {
    if (a < 0 || a >= MTN_ACT_COUNT) return;
    uint32_t bit = 1u << a;
    if (down && !(in->down & bit)) {
        in->down |= bit;
        in->pressed |= bit;
    } else if (!down && (in->down & bit)) {
        in->down &= ~bit;
        in->released |= bit;
    }
}

int mtn_input_down(const mtn_input *in, int a) { return a >= 0 && a < MTN_ACT_COUNT && (in->down >> a) & 1u; }
int mtn_input_pressed(const mtn_input *in, int a) { return a >= 0 && a < MTN_ACT_COUNT && (in->pressed >> a) & 1u; }
int mtn_input_released(const mtn_input *in, int a) { return a >= 0 && a < MTN_ACT_COUNT && (in->released >> a) & 1u; }

void mtn_input_move(const mtn_input *in, float *x, float *y) {
    float mx = in->move_x + (float)mtn_input_down(in, MTN_ACT_RIGHT) - (float)mtn_input_down(in, MTN_ACT_LEFT);
    float my = in->move_y + (float)mtn_input_down(in, MTN_ACT_FORWARD) - (float)mtn_input_down(in, MTN_ACT_BACK);
    float l = sqrtf(mx * mx + my * my);
    if (l > 1.0f) {
        mx /= l;
        my /= l;
    }
    *x = mx;
    *y = my;
}

void mtn_input_pointer(mtn_input *in, float x, float y, int down) {
    in->pointer_x = x;
    in->pointer_y = y;
    if (down && !in->pointer_down) in->pointer_pressed = 1;
    if (!down && in->pointer_down) in->pointer_released = 1;
    in->pointer_down = down;
}

/* ---------------------------------------------------------------- touch */

void mtn_touch_init(mtn_touch *t) {
    memset(t, 0, sizeof *t);
    t->stick_finger = -1;
    t->look_finger = -1;
    t->look_sensitivity = 0.006f;
}

static void add_button(mtn_touch *t, float x, float y, float r, int action, int icon) {
    if (t->button_count >= MTN_TOUCH_MAX_BUTTONS) return;
    mtn_touch_button *b = &t->buttons[t->button_count++];
    b->x = x;
    b->y = y;
    b->r = r;
    b->action = action;
    b->icon = icon;
    b->finger = -1;
}

void mtn_touch_layout(mtn_touch *t, float w, float h, int context) {
    /* keep fingers that are still down on buttons with the same action */
    mtn_touch_button old[MTN_TOUCH_MAX_BUTTONS];
    int old_n = t->button_count;
    memcpy(old, t->buttons, sizeof old);
    t->w = w;
    t->h = h;
    t->context = context;
    float u = mtn_minf(w, h) / 10.0f; /* layout unit */
    t->stick_r = u * 1.3f;
    if (t->stick_finger < 0) {
        t->stick_cx = u * 2.0f;
        t->stick_cy = h - u * 2.2f;
    }
    t->button_count = 0;
    if (context & MTN_TOUCH_CTX_CAMERA) {
        add_button(t, w - u * 1.8f, h * 0.5f, u * 1.0f, MTN_ACT_SHOOT, 0);
        add_button(t, w - u * 1.4f, h * 0.5f - u * 2.4f, u * 0.6f, MTN_ACT_ZOOM_IN, 20);
        add_button(t, w - u * 1.4f, h * 0.5f + u * 2.4f, u * 0.6f, MTN_ACT_ZOOM_OUT, 21);
        add_button(t, w - u * 3.6f, h - u * 1.2f, u * 0.6f, MTN_ACT_FILTER, 36);
        add_button(t, w - u * 1.2f, u * 1.2f, u * 0.6f, MTN_ACT_CAMERA, 22);
    } else {
        add_button(t, w - u * 1.5f, h - u * 1.5f, u * 0.8f, MTN_ACT_JUMP, 27);
        add_button(t, w - u * 3.3f, h - u * 1.1f, u * 0.75f, MTN_ACT_INTERACT, (context & MTN_TOUCH_CTX_NEAR_DOOR) ? 24 : 28);
        add_button(t, w - u * 1.3f, h - u * 3.3f, u * 0.65f, MTN_ACT_CAMERA, 0);
        add_button(t, w - u * 3.0f, h - u * 2.9f, u * 0.6f, MTN_ACT_HOUSE, 1);
        add_button(t, w - u * 1.2f, u * 1.2f, u * 0.55f, MTN_ACT_MENU, 29);
        add_button(t, w - u * 2.6f, u * 1.2f, u * 0.55f, MTN_ACT_GALLERY, 2);
    }
    for (int i = 0; i < t->button_count; i++)
        for (int k = 0; k < old_n; k++)
            if (old[k].finger >= 0 && old[k].action == t->buttons[i].action) t->buttons[i].finger = old[k].finger;
}

int mtn_touch_down(mtn_touch *t, mtn_input *in, int64_t finger, float x, float y) {
    for (int i = 0; i < t->button_count; i++) {
        mtn_touch_button *b = &t->buttons[i];
        float dx = x - b->x, dy = y - b->y;
        if (dx * dx + dy * dy <= b->r * b->r * 1.3f) {
            b->finger = finger;
            mtn_input_set(in, b->action, 1);
            return 1;
        }
    }
    if (t->stick_finger < 0 && x < t->w * 0.45f && y > t->h * 0.35f) {
        t->stick_finger = finger;
        t->stick_cx = x;
        t->stick_cy = y;
        t->stick_x = t->stick_y = 0.0f;
        return 1;
    }
    if (t->look_finger < 0) {
        t->look_finger = finger;
        t->look_lx = x;
        t->look_ly = y;
        return 1;
    }
    return 0;
}

void mtn_touch_move(mtn_touch *t, mtn_input *in, int64_t finger, float x, float y) {
    if (finger == t->stick_finger) {
        float dx = (x - t->stick_cx) / t->stick_r, dy = (y - t->stick_cy) / t->stick_r;
        float l = sqrtf(dx * dx + dy * dy);
        if (l > 1.0f) {
            dx /= l;
            dy /= l;
        }
        t->stick_x = dx;
        t->stick_y = dy;
        in->move_x = dx;
        in->move_y = -dy;
        in->analog_run = l > 0.95f;
    } else if (finger == t->look_finger) {
        in->look_x += (x - t->look_lx) * t->look_sensitivity;
        in->look_y -= (y - t->look_ly) * t->look_sensitivity;
        t->look_lx = x;
        t->look_ly = y;
    }
}

void mtn_touch_up(mtn_touch *t, mtn_input *in, int64_t finger) {
    if (finger == t->stick_finger) {
        t->stick_finger = -1;
        t->stick_x = t->stick_y = 0.0f;
        in->move_x = in->move_y = 0.0f;
        in->analog_run = 0;
    }
    if (finger == t->look_finger) t->look_finger = -1;
    for (int i = 0; i < t->button_count; i++)
        if (t->buttons[i].finger == finger) {
            t->buttons[i].finger = -1;
            mtn_input_set(in, t->buttons[i].action, 0);
        }
}

void mtn_touch_reset(mtn_touch *t, mtn_input *in) {
    if (t->stick_finger >= 0) mtn_touch_up(t, in, t->stick_finger);
    t->look_finger = -1;
    for (int i = 0; i < t->button_count; i++)
        if (t->buttons[i].finger >= 0) mtn_touch_up(t, in, t->buttons[i].finger);
}

/* ---------------------------------------------------------------- UI */

void mtn_ui_init(mtn_ui *ui) { memset(ui, 0, sizeof *ui); }

void mtn_ui_free(mtn_ui *ui) {
    free(ui->q);
    memset(ui, 0, sizeof *ui);
}

float mtn_ui_scale_for(float sw, float sh) {
    float s = floorf(mtn_minf(sw / 480.0f, sh / 270.0f));
    return s < 1.0f ? 1.0f : s;
}

void mtn_ui_begin(mtn_ui *ui, float sw, float sh, const mtn_input *in) {
    ui->count = 0;
    ui->scale = mtn_ui_scale_for(sw, sh);
    ui->w = sw / ui->scale;
    ui->h = sh / ui->scale;
    ui->px = in->pointer_x / ui->scale;
    ui->py = in->pointer_y / ui->scale;
    ui->down = in->pointer_down;
    ui->pressed = in->pointer_pressed;
    ui->released = in->pointer_released;
    ui->consumed = 0;
    if (!ui->down && !ui->released) ui->active = 0;
}

void mtn_ui_image(mtn_ui *ui, float x, float y, float w, float h, int tex, float u0, float v0, float u1, float v1,
                  uint32_t color) {
    if (ui->count == ui->cap) {
        ui->cap = ui->cap ? ui->cap * 2 : 512;
        ui->q = realloc(ui->q, sizeof(mtn_ui_quad) * (size_t)ui->cap);
    }
    mtn_ui_quad *q = &ui->q[ui->count++];
    q->x = x; q->y = y; q->w = w; q->h = h;
    q->u0 = u0; q->v0 = v0; q->u1 = u1; q->v1 = v1;
    q->color = color;
    q->tex = tex;
}

void mtn_ui_rect(mtn_ui *ui, float x, float y, float w, float h, uint32_t color) {
    mtn_ui_image(ui, x, y, w, h, MTN_UI_TEX_WHITE, 0, 0, 1, 1, color);
}

float mtn_ui_text_width(const char *s, float size) {
    float best = 0.0f, cur = 0.0f;
    for (; *s; s++) {
        if (*s == '\n') {
            cur = 0.0f;
            continue;
        }
        cur += 8.0f * size;
        if (cur > best) best = cur;
    }
    return best;
}

float mtn_ui_text(mtn_ui *ui, float x, float y, float size, const char *s, uint32_t color) {
    float cx = x, cy = y, maxw = 0.0f;
    for (; *s; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '\n') {
            cx = x;
            cy += 10.0f * size;
            continue;
        }
        if (ch > 127) ch = '?';
        if (ch != ' ') {
            float u0 = (float)(ch % 16) * 8.0f / 128.0f, v0 = (float)(ch / 16) * 8.0f / 64.0f;
            /* soft drop shadow for readability over bright skies */
            mtn_ui_image(ui, cx + size, cy + size, 8 * size, 8 * size, MTN_UI_TEX_FONT, u0, v0, u0 + 8.0f / 128.0f,
                         v0 + 8.0f / 64.0f, (color & 0xFF000000u) >> 1 & 0xFF000000u);
            mtn_ui_image(ui, cx, cy, 8 * size, 8 * size, MTN_UI_TEX_FONT, u0, v0, u0 + 8.0f / 128.0f, v0 + 8.0f / 64.0f, color);
        }
        cx += 8.0f * size;
        if (cx - x > maxw) maxw = cx - x;
    }
    return maxw;
}

void mtn_ui_icon(mtn_ui *ui, float x, float y, float size, int icon, uint32_t color) {
    float u0 = (float)(icon % 16) / 16.0f, v0 = (float)(icon / 16) / 16.0f;
    mtn_ui_image(ui, x, y, size, size, MTN_UI_TEX_ICONS, u0, v0, u0 + 1.0f / 16.0f, v0 + 1.0f / 16.0f, color);
}

int mtn_ui_hover(const mtn_ui *ui, float x, float y, float w, float h) {
    return ui->px >= x && ui->px < x + w && ui->py >= y && ui->py < y + h;
}

int mtn_ui_button(mtn_ui *ui, const char *id, float x, float y, float w, float h, const char *label, int icon) {
    uint32_t hid = mtn_hash_str(id) | 1u;
    int hover = mtn_ui_hover(ui, x, y, w, h);
    if (hover && ui->pressed) ui->active = hid;
    int clicked = hover && ui->released && ui->active == hid;
    if (hover && (ui->pressed || ui->released || ui->down)) ui->consumed = 1;
    uint32_t bg = (ui->active == hid && ui->down) ? 0xE0406890u : (hover ? 0xD0385070u : 0xB0283850u);
    mtn_ui_rect(ui, x, y, w, h, bg);
    mtn_ui_rect(ui, x, y + h - 1, w, 1, 0x60FFFFFFu);
    float tx = x + 4;
    if (icon >= 0) {
        mtn_ui_icon(ui, x + 3, y + (h - 16) * 0.5f, 16, icon, 0xFFFFFFFFu);
        tx += 18;
    }
    if (label && *label) mtn_ui_text(ui, tx, y + (h - 8) * 0.5f, 1.0f, label, 0xFFF0F0F0u);
    return clicked;
}
