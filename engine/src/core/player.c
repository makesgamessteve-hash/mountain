/* Mountain engine - first person walker with slope limit and pluggable collision. MIT License. */
#include "mtn/player.h"

#include <math.h>
#include <string.h>

void mtn_player_init(mtn_player *p, vec3 pos, float yaw) {
    memset(p, 0, sizeof *p);
    p->pos = pos;
    p->yaw = yaw;
    p->eye_height = 1.7f;
    p->radius = 0.35f;
    p->walk_speed = 2.6f;
    p->run_speed = 5.2f;
    p->jump_speed = 4.4f; /* ~1 m jump */
    p->gravity = 9.81f;
    p->max_slope = 0.64f; /* about 50 degrees */
    p->on_ground = 1;
}

vec3 mtn_player_eye(const mtn_player *p) {
    float bob = p->on_ground ? sinf(p->bob_phase * 2.0f) * 0.035f : 0.0f;
    return v3(p->pos.x, p->pos.y + p->eye_height + bob, p->pos.z);
}

vec3 mtn_player_forward(const mtn_player *p) {
    float cp = cosf(p->pitch);
    return v3(sinf(p->yaw) * cp, sinf(p->pitch), -cosf(p->yaw) * cp);
}

mat4 mtn_player_view(const mtn_player *p) {
    vec3 eye = mtn_player_eye(p);
    return m4_look_at(eye, v3_add(eye, mtn_player_forward(p)), v3(0, 1, 0));
}

const char *mtn_surface_name(int s) {
    static const char *n[] = {"grass", "rock", "snow", "wood", "water"};
    return (s >= 0 && s <= MTN_SURF_WATER) ? n[s] : "grass";
}

void mtn_player_update(mtn_player *p, const mtn_move_input *in, const mtn_collider *c, float dt) {
    if (dt <= 0.0f) return;
    p->footstep = 0;
    p->blocked = 0;
    p->yaw = mtn_wrap_angle(p->yaw + in->look_x);
    p->pitch = mtn_clampf(p->pitch + in->look_y, -1.45f, 1.45f);

    float mx = mtn_clampf(in->move_x, -1, 1), my = mtn_clampf(in->move_y, -1, 1);
    float ml = sqrtf(mx * mx + my * my);
    if (ml > 1.0f) { mx /= ml; my /= ml; ml = 1.0f; }
    vec3 fwd = v3(sinf(p->yaw), 0, -cosf(p->yaw));
    vec3 right = v3(cosf(p->yaw), 0, sinf(p->yaw));
    float speed = in->run ? p->run_speed : p->walk_speed;
    vec3 want = v3_scale(v3_add(v3_scale(fwd, my), v3_scale(right, mx)), speed);
    float accel = p->on_ground ? 12.0f : 2.5f;
    float k = mtn_minf(1.0f, accel * dt);
    p->vel.x += (want.x - p->vel.x) * k;
    p->vel.z += (want.z - p->vel.z) * k;

    if (in->jump && p->on_ground) {
        p->vel.y = p->jump_speed;
        p->on_ground = 0;
    }
    p->vel.y -= p->gravity * dt;

    vec3 old = p->pos;
    vec3 np = v3_add(p->pos, v3_scale(p->vel, dt));
    vec3 n;
    int surf = 0;
    /* slope limit: refuse horizontal moves that climb a too-steep slope */
    float gh_old = c->ground(c->ctx, old.x, old.z, &n, &surf);
    float gh_new = c->ground(c->ctx, np.x, np.z, &n, &surf);
    float horiz = v3_dist_xz(old, np);
    if (horiz > 1e-5f && p->on_ground) {
        float rise = gh_new - gh_old;
        if (rise > 0.0f && (n.y < p->max_slope || rise / horiz > 1.25f)) {
            np.x = old.x;
            np.z = old.z;
            p->vel.x = p->vel.z = 0.0f;
            p->blocked = 1;
        }
    }
    if (c->walkable && !c->walkable(c->ctx, np.x, np.z)) {
        np.x = old.x;
        np.z = old.z;
        p->vel.x = p->vel.z = 0.0f;
        p->blocked = 1;
    }
    if (c->push_out && c->push_out(c->ctx, &np, p->radius, p->eye_height)) p->blocked = 1;
    float gh = c->ground(c->ctx, np.x, np.z, &n, &surf);
    p->surface = surf;
    if (np.y <= gh || (p->on_ground && p->vel.y <= 0.0f && np.y - gh < 0.6f)) {
        np.y = gh; /* land or follow the ground downhill */
        p->vel.y = 0.0f;
        p->on_ground = 1;
    } else {
        p->on_ground = 0;
    }
    p->pos = np;

    float moved = v3_dist_xz(old, np);
    if (p->on_ground && moved > 0.0f) {
        p->bob_phase += moved * 2.2f;
        p->step_dist += moved;
        float stride = in->run ? 1.6f : 1.2f;
        if (p->step_dist >= stride) {
            p->step_dist -= stride;
            p->footstep = 1;
        }
    }
}
