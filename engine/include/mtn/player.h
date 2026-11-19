/* Mountain engine - first person walker with slope limit and pluggable collision. MIT License. */
#ifndef MTN_PLAYER_H
#define MTN_PLAYER_H

#include "mtn/math.h"

/* ledges up to this height are stepped up automatically */
#define MTN_STEP_HEIGHT 0.6f
/* higher ledges up to this height are climbed after walking into them for a moment */
#define MTN_CLIMB_HEIGHT 1.2f
#define MTN_CLIMB_DELAY 0.25f

typedef enum { MTN_SURF_GRASS = 0, MTN_SURF_ROCK, MTN_SURF_SNOW, MTN_SURF_WOOD, MTN_SURF_WATER } mtn_surface;

/* Collision world: ground height/normal query and obstacle push-out. */
typedef struct {
    void *ctx;
    /* returns ground height at x,z; writes normal and surface type */
    float (*ground)(void *ctx, float x, float z, vec3 *normal, int *surface);
    /* moves pos out of obstacles; returns 1 on contact */
    int (*push_out)(void *ctx, vec3 *pos, float radius, float height);
    /* returns 0 if the point may not be entered (deep water, outside the world) */
    int (*walkable)(void *ctx, float x, float z);
} mtn_collider;

typedef struct {
    float move_x, move_y;   /* strafe / forward in -1..1 */
    float look_x, look_y;   /* radians this frame */
    int run;
    int jump;
} mtn_move_input;

typedef struct {
    vec3 pos;          /* feet */
    vec3 vel;
    float yaw, pitch;
    int on_ground;
    float eye_height;
    float radius;
    float walk_speed, run_speed, jump_speed, gravity;
    float max_slope;   /* minimum normal.y that can be climbed */
    float step_height; /* ledges up to this height are stepped up (or down) automatically */
    float climb_height;/* ledges up to this height are climbed after pushing into them */
    float climb_timer; /* how long the walker has been pushing into a climbable ledge */
    float step_lag;    /* the eye catches up smoothly after a step: offset in metres */
    float bob_phase;
    float step_dist;
    int surface;
    int footstep;      /* set to 1 on the frame a footstep happens */
    int blocked;       /* last move hit a slope or obstacle */
} mtn_player;

void mtn_player_init(mtn_player *p, vec3 pos, float yaw);
void mtn_player_update(mtn_player *p, const mtn_move_input *in, const mtn_collider *c, float dt);
vec3 mtn_player_eye(const mtn_player *p);
vec3 mtn_player_forward(const mtn_player *p);
mat4 mtn_player_view(const mtn_player *p);
const char *mtn_surface_name(int s);

#endif
