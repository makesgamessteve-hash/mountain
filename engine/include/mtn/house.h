/* Mountain engine - the magic house: site analysis, exterior variants, door,
 * endless interior rooms, portals for the door and the windows. MIT License. */
#ifndef MTN_HOUSE_H
#define MTN_HOUSE_H

#include "mtn/flora.h"
#include "mtn/mesh.h"
#include "mtn/terrain.h"

#define MTN_MAX_ROOM_TYPES 16
#define MTN_MAX_ROOM_ITEMS 40
#define MTN_ROOM_LENGTH 9.0f
#define MTN_DOOR_WIDTH 1.1f
#define MTN_DOOR_HEIGHT 2.1f
#define MTN_HOUSE_UNSUMMON_DIST 25.0f

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
    char reason[64];
} mtn_house_site;

typedef struct { vec3 mn, mx; } mtn_aabb;

typedef struct {
    char prefab[24];
    float x, z, rot, scale;
    int arg;
} mtn_room_item;

typedef struct {
    char name[32];
    float width, height;
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

typedef struct {
    int index;
    int type;
    float z0, length, width, height;
    mat4 window_frame;   /* portal frame in interior space (forward = out through the glass) */
    vec3 window_size;    /* half width, half height */
    int view;            /* -1 real view from the house, otherwise viewpoint index */
    mtn_aabb colliders[96];
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
const char *mtn_house_variant_name(int v);

int mtn_house_find_site(const mtn_terrain *t, const mtn_flora *f, vec3 player, float yaw, mtn_house_site *out);
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

/* interior */
int mtn_house_room_type_for(const mtn_house *h, int index);
int mtn_house_room_at(float z);
void mtn_house_build_room(const mtn_house *h, const mtn_terrain *t, int index, mtn_room *info, mtn_mesh *solid,
                          mtn_mesh *sprites, mtn_mesh *glow);
int mtn_room_collide(const mtn_room *r, vec3 *pos, float radius);
int mtn_room_contains(const mtn_room *r, vec3 p);

#endif
