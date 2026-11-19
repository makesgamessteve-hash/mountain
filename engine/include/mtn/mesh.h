/* Mountain engine - CPU mesh building (3D primitives, billboards, voxels). MIT License. */
#ifndef MTN_MESH_H
#define MTN_MESH_H

#include "mtn/math.h"
#include <stddef.h>
#include <stdint.h>

/* Vertex flags */
enum {
    MTN_VF_EMISSIVE = 1,   /* ignores lighting (glowing windows, lanterns) */
    MTN_VF_WIND = 2,       /* sways with the wind */
    MTN_VF_SNOW = 4,       /* upward faces receive snow cover */
    MTN_VF_LEAF = 8,       /* tinted with the species' seasonal leaf colour (extra = species) */
    MTN_VF_BLOSSOM = 16,   /* scaled by the species' blossom amount */
    MTN_VF_BILLBOARD = 32, /* camera facing sprite: normal holds the corner offset */
    MTN_VF_UPRIGHT = 64,   /* billboard rotates around the vertical axis only */
    MTN_VF_ABS_UV = 128    /* uv is already inside the tile (no wrapping) */
};

typedef struct {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
    uint8_t r, g, b, a;
    uint8_t tile, flags, extra, ao;
} mtn_vertex;

typedef struct {
    mtn_vertex *v;
    int vcount, vcap;
    uint32_t *idx;
    int icount, icap;
} mtn_mesh;

typedef struct {
    uint8_t r, g, b, a;
    uint8_t tile, flags, extra;
    float uv_scale; /* texture tiles per metre for box/cylinder builders */
} mtn_material;

#define MTN_ATLAS_COLS 8

void mtn_mesh_init(mtn_mesh *m);
void mtn_mesh_free(mtn_mesh *m);
void mtn_mesh_clear(mtn_mesh *m);
int mtn_mesh_vertex(mtn_mesh *m, const mtn_vertex *v);
void mtn_mesh_tri(mtn_mesh *m, int a, int b, int c);
void mtn_mesh_append(mtn_mesh *dst, const mtn_mesh *src, mat4 xf);
void mtn_mesh_bounds(const mtn_mesh *m, vec3 *mn, vec3 *mx);

mtn_material mtn_mat(uint32_t rgba, int tile, int flags, float uv_scale);
uint32_t mtn_rgb(int r, int g, int b);
uint32_t mtn_rgb_scale(uint32_t rgba, float s);

/* quad p0..p3 counter-clockwise when seen from the front */
void mtn_mesh_quad(mtn_mesh *m, mat4 xf, vec3 p0, vec3 p1, vec3 p2, vec3 p3, const mtn_material *mat, vec2 uv0,
                   vec2 uv1);
/* faces: bit 0 -x, 1 +x, 2 -y, 3 +y, 4 -z, 5 +z */
void mtn_mesh_box(mtn_mesh *m, mat4 xf, vec3 mn, vec3 mx, const mtn_material *mat, int faces);
void mtn_mesh_cylinder(mtn_mesh *m, mat4 xf, vec3 a, vec3 b, float ra, float rb, int sides,
                       const mtn_material *mat, int caps);
/* anchor 0 = centred, 1 = standing on its bottom edge */
void mtn_mesh_billboard(mtn_mesh *m, vec3 c, float w, float h, float anchor, const mtn_material *mat, float wind);
void mtn_mesh_rock(mtn_mesh *m, mat4 xf, float radius, uint32_t seed, const mtn_material *mat);

/* ---- voxels ---- */
typedef struct {
    int sx, sy, sz;
    float size;
    uint8_t *v;
    uint32_t pal[256];
} mtn_voxels;

void mtn_voxels_init(mtn_voxels *vx, int sx, int sy, int sz, float size);
void mtn_voxels_free(mtn_voxels *vx);
void mtn_voxels_set(mtn_voxels *vx, int x, int y, int z, uint8_t c);
uint8_t mtn_voxels_get(const mtn_voxels *vx, int x, int y, int z);
void mtn_voxels_box(mtn_voxels *vx, int x0, int y0, int z0, int x1, int y1, int z1, uint8_t c);
void mtn_voxels_blob(mtn_voxels *vx, float cx, float cy, float cz, float r, uint8_t c, uint32_t seed);
int mtn_voxels_count(const mtn_voxels *vx);
/* Emits only visible faces; adds ambient occlusion and per-voxel colour jitter. */
void mtn_voxels_mesh(const mtn_voxels *vx, mtn_mesh *m, mat4 xf, int tile, int flags);

#endif
