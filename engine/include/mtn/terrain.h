/* Mountain engine - procedural mountain terrain. MIT License. */
#ifndef MTN_TERRAIN_H
#define MTN_TERRAIN_H

#include "mtn/math.h"
#include <stdint.h>

#define MTN_MAX_VIEWPOINTS 24
#define MTN_NO_WATER (-1.0e9f)
#define MTN_TRAIL_MAX_GRADE 0.30f /* rise per metre along the trail */
#define MTN_TRAIL_WIDTH 2.4f
#define MTN_TRAIL_WALK_HALF 1.4f /* the walkable path is a little wider than it looks */

typedef enum {
    MTN_BIOME_WATER = 0,
    MTN_BIOME_FOREST,
    MTN_BIOME_MEADOW,
    MTN_BIOME_ALPINE,
    MTN_BIOME_SCREE,
    MTN_BIOME_ROCK,
    MTN_BIOME_SNOW,
    MTN_BIOME_COUNT
} mtn_biome;

typedef struct {
    uint32_t seed;
    int size;              /* grid points per side, e.g. 257 or 513 */
    float cell;            /* metres per grid cell */
    float peak_height;     /* main peak height above the valley floor */
    float range_height;    /* surrounding ranges */
    float hill_height;     /* low rolling hills */
    float ridge_strength;  /* 0..1 sharp ridges */
    int erosion_drops;     /* hydraulic erosion particles */
    float lake_min_depth;  /* metres */
    int lake_min_cells;
    float sea_level;       /* MTN_NO_WATER = no sea (reserved for beaches) */
    float playable_radius; /* fraction of the half size that is walkable */
    float tree_line;       /* altitude fraction where forest ends */
    float snow_line;       /* altitude fraction of the summer snow line */
} mtn_terrain_params;

typedef struct {
    vec3 pos;        /* eye position */
    float yaw;       /* view direction: (sin(yaw), 0, -cos(yaw)) */
    float score;
} mtn_viewpoint;

typedef struct {
    mtn_terrain_params p;
    int n;
    float cell;
    float world_size;
    float *h;       /* heights n*n, row major (z * n + x) */
    float *water;   /* water surface height or MTN_NO_WATER */
    float *moist;   /* 0..1 */
    uint8_t *biome; /* mtn_biome per grid point */
    float *trail_dist; /* distance to trail in metres (capped) */
    int *trail_cell_start, *trail_cell_items; /* trail segments near each grid cell */
    float min_h, max_h;
    vec3 center;
    vec3 summit;
    vec3 trailhead;
    vec3 *trail;
    int trail_len;
    vec3 *cairns;
    int cairn_count;
    vec3 *posts;          /* painted trail marker posts */
    int post_count;
    vec3 signpost;        /* signpost at the trailhead */
    float signpost_yaw;
    mtn_viewpoint vps[MTN_MAX_VIEWPOINTS];
    int vp_count;
    int lake_count;
} mtn_terrain;

void mtn_terrain_default_params(mtn_terrain_params *p);
int mtn_terrain_generate(mtn_terrain *t, const mtn_terrain_params *p);
void mtn_terrain_free(mtn_terrain *t);

float mtn_terrain_height(const mtn_terrain *t, float x, float z);
vec3 mtn_terrain_normal(const mtn_terrain *t, float x, float z);
float mtn_terrain_slope(const mtn_terrain *t, float x, float z); /* 0 flat .. 1 vertical */
float mtn_terrain_water(const mtn_terrain *t, float x, float z); /* surface or MTN_NO_WATER */
float mtn_terrain_altitude_frac(const mtn_terrain *t, float h);
mtn_biome mtn_terrain_biome(const mtn_terrain *t, float x, float z);
float mtn_terrain_moisture(const mtn_terrain *t, float x, float z);
float mtn_terrain_trail_distance(const mtn_terrain *t, float x, float z);
/* Closest point on the trail centre line (y = path surface); 0 when far from the trail. */
int mtn_terrain_trail_nearest(const mtn_terrain *t, float x, float z, vec3 *closest);
/* Like mtn_terrain_trail_nearest, but only stretches of path whose surface is within
 * tol of ref_y count (the stretch the walker is on, not the one above or below). */
int mtn_terrain_trail_nearest_level(const mtn_terrain *t, float x, float z, float ref_y, float tol, vec3 *closest);
/* Height of the level path surface when (x, z) is on the path. Where two stretches
 * overlap (the tip of a switchback) the one nearest to ref_y (the walker's feet) wins. */
int mtn_terrain_trail_surface(const mtn_terrain *t, float x, float z, float ref_y, float *y);
int mtn_terrain_in_playable(const mtn_terrain *t, float x, float z);
/* Clamp a position into the playable disc; returns 1 when it was moved. */
int mtn_terrain_clamp_playable(const mtn_terrain *t, vec3 *p);
int mtn_terrain_raycast(const mtn_terrain *t, vec3 o, vec3 d, float maxdist, vec3 *hit);
float mtn_terrain_grid(const mtn_terrain *t, int x, int z);
const char *mtn_biome_name(mtn_biome b);

/* Building blocks exposed for tests. */
void mtn_terrain_erode(float *h, int n, float cell, int drops, uint32_t seed);
int mtn_terrain_fill_lakes(mtn_terrain *t);
int mtn_terrain_find_trail(mtn_terrain *t);
int mtn_terrain_find_viewpoints(mtn_terrain *t);
float mtn_terrain_openness(const mtn_terrain *t, vec3 eye, float yaw, float range);
/* FNV-1a over heights (1 cm), water, biomes, trail, cairns and viewpoints */
uint32_t mtn_terrain_checksum(const mtn_terrain *t);

#endif
