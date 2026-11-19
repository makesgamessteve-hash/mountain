/* Mountain engine - plants: species data, placement, LOD meshes, ground cover. MIT License. */
#ifndef MTN_FLORA_H
#define MTN_FLORA_H

#include "mtn/mesh.h"
#include "mtn/sky.h"
#include "mtn/terrain.h"

#define MTN_MAX_SPECIES 48
#define MTN_FLORA_CHUNK 96.0f

typedef enum { MTN_PLANT_TREE = 0, MTN_PLANT_BUSH, MTN_PLANT_GROUND, MTN_PLANT_BOULDER } mtn_plant_kind;
typedef enum { MTN_CROWN_CONE = 0, MTN_CROWN_ROUND, MTN_CROWN_OVAL, MTN_CROWN_UMBRELLA } mtn_crown;

typedef struct {
    char name[32];
    int kind;            /* mtn_plant_kind */
    int crown;           /* mtn_crown (trees) */
    int deciduous;
    float height_min, height_max;
    float trunk_radius;  /* metres at the base for a 10 m tree */
    float crown_width;   /* crown radius relative to height */
    int leaf_tiles[4];
    int leaf_tile_count;
    int bark_tile;
    vec3 bark_color;
    vec3 leaf_color[MTN_SEASON_COUNT];
    int blossom_tile;    /* -1 none */
    int bloom_mask;      /* seasons with flowers (ground plants) as bit mask */
    float alt_min, alt_max;
    float slope_max;
    float moist_min, moist_max;
    int biome_mask;      /* bit per mtn_biome */
    float density;       /* chance per placement cell in ideal habitat */
    float patchiness;    /* 0 uniform .. 1 strongly clumped */
} mtn_plant_species;

typedef struct {
    vec3 pos;
    float scale;   /* height in metres for trees, size for others */
    float rot;
    uint16_t species;
    uint8_t hidden;
    uint8_t variant;
} mtn_plant;

typedef struct {
    vec3 pos;
    float size;
    uint8_t species;
    uint8_t variant;
} mtn_ground_item;

typedef struct {
    mtn_plant_species species[MTN_MAX_SPECIES];
    int species_count;
    mtn_plant *plants;
    int count, cap;
    int chunks;          /* chunks per side */
    int *chunk_start;    /* index into plants (sorted by chunk) */
    int *chunk_count;
    int *chunk_version;  /* bumped when plants in a chunk change visibility */
    uint32_t seed;
} mtn_flora;

typedef struct {
    vec3 leaf_color[MTN_MAX_SPECIES];
    float leaf_amount[MTN_MAX_SPECIES]; /* 0 bare .. 1 full; bloom for ground plants */
    float blossom[MTN_MAX_SPECIES];
} mtn_flora_season;

void mtn_flora_init(mtn_flora *f);
void mtn_flora_free(mtn_flora *f);
void mtn_flora_clear_species(mtn_flora *f);
void mtn_species_defaults(mtn_plant_species *s);
int mtn_flora_add_species(mtn_flora *f, const mtn_plant_species *s);
int mtn_flora_find_species(const mtn_flora *f, const char *name);
float mtn_flora_habitat(const mtn_plant_species *s, const mtn_terrain *t, float x, float z);
void mtn_flora_populate(mtn_flora *f, const mtn_terrain *t, uint32_t seed);
int mtn_flora_chunk_of(const mtn_flora *f, float x, float z);

/* Seasonal colours/amounts for every species. */
void mtn_flora_season_eval(const mtn_flora *f, mtn_season_blend s, float snow, mtn_flora_season *out);

/* Ground cover (grass, flowers, ferns...) for an 8 m cell, deterministic. */
int mtn_flora_ground_cell(const mtn_flora *f, const mtn_terrain *t, int cx, int cz, mtn_ground_item *out, int max);

/* Mesh building. lod 0 = full detail, 1 = medium, 2 = far billboard. */
void mtn_tree_build(const mtn_flora *f, const mtn_plant *p, int lod, mtn_mesh *solid, mtn_mesh *leaves);
void mtn_flora_chunk_build(const mtn_flora *f, int chunk, int lod, mtn_mesh *solid, mtn_mesh *leaves);
void mtn_ground_build(const mtn_flora *f, const mtn_ground_item *items, int n, mtn_mesh *leaves);

/* Interaction */
int mtn_flora_hide_radius(mtn_flora *f, vec3 c, float r, int keep);
void mtn_flora_unhide_all(mtn_flora *f);
int mtn_flora_nearest_tree(const mtn_flora *f, vec3 p, float max_dist, float min_height);
/* pushes a circle (player) out of trunks and boulders; returns 1 on contact */
int mtn_flora_collide(const mtn_flora *f, vec3 *pos, float radius);

#endif
