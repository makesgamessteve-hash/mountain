/* Mountain engine - the magic house: site analysis, exterior variants, door,
 * endless interior rooms, portals for the door and the windows. MIT License. */
#ifndef MTN_HOUSE_H
#define MTN_HOUSE_H

#include "mtn/flora.h"
#include "mtn/mesh.h"
#include "mtn/terrain.h"

#define MTN_MAX_ROOM_TYPES 24
#define MTN_MAX_ROOM_ITEMS 48
#define MTN_ROOM_CELL 18.0f      /* the rooms sit on a grid of 18 m cells */
#define MTN_ROOM_MAX_SIZE 13.0f  /* so corridors are at least 5 m long */
#define MTN_ROOM_MIN_SIZE 4.0f
#define MTN_MAX_ROOMS 13         /* the current room and every room within two steps */
#define MTN_CORRIDOR_HALF 0.8f
#define MTN_CORRIDOR_HEIGHT 2.5f
#define MTN_BALCONY_DEPTH 2.0f
#define MTN_BALCONY_LIFT 8.0f    /* balconies hang this high above their viewpoint */
#define MTN_ROOM_MAX_COLLIDERS 128
#define MTN_ROOM_MAX_WALK 8
#define MTN_DOOR_WIDTH 1.1f
#define MTN_DOOR_HEIGHT 2.1f
#define MTN_HOUSE_UNSUMMON_DIST 25.0f
#define MTN_STAIR_GRADE 0.7f      /* rise per metre of run */
#define MTN_STAIR_MAX_RUN 16.0f
#define MTN_STAIR_LAST_STEP 0.25f /* the last step is at most this high above dry ground */

typedef enum { MTN_HOUSE_NONE = 0, MTN_HOUSE_APPEARING, MTN_HOUSE_PRESENT, MTN_HOUSE_VANISHING } mtn_house_state;
typedef enum { MTN_HV_COTTAGE = 0, MTN_HV_HILLSIDE, MTN_HV_TREE, MTN_HV_STILTS, MTN_HV_COUNT } mtn_house_variant;

typedef struct {
    int ok;
    vec3 pos;        /* floor centre (x,z), y = floor height */
    vec3 front;      /* unit vector the door faces */
    int variant;
    float ground_min, ground_max;
    int tree;        /* plant index for the tree house */
    float water_level;
    int stair_side;  /* 0 front, 1 back, 2 right, 3 left */
    float stair_len; /* horizontal run of the stairs, 0 = none */
    char reason[64];
} mtn_house_site;

typedef struct { vec3 mn, mx; } mtn_aabb;

/* grid directions in interior space */
enum { MTN_SIDE_EAST = 0, MTN_SIDE_WEST, MTN_SIDE_NORTH, MTN_SIDE_SOUTH };
/* room walls in room-local space: +x is always the window (or balcony, or front door) side */
enum { MTN_WALL_NONE = -1, MTN_WALL_WINDOW = 0, MTN_WALL_BACK, MTN_WALL_RIGHT, MTN_WALL_LEFT };

typedef struct {
    char prefab[24];
    float x, z;        /* metres from the room centre (+x towards the window) */
    float fx, fz;      /* plus fractions of the half width / half length */
    int wall;          /* MTN_WALL_*: stand against this wall, facing into the room */
    float along, at;   /* along the wall: fraction of its half length plus metres */
    float inset;       /* distance of the item's origin from the wall */
    float rot, scale;  /* rot: degrees (added to the wall's facing) */
    int arg;
} mtn_room_item;

typedef struct {
    char name[32];
    float width, height;             /* width: towards the window; height of the ceiling */
    float width_min, width_max;      /* each room of this type gets a size in these ranges */
    float length_min, length_max;    /* length: along the window wall */
    float balcony;                   /* chance that a room of this type has a balcony */
    int wall_tile, floor_tile;
    uint32_t wall_color, floor_color;
    vec3 light_color;
    float weight;
    int is_cabin;
    int item_count;
    mtn_room_item items[MTN_MAX_ROOM_ITEMS];
} mtn_room_type;

typedef struct { vec3 pos; vec3 color; float radius; float flicker; } mtn_light;
typedef struct { vec3 center, right, up; int photo; } mtn_photo_slot;

/* The plan of one grid cell: cheap to compute, the same for the same house and cell. */
typedef struct {
    int ci, cj;
    int type;
    int closed;          /* MTN_SIDE_* of the window / balcony side (the cabin: the front door side) */
    int balcony;
    float width, length, height;
    vec3 center;         /* centre of the floor in interior space */
    mat4 xf;             /* room-local -> interior space */
} mtn_room_plan;

typedef struct {
    int built;
    int ci, cj;
    int type;
    int closed, balcony;
    float width, length, height;
    vec3 center;
    mat4 xf;
    mat4 window_frame;   /* portal frame in interior space (forward = out through the glass) */
    vec3 window_size;    /* half width, half height */
    int view;            /* -1 real view from the house, otherwise viewpoint index */
    vec3 balcony_quads[5][4]; /* the box of view around a balcony, facing inwards */
    mtn_aabb walk[MTN_ROOM_MAX_WALK]; /* floor areas: the room, its corridors, the balcony */
    int walk_count;
    mtn_aabb colliders[MTN_ROOM_MAX_COLLIDERS];
    int collider_count;
    mtn_light lights[8];
    int light_count;
    mtn_photo_slot photos[8];
    int photo_count;
    int has_fire;
    vec3 fire_pos;
} mtn_room;

typedef struct {
    int state;
    float anim;          /* 0..1 appear/vanish progress */
    int variant;
    uint32_t seed;
    vec3 origin;         /* floor centre in world space */
    vec3 front, right;
    float floor_y, ground_min, water_level;
    int tree;
    int stair_side;      /* side of the porch/platform the stairs leave from */
    float stair_len;     /* horizontal run; the last step is on reachable ground */
    mat4 frame, inv_frame;
    float width, depth, wall_h;
    float door_angle;    /* radians, 0 closed */
    int door_open;
    int version;
    int inside;
    int room;
    vec3 chimney_top;
    int type_count;
    mtn_room_type types[MTN_MAX_ROOM_TYPES];
} mtn_house;

void mtn_house_init(mtn_house *h);
void mtn_room_type_defaults(mtn_room_type *t);
int mtn_house_add_room_type(mtn_house *h, const mtn_room_type *t);
int mtn_room_add_item(mtn_room_type *t, const char *prefab, float x, float z, float rot, float scale, int arg);
int mtn_room_add_item_ex(mtn_room_type *t, const mtn_room_item *it);
void mtn_room_item_defaults(mtn_room_item *it);
const char *mtn_house_variant_name(int v);

int mtn_house_find_site(const mtn_terrain *t, const mtn_flora *f, vec3 player, float yaw, mtn_house_site *out);
/* Fit the stairs of a site to the terrain: the run ends where the steps meet dry ground. */
int mtn_house_fit_stairs(const mtn_terrain *t, mtn_house_site *s);
/* World position just below the last step (where the stairs reach the ground). */
vec3 mtn_house_stair_foot(const mtn_house *h);
void mtn_house_summon(mtn_house *h, const mtn_house_site *s, uint32_t seed);
void mtn_house_unsummon(mtn_house *h);
void mtn_house_update(mtn_house *h, float dt);
int mtn_house_exists(const mtn_house *h);
int mtn_house_can_unsummon(const mtn_house *h, vec3 player);
void mtn_house_toggle_door(mtn_house *h);
float mtn_house_door_distance(const mtn_house *h, vec3 p);
vec3 mtn_house_door_world(const mtn_house *h);

/* walkable surfaces (porch, stairs, platform) on top of the terrain */
int mtn_house_surface(const mtn_house *h, float x, float z, float *height);
int mtn_house_collide(const mtn_house *h, vec3 *pos, float radius);
/* 1 when the move prev->cur passes through the open door into the house */
int mtn_house_crossed_in(const mtn_house *h, vec3 prev, vec3 cur);

/* portals */
mat4 mtn_house_door_frame_ext(const mtn_house *h);
mat4 mtn_house_door_frame_int(void);
mat4 mtn_house_ext_to_int(const mtn_house *h);
mat4 mtn_house_window_to_world(const mtn_house *h, const mtn_terrain *t, const mtn_room *r);

/* meshes (world space for the exterior, interior space for rooms) */
void mtn_house_build_exterior(const mtn_house *h, const mtn_terrain *t, const mtn_flora *f, mtn_mesh *solid,
                              mtn_mesh *sprites, mtn_mesh *glow);
void mtn_house_build_door(mtn_mesh *m);
mat4 mtn_house_door_model(const mtn_house *h, int interior);

/* interior: an endless grid of rooms */
int mtn_house_room_type_at(const mtn_house *h, int ci, int cj);
int mtn_house_closed_side(const mtn_house *h, int ci, int cj);
void mtn_house_room_plan(const mtn_house *h, int ci, int cj, mtn_room_plan *p);
vec3 mtn_house_cell_center(const mtn_house *h, int ci, int cj);
void mtn_house_cell_at(const mtn_house *h, vec3 p, int *ci, int *cj);
const char *mtn_side_name(int side);
void mtn_house_build_room(const mtn_house *h, const mtn_terrain *t, int ci, int cj, mtn_room *info, mtn_mesh *solid,
                          mtn_mesh *sprites, mtn_mesh *glow);
int mtn_room_contains(const mtn_room *r, vec3 p);
int mtn_room_collide(const mtn_room *r, vec3 *pos, float radius);

#endif
