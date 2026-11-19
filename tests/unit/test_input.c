#include "test.h"
#include "mtn/input.h"

#include <stdlib.h>

static void test_actions(void) {
    CHECK_STR(mtn_action_name(MTN_ACT_FORWARD), "forward");
    CHECK_STR(mtn_action_name(MTN_ACT_HOUSE), "house");
    CHECK_STR(mtn_action_name(MTN_ACT_DOWNLOAD), "download");
    CHECK_STR(mtn_action_name(-1), "unknown");
    CHECK_STR(mtn_action_name(MTN_ACT_COUNT), "unknown");
    for (int a = 0; a < MTN_ACT_COUNT; a++) CHECK(mtn_action_from_name(mtn_action_name(a)) == a);
    CHECK(mtn_action_from_name("fly") == -1);
    CHECK(mtn_action_from_name("unknown") == -1);
}

static void test_digital(void) {
    mtn_input in;
    mtn_input_init(&in);
    CHECK(in.down == 0 && in.mode == MTN_MODE_KEYBOARD);
    mtn_input_set(&in, MTN_ACT_JUMP, 1);
    CHECK(mtn_input_down(&in, MTN_ACT_JUMP));
    CHECK(mtn_input_pressed(&in, MTN_ACT_JUMP));
    CHECK(!mtn_input_released(&in, MTN_ACT_JUMP));
    CHECK(!mtn_input_down(&in, MTN_ACT_RUN));
    /* repeated "down" does not press again */
    mtn_input_begin_frame(&in);
    mtn_input_set(&in, MTN_ACT_JUMP, 1);
    CHECK(mtn_input_down(&in, MTN_ACT_JUMP));
    CHECK(!mtn_input_pressed(&in, MTN_ACT_JUMP));
    mtn_input_set(&in, MTN_ACT_JUMP, 0);
    CHECK(!mtn_input_down(&in, MTN_ACT_JUMP));
    CHECK(mtn_input_released(&in, MTN_ACT_JUMP));
    /* releasing an action that is up is a no-op */
    mtn_input_begin_frame(&in);
    mtn_input_set(&in, MTN_ACT_JUMP, 0);
    CHECK(!mtn_input_released(&in, MTN_ACT_JUMP));
    /* invalid actions are ignored */
    mtn_input_set(&in, -1, 1);
    mtn_input_set(&in, MTN_ACT_COUNT, 1);
    mtn_input_set(&in, 31, 1);
    CHECK(in.down == 0 && in.pressed == 0);
    CHECK(!mtn_input_down(&in, -1) && !mtn_input_pressed(&in, MTN_ACT_COUNT) && !mtn_input_released(&in, 40));
    /* the last action works like any other */
    mtn_input_set(&in, MTN_ACT_DOWNLOAD, 1);
    CHECK(mtn_input_pressed(&in, MTN_ACT_DOWNLOAD));
    /* begin_frame clears per-frame state but keeps held keys and pointer */
    in.look_x = 1;
    in.look_y = 2;
    in.wheel = 3;
    mtn_input_pointer(&in, 10, 20, 1);
    mtn_input_begin_frame(&in);
    CHECK(in.pressed == 0 && in.released == 0);
    CHECK(in.look_x == 0 && in.look_y == 0 && in.wheel == 0);
    CHECK(in.pointer_pressed == 0 && in.pointer_down == 1);
    CHECK(mtn_input_down(&in, MTN_ACT_DOWNLOAD));
}

static void test_move_and_pointer(void) {
    mtn_input in;
    mtn_input_init(&in);
    float x, y;
    mtn_input_move(&in, &x, &y);
    CHECK(x == 0 && y == 0);
    mtn_input_set(&in, MTN_ACT_FORWARD, 1);
    mtn_input_move(&in, &x, &y);
    CHECK_NEAR(x, 0, 0);
    CHECK_NEAR(y, 1, 0);
    mtn_input_set(&in, MTN_ACT_RIGHT, 1);
    mtn_input_move(&in, &x, &y);
    CHECK_NEAR(x, sqrtf(0.5f), 1e-6);
    CHECK_NEAR(y, sqrtf(0.5f), 1e-6);
    mtn_input_set(&in, MTN_ACT_FORWARD, 0);
    mtn_input_set(&in, MTN_ACT_RIGHT, 0);
    mtn_input_set(&in, MTN_ACT_BACK, 1);
    mtn_input_set(&in, MTN_ACT_LEFT, 1);
    mtn_input_move(&in, &x, &y);
    CHECK(x < 0 && y < 0);
    CHECK_NEAR(x * x + y * y, 1, 1e-5);
    mtn_input_init(&in);
    in.move_x = 0.3f;
    in.move_y = 0.4f;
    mtn_input_move(&in, &x, &y); /* analog below 1 is kept */
    CHECK_NEAR(x, 0.3f, 1e-6);
    CHECK_NEAR(y, 0.4f, 1e-6);
    in.move_x = 1.0f;
    in.move_y = 0.0f;
    mtn_input_set(&in, MTN_ACT_RIGHT, 1);
    mtn_input_move(&in, &x, &y);
    CHECK_NEAR(x, 1, 1e-6);
    CHECK_NEAR(y, 0, 1e-6);

    mtn_input_init(&in);
    mtn_input_pointer(&in, 5, 6, 1);
    CHECK(in.pointer_pressed && !in.pointer_released && in.pointer_down);
    CHECK(in.pointer_x == 5 && in.pointer_y == 6);
    mtn_input_begin_frame(&in);
    mtn_input_pointer(&in, 7, 8, 1);
    CHECK(!in.pointer_pressed && in.pointer_down);
    mtn_input_pointer(&in, 9, 9, 0);
    CHECK(in.pointer_released && !in.pointer_down);
    mtn_input_begin_frame(&in);
    mtn_input_pointer(&in, 9, 9, 0);
    CHECK(!in.pointer_released && !in.pointer_pressed);
}

static int find_button(const mtn_touch *t, int action) {
    for (int i = 0; i < t->button_count; i++)
        if (t->buttons[i].action == action) return i;
    return -1;
}

static void test_touch_layout(void) {
    mtn_touch t;
    mtn_touch_init(&t);
    CHECK(t.stick_finger == -1 && t.look_finger == -1);
    CHECK(t.look_sensitivity > 0);
    mtn_touch_layout(&t, 1000, 500, 0);
    CHECK(t.w == 1000 && t.h == 500 && t.context == 0);
    CHECK_NEAR(t.stick_r, 65, 1e-4);
    CHECK_NEAR(t.stick_cx, 100, 1e-4);
    CHECK_NEAR(t.stick_cy, 390, 1e-4);
    CHECK(t.button_count == 6);
    int acts[6] = {MTN_ACT_JUMP, MTN_ACT_INTERACT, MTN_ACT_CAMERA, MTN_ACT_HOUSE, MTN_ACT_MENU, MTN_ACT_GALLERY};
    for (int i = 0; i < 6; i++) {
        int b = find_button(&t, acts[i]);
        CHECK(b >= 0);
        if (b < 0) continue;
        CHECK(t.buttons[b].finger == -1);
        CHECK(t.buttons[b].r > 0);
        /* every button is on screen, on the right half */
        CHECK(t.buttons[b].x > 500 && t.buttons[b].x < 1000 && t.buttons[b].y > 0 && t.buttons[b].y < 500);
    }
    CHECK(t.buttons[find_button(&t, MTN_ACT_INTERACT)].icon == 28);
    mtn_touch_layout(&t, 1000, 500, MTN_TOUCH_CTX_NEAR_DOOR);
    CHECK(t.buttons[find_button(&t, MTN_ACT_INTERACT)].icon == 24);
    /* camera context: shutter, zoom, filter, exit */
    mtn_touch_layout(&t, 1000, 500, MTN_TOUCH_CTX_CAMERA);
    CHECK(t.button_count == 5);
    CHECK(find_button(&t, MTN_ACT_SHOOT) >= 0 && find_button(&t, MTN_ACT_ZOOM_IN) >= 0);
    CHECK(find_button(&t, MTN_ACT_ZOOM_OUT) >= 0 && find_button(&t, MTN_ACT_FILTER) >= 0);
    CHECK(find_button(&t, MTN_ACT_CAMERA) >= 0);
    CHECK(find_button(&t, MTN_ACT_JUMP) == -1);
    int sh = find_button(&t, MTN_ACT_SHOOT);
    CHECK_NEAR(t.buttons[sh].x, 910, 1e-3);
    CHECK_NEAR(t.buttons[sh].y, 250, 1e-3);
    CHECK_NEAR(t.buttons[sh].r, 50, 1e-3);
    /* portrait screens use the short side as the unit */
    mtn_touch_layout(&t, 400, 800, 0);
    CHECK_NEAR(t.stick_r, 52, 1e-4);
    CHECK_NEAR(t.stick_cy, 800 - 88, 1e-3);
}

static void test_touch_buttons(void) {
    mtn_touch t;
    mtn_input in;
    mtn_touch_init(&t);
    mtn_input_init(&in);
    mtn_touch_layout(&t, 1000, 500, 0);
    int jb = find_button(&t, MTN_ACT_JUMP);
    mtn_touch_button jump = t.buttons[jb];
    /* slightly outside the drawn radius still counts */
    CHECK(mtn_touch_down(&t, &in, 1, jump.x + jump.r * 1.1f, jump.y));
    CHECK(mtn_input_pressed(&in, MTN_ACT_JUMP) && mtn_input_down(&in, MTN_ACT_JUMP));
    CHECK(t.buttons[jb].finger == 1);
    CHECK(t.stick_finger == -1 && t.look_finger == -1);
    mtn_touch_up(&t, &in, 1);
    CHECK(!mtn_input_down(&in, MTN_ACT_JUMP) && mtn_input_released(&in, MTN_ACT_JUMP));
    CHECK(t.buttons[jb].finger == -1);

    /* holding the camera button survives a re-layout into the camera context */
    int cb = find_button(&t, MTN_ACT_CAMERA);
    CHECK(mtn_touch_down(&t, &in, 3, t.buttons[cb].x, t.buttons[cb].y));
    CHECK(mtn_input_down(&in, MTN_ACT_CAMERA));
    mtn_touch_layout(&t, 1000, 500, MTN_TOUCH_CTX_CAMERA);
    int cb2 = find_button(&t, MTN_ACT_CAMERA);
    CHECK(t.buttons[cb2].finger == 3);
    CHECK(t.buttons[find_button(&t, MTN_ACT_SHOOT)].finger == -1);
    mtn_touch_up(&t, &in, 3);
    CHECK(!mtn_input_down(&in, MTN_ACT_CAMERA));
    CHECK(t.buttons[cb2].finger == -1);
    /* lifting an unknown finger changes nothing */
    in.down = 0;
    mtn_touch_up(&t, &in, 77);
    CHECK(in.down == 0);
}

static void test_touch_stick_look(void) {
    mtn_touch t;
    mtn_input in;
    mtn_touch_init(&t);
    mtn_input_init(&in);
    mtn_touch_layout(&t, 1000, 500, 0);
    /* stick appears where the finger lands in the lower left */
    CHECK(mtn_touch_down(&t, &in, 10, 120, 400));
    CHECK(t.stick_finger == 10);
    CHECK(t.stick_cx == 120 && t.stick_cy == 400);
    mtn_touch_move(&t, &in, 10, 120 + 32.5f, 400 - 32.5f);
    CHECK_NEAR(t.stick_x, 0.5f, 1e-5);
    CHECK_NEAR(t.stick_y, -0.5f, 1e-5);
    CHECK_NEAR(in.move_x, 0.5f, 1e-5);
    CHECK_NEAR(in.move_y, 0.5f, 1e-5); /* screen up = forward */
    CHECK(!in.analog_run);
    mtn_touch_move(&t, &in, 10, 120 + 650, 400); /* far: clamped, running */
    CHECK_NEAR(t.stick_x, 1, 1e-5);
    CHECK_NEAR(t.stick_y, 0, 1e-5);
    CHECK(in.analog_run);
    /* re-layout keeps the stick where it is while held */
    mtn_touch_layout(&t, 1000, 500, 0);
    CHECK(t.stick_cx == 120 && t.stick_cy == 400);

    /* a second finger on the left in the stick area becomes the look finger */
    CHECK(mtn_touch_down(&t, &in, 11, 200, 450));
    CHECK(t.look_finger == 11);
    mtn_touch_move(&t, &in, 11, 250, 430);
    CHECK_NEAR(in.look_x, 50 * t.look_sensitivity, 1e-6);
    CHECK_NEAR(in.look_y, 20 * t.look_sensitivity, 1e-6);
    mtn_touch_move(&t, &in, 11, 260, 430);
    CHECK_NEAR(in.look_x, 60 * t.look_sensitivity, 1e-6);
    /* a third finger off the buttons has nothing left to do */
    CHECK(!mtn_touch_down(&t, &in, 12, 600, 100));
    mtn_touch_move(&t, &in, 12, 700, 200);
    CHECK_NEAR(in.look_x, 60 * t.look_sensitivity, 1e-6);
    /* but buttons still work */
    int hb = find_button(&t, MTN_ACT_HOUSE);
    CHECK(mtn_touch_down(&t, &in, 13, t.buttons[hb].x, t.buttons[hb].y));
    CHECK(mtn_input_down(&in, MTN_ACT_HOUSE));

    mtn_touch_up(&t, &in, 10);
    CHECK(t.stick_finger == -1);
    CHECK(in.move_x == 0 && in.move_y == 0 && !in.analog_run && t.stick_x == 0);
    CHECK(t.look_finger == 11);
    mtn_touch_up(&t, &in, 11);
    CHECK(t.look_finger == -1);
    /* with the stick free, a touch on the upper left is a look, not the stick */
    CHECK(mtn_touch_down(&t, &in, 14, 100, 50));
    CHECK(t.look_finger == 14 && t.stick_finger == -1);
    CHECK(mtn_touch_down(&t, &in, 15, 100, 300));
    CHECK(t.stick_finger == 15);

    /* reset releases everything */
    mtn_touch_move(&t, &in, 15, 100, 250);
    mtn_touch_reset(&t, &in);
    CHECK(t.stick_finger == -1 && t.look_finger == -1);
    CHECK(in.move_y == 0);
    CHECK(!mtn_input_down(&in, MTN_ACT_HOUSE));
    for (int i = 0; i < t.button_count; i++) CHECK(t.buttons[i].finger == -1);
    mtn_touch_reset(&t, &in); /* idempotent */
    CHECK(t.stick_finger == -1);
}

static void test_ui_basics(void) {
    CHECK_NEAR(mtn_ui_scale_for(1920, 1080), 4, 0);
    CHECK_NEAR(mtn_ui_scale_for(2560, 1440), 5, 0);
    CHECK_NEAR(mtn_ui_scale_for(800, 600), 1, 0);
    CHECK_NEAR(mtn_ui_scale_for(320, 200), 1, 0);
    CHECK_NEAR(mtn_ui_scale_for(3840, 600), 2, 0);

    mtn_ui ui;
    mtn_input in;
    mtn_ui_init(&ui);
    mtn_input_init(&in);
    CHECK(ui.q == NULL && ui.count == 0);
    mtn_input_pointer(&in, 400, 200, 0);
    mtn_ui_begin(&ui, 1920, 1080, &in);
    CHECK_NEAR(ui.scale, 4, 0);
    CHECK_NEAR(ui.w, 480, 0);
    CHECK_NEAR(ui.h, 270, 0);
    CHECK_NEAR(ui.px, 100, 0);
    CHECK_NEAR(ui.py, 50, 0);
    CHECK(mtn_ui_hover(&ui, 100, 50, 1, 1));
    CHECK(!mtn_ui_hover(&ui, 101, 50, 10, 10));
    CHECK(!mtn_ui_hover(&ui, 90, 40, 10, 10)); /* right/bottom edges are exclusive */
    CHECK(mtn_ui_hover(&ui, 90, 40, 10.5f, 10.5f));

    mtn_ui_rect(&ui, 1, 2, 3, 4, 0x11223344u);
    CHECK(ui.count == 1);
    mtn_ui_quad q = ui.q[0];
    CHECK(q.x == 1 && q.y == 2 && q.w == 3 && q.h == 4);
    CHECK(q.u0 == 0 && q.v0 == 0 && q.u1 == 1 && q.v1 == 1);
    CHECK(q.color == 0x11223344u && q.tex == MTN_UI_TEX_WHITE);
    mtn_ui_image(&ui, 5, 6, 7, 8, MTN_UI_TEX_PHOTO, 0.1f, 0.2f, 0.3f, 0.4f, 0xFFu);
    CHECK(ui.q[1].tex == MTN_UI_TEX_PHOTO && ui.q[1].v1 == 0.4f && ui.q[1].color == 0xFFu);
    /* the quad buffer grows */
    for (int i = 0; i < 700; i++) mtn_ui_rect(&ui, (float)i, 0, 1, 1, (uint32_t)i);
    CHECK(ui.count == 702);
    CHECK(ui.cap >= 702);
    CHECK(ui.q[701].x == 699 && ui.q[701].color == 699u);
    CHECK(ui.q[0].color == 0x11223344u);

    /* icons address a 16x16 atlas */
    ui.count = 0;
    mtn_ui_icon(&ui, 10, 20, 16, 37, 0xFFFFFFFFu);
    CHECK(ui.count == 1 && ui.q[0].tex == MTN_UI_TEX_ICONS);
    CHECK_NEAR(ui.q[0].u0, 5.0f / 16.0f, 1e-6);
    CHECK_NEAR(ui.q[0].v0, 2.0f / 16.0f, 1e-6);
    CHECK_NEAR(ui.q[0].u1 - ui.q[0].u0, 1.0f / 16.0f, 1e-6);
    CHECK(ui.q[0].w == 16 && ui.q[0].h == 16);

    /* a new frame starts with an empty list */
    mtn_ui_begin(&ui, 800, 600, &in);
    CHECK(ui.count == 0 && ui.scale == 1);
    mtn_ui_free(&ui);
    CHECK(ui.q == NULL && ui.cap == 0);
}

static void test_ui_text(void) {
    CHECK_NEAR(mtn_ui_text_width("", 1), 0, 0);
    CHECK_NEAR(mtn_ui_text_width("abc", 1), 24, 0);
    CHECK_NEAR(mtn_ui_text_width("ab\ncde\nf", 2), 48, 0);
    mtn_ui ui;
    mtn_input in;
    mtn_ui_init(&ui);
    mtn_input_init(&in);
    mtn_ui_begin(&ui, 480, 270, &in);
    float w = mtn_ui_text(&ui, 10, 20, 1, "Hi\n A\xC3", 0xFFFFFFFFu);
    CHECK_NEAR(w, 24, 0);
    CHECK(ui.count == 8); /* 4 glyphs (spaces are skipped) with shadows */
    /* shadow first, offset by the size, half transparent black */
    CHECK(ui.q[0].x == 11 && ui.q[0].y == 21 && ui.q[0].color == 0x7F000000u);
    CHECK(ui.q[1].x == 10 && ui.q[1].y == 20 && ui.q[1].color == 0xFFFFFFFFu);
    CHECK(ui.q[1].tex == MTN_UI_TEX_FONT);
    CHECK_NEAR(ui.q[1].u0, (float)('H' % 16) * 8.0f / 128.0f, 1e-6);
    CHECK_NEAR(ui.q[1].v0, (float)('H' / 16) * 8.0f / 64.0f, 1e-6);
    CHECK_NEAR(ui.q[1].u1 - ui.q[1].u0, 8.0f / 128.0f, 1e-6);
    CHECK_NEAR(ui.q[1].v1 - ui.q[1].v0, 8.0f / 64.0f, 1e-6);
    /* "A" on the second line after a space */
    CHECK(ui.q[5].x == 18 && ui.q[5].y == 30);
    /* non-ASCII becomes '?' */
    CHECK(ui.q[7].x == 26);
    CHECK_NEAR(ui.q[7].u0, 15.0f * 8.0f / 128.0f, 1e-6);
    CHECK_NEAR(ui.q[7].v0, 3.0f * 8.0f / 64.0f, 1e-6);
    /* size scales everything */
    ui.count = 0;
    w = mtn_ui_text(&ui, 0, 0, 3, "ab", 0x80FFFFFFu);
    CHECK_NEAR(w, 48, 0);
    CHECK(ui.q[2].x == 27 && ui.q[3].x == 24 && ui.q[3].w == 24);
    CHECK(ui.q[0].color == 0x40000000u);
    mtn_ui_free(&ui);
}

static void ui_frame(mtn_ui *ui, mtn_input *in, float x, float y, int down) {
    mtn_input_begin_frame(in);
    mtn_input_pointer(in, x, y, down);
    mtn_ui_begin(ui, 480, 270, in);
}

static void test_ui_button(void) {
    mtn_ui ui;
    mtn_input in;
    mtn_ui_init(&ui);
    mtn_input_init(&in);
    /* idle, not hovered */
    ui_frame(&ui, &in, 0, 0, 0);
    CHECK(!mtn_ui_button(&ui, "ok", 10, 10, 50, 20, "OK", -1));
    CHECK(!ui.consumed);
    CHECK(ui.count == 2 + 2 * 2); /* background, edge, two glyphs with shadows */
    CHECK(ui.q[0].color == 0xB0283850u);
    /* hovering */
    ui_frame(&ui, &in, 20, 20, 0);
    CHECK(!mtn_ui_button(&ui, "ok", 10, 10, 50, 20, "OK", -1));
    CHECK(ui.q[0].color == 0xD0385070u);
    CHECK(!ui.consumed);
    /* press inside: becomes active, not clicked yet */
    ui_frame(&ui, &in, 20, 20, 1);
    CHECK(!mtn_ui_button(&ui, "ok", 10, 10, 50, 20, "OK", 3));
    CHECK(ui.consumed);
    CHECK(ui.active != 0);
    CHECK(ui.q[0].color == 0xE0406890u);
    CHECK(ui.count == 2 + 1 + 4); /* with an icon */
    CHECK(ui.q[2].tex == MTN_UI_TEX_ICONS);
    CHECK(ui.q[4].x == 10 + 4 + 18); /* label after the icon */
    /* other buttons are not affected by the drag */
    CHECK(!mtn_ui_button(&ui, "cancel", 100, 10, 50, 20, NULL, -1));
    /* hold */
    ui_frame(&ui, &in, 21, 21, 1);
    CHECK(!mtn_ui_button(&ui, "ok", 10, 10, 50, 20, "OK", -1));
    CHECK(ui.consumed);
    /* release inside: click */
    ui_frame(&ui, &in, 21, 21, 0);
    CHECK(mtn_ui_button(&ui, "ok", 10, 10, 50, 20, "OK", -1));
    CHECK(ui.consumed);
    /* next frame forgets the active widget */
    ui_frame(&ui, &in, 21, 21, 0);
    CHECK(ui.active == 0);
    CHECK(!mtn_ui_button(&ui, "ok", 10, 10, 50, 20, "OK", -1));

    /* press on one button and release over another: no click */
    ui_frame(&ui, &in, 20, 20, 1);
    mtn_ui_button(&ui, "ok", 10, 10, 50, 20, "OK", -1);
    mtn_ui_button(&ui, "cancel", 100, 10, 50, 20, "", -1);
    ui_frame(&ui, &in, 110, 20, 0);
    CHECK(!mtn_ui_button(&ui, "ok", 10, 10, 50, 20, "OK", -1));
    CHECK(!mtn_ui_button(&ui, "cancel", 100, 10, 50, 20, "", -1));
    CHECK(ui.count == 2 + 2 * 2 + 2); /* the empty label draws no text */
    CHECK(ui.consumed);
    /* releasing outside every button */
    ui_frame(&ui, &in, 20, 20, 1);
    mtn_ui_button(&ui, "ok", 10, 10, 50, 20, "OK", -1);
    ui_frame(&ui, &in, 300, 200, 0);
    CHECK(!mtn_ui_button(&ui, "ok", 10, 10, 50, 20, "OK", -1));
    CHECK(!ui.consumed);
    mtn_ui_free(&ui);
}

static void test_ui_focus_navigation(void) {
    mtn_ui ui;
    mtn_input in;
    mtn_ui_init(&ui);
    mtn_input_init(&in);
    /* frame 1: navigation disabled, buttons are only counted */
    mtn_ui_begin(&ui, 960, 540, &in);
    CHECK(!mtn_ui_button(&ui, "a", 0, 0, 10, 10, "A", -1));
    CHECK(!mtn_ui_button(&ui, "b", 0, 20, 10, 10, "B", -1));
    CHECK(ui.buttons == 2);
    /* frame 2: a menu is open, "down" shows the focus on the first button */
    ui.nav_enabled = 1;
    mtn_input_begin_frame(&in);
    mtn_input_set(&in, MTN_ACT_BACK, 1);
    mtn_ui_begin(&ui, 960, 540, &in);
    CHECK(ui.focus_visible && ui.focus == 0);
    mtn_ui_button(&ui, "a", 0, 0, 10, 10, "A", -1);
    mtn_ui_button(&ui, "b", 0, 20, 10, 10, "B", -1);
    /* frame 3: "down" again moves to the second; "up" twice wraps around */
    mtn_input_begin_frame(&in);
    mtn_input_set(&in, MTN_ACT_BACK, 0);
    mtn_input_set(&in, MTN_ACT_BACK, 1);
    mtn_ui_begin(&ui, 960, 540, &in);
    CHECK(ui.focus == 1);
    mtn_ui_button(&ui, "a", 0, 0, 10, 10, "A", -1);
    mtn_ui_button(&ui, "b", 0, 20, 10, 10, "B", -1);
    mtn_input_begin_frame(&in);
    mtn_input_set(&in, MTN_ACT_BACK, 0);
    mtn_input_set(&in, MTN_ACT_ZOOM_IN, 1);
    mtn_ui_begin(&ui, 960, 540, &in);
    CHECK(ui.focus == 0);
    mtn_ui_button(&ui, "a", 0, 0, 10, 10, "A", -1);
    mtn_ui_button(&ui, "b", 0, 20, 10, 10, "B", -1);
    /* frame 4: A (jump) presses the focused button */
    mtn_input_begin_frame(&in);
    mtn_input_set(&in, MTN_ACT_ZOOM_IN, 0);
    mtn_input_set(&in, MTN_ACT_JUMP, 1);
    mtn_ui_begin(&ui, 960, 540, &in);
    CHECK(mtn_ui_button(&ui, "a", 0, 0, 10, 10, "A", -1));
    CHECK(!mtn_ui_button(&ui, "b", 0, 20, 10, 10, "B", -1));
    /* a pointer press hides the keyboard focus again; closing the menu too */
    mtn_input_begin_frame(&in);
    mtn_input_pointer(&in, 500, 500, 1);
    mtn_ui_begin(&ui, 960, 540, &in);
    CHECK(!ui.focus_visible);
    ui.nav_enabled = 0;
    mtn_ui_begin(&ui, 960, 540, &in);
    CHECK(!ui.focus_visible);
    mtn_ui_free(&ui);
}

MTN_SUITE(input) {
    TEST_RUN(test_ui_focus_navigation);
    TEST_RUN(test_actions);
    TEST_RUN(test_digital);
    TEST_RUN(test_move_and_pointer);
    TEST_RUN(test_touch_layout);
    TEST_RUN(test_touch_buttons);
    TEST_RUN(test_touch_stick_look);
    TEST_RUN(test_ui_basics);
    TEST_RUN(test_ui_text);
    TEST_RUN(test_ui_button);
}
