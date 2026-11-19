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
    p->step_height = MTN_STEP_HEIGHT;
    p->climb_height = MTN_CLIMB_HEIGHT;
    p->on_ground = 1;
}

vec3 mtn_player_eye(const mtn_player *p) {
    float bob = p->on_ground ? sinf(p->bob_phase * 2.0f) * 0.035f : 0.0f;
    return v3(p->pos.x, p->pos.y + p->eye_height + bob - p->step_lag, p->pos.z);
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
    if (horiz <= 1e-5f || !p->on_ground) p->climb_timer = 0.0f;
    if (horiz > 1e-5f && p->on_ground) {
        float rise = gh_new - gh_old;
        /* a ledge (the edge of the path, a porch, a stone) up to the step height is
         * stepped up; anything else that climbs must be a gentle enough slope */
        int ledge = rise > p->step_height && rise <= p->climb_height && n.y >= p->max_slope;
        if (ledge) p->climb_timer += dt; /* pushing into a ledge: lift off after a moment */
        int step = n.y >= p->max_slope && (rise <= p->step_height || (ledge && p->climb_timer >= MTN_CLIMB_DELAY));
        if (!ledge) p->climb_timer = 0.0f;
        if (rise > 0.0f && !step && (n.y < p->max_slope || rise / horiz > 1.25f)) {
            /* too steep: drop the uphill part of the move and slide along the slope */
            p->blocked = 1;
            vec3 up = v3_norm(v3(-n.x, 0, -n.z));
            vec3 m = v3(np.x - old.x, 0, np.z - old.z);
            float into = v3_dot(m, up);
            vec3 along = into > 0.0f ? v3_sub(m, v3_scale(up, into)) : m;
            vec3 sp = v3(old.x + along.x, np.y, old.z + along.z);
            vec3 n2;
            float gh_side = c->ground(c->ctx, sp.x, sp.z, &n2, &surf);
            float side = v3_len(along);
            float rs = gh_side - gh_old;
            if (side > 1e-4f && (rs <= 0.0f || (n2.y >= p->max_slope && (rs <= p->step_height || rs / side <= 1.25f)))) {
                np = sp;
                float vin = v3_dot(v3(p->vel.x, 0, p->vel.z), up);
                if (vin > 0.0f) {
                    p->vel.x -= up.x * vin;
                    p->vel.z -= up.z * vin;
                }
            } else {
                np.x = old.x;
                np.z = old.z;
                p->vel.x = p->vel.z = 0.0f;
            }
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
    int was_on_ground = p->on_ground;
    if (np.y <= gh || (p->on_ground && p->vel.y <= 0.0f && np.y - gh < p->step_height + 0.05f)) {
        if (gh - np.y > p->step_height) p->climb_timer = 0.0f; /* climbed up */
        np.y = gh; /* land, step up or follow the ground downhill */
        p->vel.y = 0.0f;
        p->on_ground = 1;
    } else {
        p->on_ground = 0;
    }
    /* a sudden step while walking: the eye follows smoothly instead of jumping */
    float moved_xz = v3_dist_xz(old, np);
    float dy = np.y - old.y;
    if (was_on_ground && p->on_ground && fabsf(dy) > 0.04f + 1.25f * moved_xz) p->step_lag += dy;
    p->step_lag = mtn_clampf(p->step_lag, -p->climb_height, p->climb_height) * expf(-(p->step_lag > 0.5f ? 7.0f : 12.0f) * dt);
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
