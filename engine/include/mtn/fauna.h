/* Mountain engine - calm wildlife: species data, spawning, behaviour, meshes. MIT License. */
#ifndef MTN_FAUNA_H
#define MTN_FAUNA_H

#include "mtn/mesh.h"
#include "mtn/rng.h"
#include "mtn/sky.h"
#include "mtn/terrain.h"

#define MTN_MAX_ANIMAL_SPECIES 24
#define MTN_MAX_ANIMALS 96

typedef enum {
    MTN_ANIMAL_QUADRUPED = 0, /* deer, ibex, fox, rabbit, marmot */
    MTN_ANIMAL_BIRD,          /* songbird flocks */
    MTN_ANIMAL_SOARING,       /* eagle circling high */
    MTN_ANIMAL_BUTTERFLY,
    MTN_ANIMAL_FIREFLY
} mtn_animal_kind;

typedef enum { MTN_AS_IDLE = 0, MTN_AS_GRAZE, MTN_AS_WALK, MTN_AS_FLEE, MTN_AS_FLY } mtn_animal_state;

typedef struct {
    char name[32];
    int kind;
    float size;             /* body length in metres */
    float leg_len, neck_len, head_size;
    vec3 color, color2;
    int horns;              /* 0 none, 1 antlers, 2 curved horns, 3 long ears */
    float tail;
    float walk_speed, run_speed;
    float comfort_dist;
    float alt_min, alt_max;
    int biome_mask;
    float hour_start, hour_end; /* active hours, may wrap past midnight */
    int season_mask;
    int group_min, group_max;
    int max_groups;
    float spawn_chance;     /* per spawn attempt */
    int sprite_tile;        /* butterflies/fireflies */
    char sound[24];         /* optional call sound */
} mtn_animal_species;

typedef struct {
    int species;
    int alive;
    int state;
    vec3 pos, vel;
    float yaw;
    float timer;
    float anim;
    float fade;     /* 0..1 fade in/out */
    int leaving;
    int group;
    vec3 home;      /* flock / wander centre */
    float phase;
} mtn_animal;

typedef struct {
    mtn_animal_species species[MTN_MAX_ANIMAL_SPECIES];
    int species_count;
    mtn_animal animals[MTN_MAX_ANIMALS];
    mtn_rng rng;
    float spawn_timer;
    int next_group;
    char call_events[4][24];
    vec3 call_pos[4];
    vec3 last_call_pos;     /* where the last popped call came from */
    int call_count;
} mtn_fauna;

void mtn_fauna_init(mtn_fauna *f, uint32_t seed);
void mtn_animal_defaults(mtn_animal_species *s);
int mtn_fauna_add_species(mtn_fauna *f, const mtn_animal_species *s);
void mtn_fauna_clear(mtn_fauna *f);
int mtn_fauna_active(const mtn_animal_species *s, float hour, mtn_season_blend season);
int mtn_fauna_count(const mtn_fauna *f, int species);
int mtn_fauna_spawn_group(mtn_fauna *f, const mtn_terrain *t, int species, vec3 at);
void mtn_fauna_update(mtn_fauna *f, const mtn_terrain *t, vec3 player, float hour, mtn_season_blend season,
                      float precipitation, float dt);
/* Build dynamic meshes for all visible animals near the camera. */
void mtn_fauna_build(const mtn_fauna *f, vec3 cam, float max_dist, mtn_mesh *solid, mtn_mesh *sprites);
/* Pops a species call sound name ("" if none); its position is then in last_call_pos. */
const char *mtn_fauna_pop_call(mtn_fauna *f);

#endif
