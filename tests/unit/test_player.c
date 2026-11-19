#include "test.h"
#include "mtn/player.h"

/* ------------------------------------------------------------------ fake world */

typedef struct {
    float base;       /* ground height for x <= edge_x */
    float edge_x;     /* where the feature starts */
    int feature;      /* 0 none, 1 ramp (slope), 2 step (height), 3 cliff down (height) */
    float slope;      /* dh/dx of the ramp */
    float height;     /* step / cliff height */
    int surface;
    float wall_x;     /* not walkable beyond (when use_wall) */
    int use_wall;
    vec3 post;        /* round obstacle */
    float post_r;
    int ground_calls, push_calls;
} world;

static float w_ground(void *ctx, float x, float z, vec3 *n, int *surf) {
    world *w = ctx;
    (void)z;
    w->ground_calls++;
    *surf = w->surface;
    *n = v3(0, 1, 0);
    if (x <= w->edge_x) return w->base;
    switch (w->feature) {
    case 1:
        *n = v3_norm(v3(-w->slope, 1.0f, 0.0f));
        return w->base + (x - w->edge_x) * w->slope;
    case 2: return w->base + w->height;
    case 3: return w->base - w->height;
    default: return w->base;
    }
}

static int w_push(void *ctx, vec3 *pos, float radius, float height) {
    world *w = ctx;
    (void)height;
    w->push_calls++;
    if (w->post_r <= 0.0f) return 0;
    float d = v3_dist_xz(*pos, w->post), min_d = w->post_r + radius;
    if (d >= min_d) return 0;
    vec3 dir = v3_norm(v3(pos->x - w->post.x, 0, pos->z - w->post.z));
    pos->x = w->post.x + dir.x * min_d;
    pos->z = w->post.z + dir.z * min_d;
    return 1;
}

static int w_walkable(void *ctx, float x, float z) {
    world *w = ctx;
    (void)z;
    return !w->use_wall || x < w->wall_x;
}

static mtn_collider collider(world *w, int full) {
    mtn_collider c;
    c.ctx = w;
    c.ground = w_ground;
    c.push_out = full ? w_push : NULL;
    c.walkable = full ? w_walkable : NULL;
    return c;
}

static mtn_move_input input(float mx, float my) {
    mtn_move_input in;
    memset(&in, 0, sizeof in);
    in.move_x = mx;
    in.move_y = my;
    return in;
}

#define DT (1.0f / 60.0f)

/* ------------------------------------------------------------------ tests */

static void test_init_and_view(void) {
    mtn_player p;
    mtn_player_init(&p, v3(1, 2, 3), 0.0f);
    CHECK_NEAR(p.pos.x, 1, 0);
    CHECK_NEAR(p.pos.y, 2, 0);
    CHECK(p.on_ground);
    CHECK_NEAR(p.eye_height, 1.7f, 0);
    CHECK(p.walk_speed < p.run_speed);
    CHECK(p.max_slope > 0.5f && p.max_slope < 0.8f);
    vec3 eye = mtn_player_eye(&p);
    CHECK_NEAR(eye.y, 3.7f, 1e-6);
    vec3 fwd = mtn_player_forward(&p);
    CHECK_NEAR(fwd.z, -1.0f, 1e-6);
    CHECK_NEAR(fwd.x, 0.0f, 1e-6);
    p.yaw = MTN_PI * 0.5f;
    fwd = mtn_player_forward(&p);
    CHECK_NEAR(fwd.x, 1.0f, 1e-6);
    p.pitch = 0.5f;
    fwd = mtn_player_forward(&p);
    CHECK_NEAR(fwd.y, sinf(0.5f), 1e-6);
    CHECK_NEAR(v3_len(fwd), 1.0f, 1e-5);
    /* the view matrix puts the eye at the origin looking down -z */
    mat4 v = mtn_player_view(&p);
    vec3 o = m4_mul_point(v, eye);
    CHECK_NEAR(v3_len(o), 0.0f, 1e-4);
    vec3 ahead = m4_mul_point(v, v3_add(eye, v3_scale(fwd, 5.0f)));
    CHECK_NEAR(ahead.x, 0.0f, 1e-4);
    CHECK_NEAR(ahead.y, 0.0f, 1e-4);
    CHECK_NEAR(ahead.z, -5.0f, 1e-4);
    /* head bob only on the ground */
    p.bob_phase = MTN_PI * 0.25f;
    CHECK_NEAR(mtn_player_eye(&p).y, 3.7f + 0.035f, 1e-5);
    p.on_ground = 0;
    CHECK_NEAR(mtn_player_eye(&p).y, 3.7f, 1e-6);

    CHECK_STR(mtn_surface_name(MTN_SURF_GRASS), "grass");
    CHECK_STR(mtn_surface_name(MTN_SURF_ROCK), "rock");
    CHECK_STR(mtn_surface_name(MTN_SURF_SNOW), "snow");
    CHECK_STR(mtn_surface_name(MTN_SURF_WOOD), "wood");
    CHECK_STR(mtn_surface_name(MTN_SURF_WATER), "water");
    CHECK_STR(mtn_surface_name(-1), "grass");
    CHECK_STR(mtn_surface_name(5), "grass");
}

static void test_walking(void) {
    world w = {0};
    w.surface = MTN_SURF_ROCK;
    mtn_collider c = collider(&w, 0);
    mtn_player p;
    mtn_player_init(&p, v3(0, 0, 0), 0.0f);

    /* no time, no change */
    mtn_move_input in = input(0, 1);
    p.footstep = 1;
    mtn_player_update(&p, &in, &c, 0.0f);
    CHECK_NEAR(p.pos.z, 0, 0);
    CHECK(p.footstep == 1);
    CHECK(w.ground_calls == 0);
    mtn_player_update(&p, &in, &c, -1.0f);
    CHECK(w.ground_calls == 0);

    /* walking forward accelerates to walk speed, heading -z */
    int steps = 0;
    float last_step_z = 0.0f, min_gap = 1e9f;
    for (int i = 0; i < 240; i++) {
        mtn_player_update(&p, &in, &c, DT);
        if (p.footstep) {
            steps++;
            if (steps > 1) min_gap = mtn_minf(min_gap, last_step_z - p.pos.z);
            last_step_z = p.pos.z;
        }
        CHECK(p.on_ground);
        CHECK_NEAR(p.pos.y, 0.0f, 0);
        CHECK(!p.blocked);
    }
    CHECK_NEAR(v3_len(p.vel), p.walk_speed, 1e-3);
    CHECK_NEAR(p.pos.x, 0.0f, 1e-5);
    float walked = -p.pos.z;
    CHECK(walked > 9.0f && walked < 4.0f * p.walk_speed);
    CHECK(p.surface == MTN_SURF_ROCK);
    /* a footstep every 1.2 m */
    CHECK(steps == (int)(walked / 1.2f));
    CHECK(min_gap > 1.15f && min_gap < 1.25f);
    CHECK(p.bob_phase > 0.0f);

    /* running is faster and has a longer stride */
    in.run = 1;
    float z0 = p.pos.z;
    steps = 0;
    for (int i = 0; i < 240; i++) {
        mtn_player_update(&p, &in, &c, DT);
        steps += p.footstep;
    }
    CHECK_NEAR(v3_len(p.vel), p.run_speed, 1e-3);
    float ran = z0 - p.pos.z;
    CHECK(ran > walked * 1.8f);
    CHECK(steps >= (int)(ran / 1.6f) - 1 && steps <= (int)(ran / 1.6f) + 1);

    /* diagonal input is not faster than straight input */
    mtn_player_init(&p, v3(0, 0, 0), 0.0f);
    in = input(1, 1);
    for (int i = 0; i < 240; i++) mtn_player_update(&p, &in, &c, DT);
    CHECK_NEAR(v3_len(p.vel), p.walk_speed, 1e-3);
    CHECK_NEAR(p.vel.x, -p.vel.z, 1e-3); /* right (+x) and forward (-z) */
    CHECK(p.pos.x > 0.0f);
    /* out of range input is clamped */
    mtn_player_init(&p, v3(0, 0, 0), 0.0f);
    in = input(0, 7);
    for (int i = 0; i < 240; i++) mtn_player_update(&p, &in, &c, DT);
    CHECK_NEAR(v3_len(p.vel), p.walk_speed, 1e-3);

    /* stopping: friction brings the walker to rest, no more footsteps */
    in = input(0, 0);
    for (int i = 0; i < 120; i++) mtn_player_update(&p, &in, &c, DT);
    CHECK(v3_len(p.vel) < 0.01f);
    float zs = p.pos.z;
    float sd = p.step_dist;
    for (int i = 0; i < 10; i++) mtn_player_update(&p, &in, &c, DT);
    CHECK(p.pos.z - zs > -0.01f);
    CHECK(!p.footstep);
    CHECK(p.step_dist >= sd);

    /* looking around: yaw wraps, pitch is clamped */
    mtn_player_init(&p, v3(0, 0, 0), 3.0f);
    in = input(0, 0);
    in.look_x = 0.5f;
    in.look_y = 1.0f;
    mtn_player_update(&p, &in, &c, DT);
    CHECK_NEAR(p.yaw, 3.5f - MTN_TAU, 1e-5);
    CHECK_NEAR(p.pitch, 1.0f, 1e-6);
    mtn_player_update(&p, &in, &c, DT);
    CHECK_NEAR(p.pitch, 1.45f, 1e-6);
    in.look_y = -5.0f;
    mtn_player_update(&p, &in, &c, DT);
    CHECK_NEAR(p.pitch, -1.45f, 1e-6);
    /* walking follows the view direction */
    mtn_player_init(&p, v3(0, 0, 0), MTN_PI * 0.5f);
    in = input(0, 1);
    for (int i = 0; i < 60; i++) mtn_player_update(&p, &in, &c, DT);
    CHECK(p.pos.x > 1.0f);
    CHECK_NEAR(p.pos.z, 0.0f, 1e-4);
}

static void walk_east(mtn_player *p, const mtn_collider *c, int frames, int *blocked) {
    mtn_move_input in = input(0, 1);
    *blocked = 0;
    for (int i = 0; i < frames; i++) {
        mtn_player_update(p, &in, c, DT);
        *blocked += p->blocked;
    }
}

static void test_slopes(void) {
    world w = {0};
    w.edge_x = 1.0f;
    w.feature = 1;
    w.slope = 2.0f; /* 63 degrees */
    mtn_collider c = collider(&w, 0);
    mtn_player p;
    int blocked;

    /* too steep: cannot climb */
    mtn_player_init(&p, v3(0, 0, 0), MTN_PI * 0.5f);
    walk_east(&p, &c, 300, &blocked);
    CHECK(blocked > 100);
    CHECK(p.pos.x < 1.0f + 0.1f);
    CHECK(p.pos.y < 0.2f);
    CHECK(p.on_ground);
    CHECK(p.blocked);
    CHECK_NEAR(p.vel.x, 0.0f, 0); /* stopped on the frame it was blocked */

    /* a gentle slope is walkable */
    w.slope = 0.5f; /* 27 degrees */
    mtn_player_init(&p, v3(0, 0, 0), MTN_PI * 0.5f);
    walk_east(&p, &c, 300, &blocked);
    CHECK(blocked == 0);
    CHECK(p.pos.x > 8.0f);
    CHECK_NEAR(p.pos.y, (p.pos.x - 1.0f) * 0.5f, 1e-4);
    CHECK(p.on_ground);

    /* right at the limit (normal.y just above max_slope) still works */
    w.slope = 1.15f; /* normal.y = 0.656 */
    mtn_player_init(&p, v3(0, 0, 0), MTN_PI * 0.5f);
    walk_east(&p, &c, 200, &blocked);
    CHECK(blocked == 0);
    CHECK(p.pos.y > 2.0f);
    w.slope = 1.25f; /* normal.y = 0.625 */
    mtn_player_init(&p, v3(0, 0, 0), MTN_PI * 0.5f);
    walk_east(&p, &c, 200, &blocked);
    CHECK(blocked > 0);
    CHECK(p.pos.y < 0.2f);

    /* downhill is always allowed and the walker sticks to the ground */
    w.slope = 2.0f;
    mtn_player_init(&p, v3(10, 18, 0), -MTN_PI * 0.5f);
    walk_east(&p, &c, 120, &blocked); /* "east" in the player's frame: heading -x, downhill */
    CHECK(blocked == 0);
    CHECK(p.pos.x < 10.0f - 3.0f);
    CHECK(p.on_ground);
    CHECK_NEAR(p.pos.y, w_ground(&w, p.pos.x, 0, &(vec3){0}, &(int){0}), 1e-4);

    /* a wall-like step (flat on top) cannot be walked up, a small curb can */
    w.feature = 2;
    w.height = 0.5f;
    mtn_player_init(&p, v3(0, 0, 0), MTN_PI * 0.5f);
    walk_east(&p, &c, 200, &blocked);
    CHECK(blocked > 0);
    CHECK(p.pos.x <= 1.0f);
    w.height = 0.02f;
    mtn_player_init(&p, v3(0, 0, 0), MTN_PI * 0.5f);
    walk_east(&p, &c, 200, &blocked);
    CHECK(blocked == 0);
    CHECK(p.pos.x > 3.0f);
    CHECK_NEAR(p.pos.y, 0.02f, 1e-6);

    /* ... but jumping up the step works (the slope limit only applies on the ground) */
    w.height = 0.5f;
    mtn_player_init(&p, v3(0.8f, 0, 0), MTN_PI * 0.5f);
    mtn_move_input in = input(0, 1);
    for (int i = 0; i < 30; i++) mtn_player_update(&p, &in, &c, DT); /* gain speed against the step */
    in.jump = 1;
    for (int i = 0; i < 90; i++) {
        mtn_player_update(&p, &in, &c, DT);
        in.jump = 0;
    }
    CHECK(p.pos.x > 1.2f);
    CHECK_NEAR(p.pos.y, 0.5f, 1e-6);
    CHECK(p.on_ground);
}

static void test_jump_and_fall(void) {
    world w = {0};
    w.base = 2.0f;
    mtn_collider c = collider(&w, 0);
    mtn_player p;
    mtn_player_init(&p, v3(0, 2, 0), 0.0f);
    mtn_move_input in = input(0, 0);
    in.jump = 1;
    mtn_player_update(&p, &in, &c, DT);
    CHECK(!p.on_ground);
    CHECK(p.pos.y > 2.0f);
    CHECK_NEAR(p.vel.y, p.jump_speed - p.gravity * DT, 1e-5);
    float peak = p.pos.y;
    int frames = 1;
    while (!p.on_ground && frames < 600) {
        mtn_player_update(&p, &in, &c, DT); /* holding jump in the air does nothing */
        peak = mtn_maxf(peak, p.pos.y);
        frames++;
    }
    CHECK(p.on_ground);
    CHECK_NEAR(p.pos.y, 2.0f, 0);
    CHECK_NEAR(p.vel.y, 0.0f, 0);
    /* about one metre high, airborne for 2 v / g */
    CHECK_NEAR(peak - 2.0f, p.jump_speed * p.jump_speed / (2.0f * p.gravity), 0.06);
    CHECK_NEAR((float)frames * DT, 2.0f * p.jump_speed / p.gravity, 0.05);

    /* air control is weaker than ground control */
    mtn_player ground, air;
    mtn_player_init(&ground, v3(0, 2, 0), 0.0f);
    mtn_player_init(&air, v3(0, 2, 0), 0.0f);
    in = input(0, 0);
    in.jump = 1;
    mtn_player_update(&air, &in, &c, DT); /* take off standing still */
    CHECK(!air.on_ground);
    in = input(0, 1);
    mtn_player_update(&ground, &in, &c, DT);
    mtn_player_update(&air, &in, &c, DT);
    CHECK(!air.on_ground);
    CHECK(fabsf(air.vel.z) > 0.0f);
    CHECK(fabsf(air.vel.z) < fabsf(ground.vel.z) * 0.5f);
    /* no footsteps while flying */
    float sd = air.step_dist;
    for (int i = 0; i < 20; i++) mtn_player_update(&air, &in, &c, DT);
    CHECK(!air.on_ground);
    CHECK_NEAR(air.step_dist, sd, 0);

    /* falling from a height lands on the ground */
    mtn_player_init(&p, v3(0, 7, 0), 0.0f);
    p.on_ground = 0;
    in = input(0, 0);
    frames = 0;
    while (!p.on_ground && frames < 600) {
        mtn_player_update(&p, &in, &c, DT);
        frames++;
    }
    CHECK(p.on_ground);
    CHECK_NEAR(p.pos.y, 2.0f, 0);
    CHECK_NEAR((float)frames * DT, sqrtf(2.0f * 5.0f / p.gravity), 0.05);

    /* walking off a cliff: airborne, then lands below */
    w.edge_x = 1.0f;
    w.feature = 3;
    w.height = 3.0f;
    mtn_player_init(&p, v3(0, 2, 0), MTN_PI * 0.5f);
    in = input(0, 1);
    int was_airborne = 0;
    for (int i = 0; i < 240; i++) {
        mtn_player_update(&p, &in, &c, DT);
        if (!p.on_ground) was_airborne = 1;
    }
    CHECK(was_airborne);
    CHECK(p.on_ground);
    CHECK_NEAR(p.pos.y, -1.0f, 0);
    CHECK(p.pos.x > 2.0f);
}

static void test_obstacles(void) {
    world w = {0};
    w.surface = MTN_SURF_SNOW;
    w.use_wall = 1;
    w.wall_x = 2.0f;
    mtn_collider c = collider(&w, 1);
    mtn_player p;
    int blocked;

    /* not walkable beyond x = 2 (deep water, end of the world) */
    mtn_player_init(&p, v3(0, 0, 0), MTN_PI * 0.5f);
    walk_east(&p, &c, 240, &blocked);
    CHECK(blocked > 0);
    CHECK(p.pos.x < 2.0f);
    CHECK(p.pos.x > 1.9f);
    CHECK(p.surface == MTN_SURF_SNOW);
    CHECK(w.push_calls == 240);

    /* obstacles push the walker out */
    w.use_wall = 0;
    w.post = v3(0, 0, -3);
    w.post_r = 0.5f;
    mtn_player_init(&p, v3(0, 0, 0), 0.0f);
    mtn_move_input in = input(0, 1);
    blocked = 0;
    for (int i = 0; i < 240; i++) {
        mtn_player_update(&p, &in, &c, DT);
        blocked += p.blocked;
        CHECK(v3_dist_xz(p.pos, w.post) >= 0.85f - 1e-4f);
    }
    CHECK(blocked > 0);
    CHECK(p.pos.z > -3.0f);
    CHECK_NEAR(v3_dist_xz(p.pos, w.post), 0.85f, 1e-3);
    /* sliding past it sideways */
    in = input(1, 1);
    for (int i = 0; i < 240; i++) mtn_player_update(&p, &in, &c, DT);
    CHECK(p.pos.z < -4.0f);
    CHECK(!p.blocked);
}

/* a steep wall rising towards +x (about 72 degrees) with a gentle step in z */
static float slide_ground(void *ctx, float x, float z, vec3 *n, int *surf) {
    (void)ctx;
    *surf = 1;
    float k = *(float *)ctx;
    if (x <= 5.0f) {
        *n = v3(0, 1, 0);
        return z > 20.0f ? k : 0.0f; /* beyond z = 20 an obstacle step of height k */
    }
    *n = v3_norm(v3(-3.0f, 1.0f, 0.0f));
    return (x - 5.0f) * 3.0f;
}

static void test_slide_along_slope(void) {
    float step = 0.0f;
    mtn_collider c = {&step, slide_ground, NULL, NULL};
    mtn_player p;
    /* walking diagonally into the steep slope slides along it (+z) instead of stopping */
    mtn_player_init(&p, v3(4.9f, 0, 0), 0.0f);
    p.yaw = MTN_PI * 0.25f; /* forward = (+x, -z) */
    mtn_move_input in = {0};
    in.move_y = 1.0f;
    for (int i = 0; i < 60; i++) mtn_player_update(&p, &in, &c, 1.0f / 60.0f);
    CHECK(p.blocked);
    CHECK(p.pos.x < 5.3f);       /* did not climb the wall */
    CHECK(p.pos.z < -1.0f);      /* but moved along it */
    CHECK(p.vel.x < 0.5f);
    /* straight into the wall: no sideways component, so it stops */
    mtn_player_init(&p, v3(4.9f, 0, 0), MTN_PI * 0.5f);
    for (int i = 0; i < 30; i++) mtn_player_update(&p, &in, &c, 1.0f / 60.0f);
    CHECK_NEAR(p.pos.z, 0.0f, 1e-3);
    CHECK(p.pos.x < 5.3f);
    /* sliding into something too high on the side as well: stays put */
    step = 10.0f;
    mtn_player_init(&p, v3(4.95f, 0, 19.99f), 0.0f);
    p.yaw = MTN_PI * 0.75f; /* forward = (+x, +z) */
    vec3 start = p.pos;
    mtn_player_update(&p, &in, &c, 1.0f / 60.0f);
    mtn_player_update(&p, &in, &c, 1.0f / 60.0f);
    CHECK(p.blocked);
    CHECK(v3_dist_xz(p.pos, start) < 0.2f);
}

MTN_SUITE(player) {
    TEST_RUN(test_init_and_view);
    TEST_RUN(test_walking);
    TEST_RUN(test_slopes);
    TEST_RUN(test_jump_and_fall);
    TEST_RUN(test_obstacles);
    TEST_RUN(test_slide_along_slope);
}
