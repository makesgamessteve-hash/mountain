/* Mountain engine - the endless interior of the magic house: a grid of rooms joined by
 * corridors, room shells with doorways, big windows or balconies, furniture prefabs,
 * lights and colliders. Rooms live in their own "interior space"; the cabin's front
 * door is at the origin, facing -z. MIT License. */
#include "mtn/house.h"
#include "mtn/rng.h"

#include <math.h>
#include <string.h>

#define NO_TEXTURE 255
#define WHITE 0xFFFFFFFFu
#define DOOR_HALF (MTN_DOOR_WIDTH * 0.5f)
#define DOOR_TOP (MTN_DOOR_HEIGHT + 0.1f)

/* ------------------------------------------------------------------ grid */

static const int SIDE_DI[4] = {1, -1, 0, 0}, SIDE_DJ[4] = {0, 0, 1, -1};

const char *mtn_side_name(int side) {
    static const char *n[4] = {"east", "west", "north", "south"};
    return side >= 0 && side < 4 ? n[side] : "none";
}

static int floor_div(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

static uint32_t cell_hash(uint32_t seed, int a, int b, uint32_t salt) {
    return mtn_hash_u32(seed ^ mtn_hash_u32((uint32_t)a * 73856093u ^ (uint32_t)b * 19349663u ^ salt * 83492791u));
}

static float hash01(uint32_t h) { return (float)(h & 0xFFFFu) / 65535.0f; }

/* Every room has one closed side, shared with the neighbour behind it like a domino:
 * the plane is tiled with 2x2 blocks of two dominoes each (side by side or one above
 * the other). The cabin's block puts the cabin's closed side south, at the front door. */
int mtn_house_closed_side(const mtn_house *h, int ci, int cj) {
    int bi = floor_div(ci, 2), bj = floor_div(cj + 1, 2);
    int vertical = (bi == 0 && bj == 0) ? 1 : (int)(cell_hash(h->seed, bi, bj, 0x51u) & 1u);
    if (vertical) return cj == 2 * bj - 1 ? MTN_SIDE_NORTH : MTN_SIDE_SOUTH;
    return ci == 2 * bi ? MTN_SIDE_EAST : MTN_SIDE_WEST;
}

static int cabin_type(const mtn_house *h) {
    for (int i = 0; i < h->type_count; i++)
        if (h->types[i].is_cabin) return i;
    return 0;
}

/* Room types: a per-house order of the types along each row; consecutive rows are
 * shifted by 1..k-1 so that neighbours in both directions always differ. */
int mtn_house_room_type_at(const mtn_house *h, int ci, int cj) {
    if (h->type_count == 0) return -1;
    int cabin = cabin_type(h);
    if (ci == 0 && cj == 0) return cabin;
    int types[MTN_MAX_ROOM_TYPES], k = 0;
    for (int i = 0; i < h->type_count; i++)
        if (!h->types[i].is_cabin) types[k++] = i;
    if (k == 0) return cabin;
    if (k == 1) return types[0];
    mtn_rng r;
    mtn_rng_seed(&r, h->seed ^ 0xBEEFu, 1);
    for (int i = k - 1; i > 0; i--) {
        int j = mtn_rng_int(&r, 0, i);
        int t = types[i];
        types[i] = types[j];
        types[j] = t;
    }
    int off = 0;
    if (cj > 0)
        for (int j = 1; j <= cj; j++) off += 1 + (int)(cell_hash(h->seed, j, 0, 0x77u) % (uint32_t)(k - 1));
    else
        for (int j = 0; j > cj; j--) off -= 1 + (int)(cell_hash(h->seed, j, 0, 0x77u) % (uint32_t)(k - 1));
    int slot = ((ci + off) % k + k) % k;
    return types[slot];
}

static float grid_z0(const mtn_house *h) {
    if (h->type_count == 0) return 4.0f;
    const mtn_room_type *c = &h->types[cabin_type(h)];
    return mtn_clampf(c->width, MTN_ROOM_MIN_SIZE, MTN_ROOM_MAX_SIZE) * 0.5f;
}

vec3 mtn_house_cell_center(const mtn_house *h, int ci, int cj) {
    return v3((float)ci * MTN_ROOM_CELL, 0, (float)cj * MTN_ROOM_CELL + grid_z0(h));
}

void mtn_house_cell_at(const mtn_house *h, vec3 p, int *ci, int *cj) {
    *ci = (int)lroundf(p.x / MTN_ROOM_CELL);
    *cj = (int)lroundf((p.z - grid_z0(h)) / MTN_ROOM_CELL);
}

static vec3 side_dir(int side) { return v3((float)SIDE_DI[side], 0, (float)SIDE_DJ[side]); }

void mtn_house_room_plan(const mtn_house *h, int ci, int cj, mtn_room_plan *p) {
    memset(p, 0, sizeof *p);
    p->ci = ci;
    p->cj = cj;
    p->type = mtn_house_room_type_at(h, ci, cj);
    p->closed = mtn_house_closed_side(h, ci, cj);
    const mtn_room_type *rt = p->type >= 0 ? &h->types[p->type] : NULL;
    uint32_t hs = cell_hash(h->seed, ci, cj, 0x5Au);
    if (rt) {
        float wmin = rt->width_min > 0 ? rt->width_min : rt->width, wmax = rt->width_max > 0 ? rt->width_max : rt->width;
        float lmin = rt->length_min > 0 ? rt->length_min : 9.0f, lmax = rt->length_max > 0 ? rt->length_max : lmin;
        p->width = mtn_lerpf(wmin, mtn_maxf(wmin, wmax), hash01(hs));
        p->length = mtn_lerpf(lmin, mtn_maxf(lmin, lmax), hash01(hs >> 16));
        p->height = rt->height;
        if (rt->is_cabin) p->width = rt->width;
        p->balcony = !rt->is_cabin && rt->balcony > 0.0f && hash01(cell_hash(h->seed, ci, cj, 0xBA1Cu)) < rt->balcony;
    } else {
        p->width = 8.0f;
        p->length = 9.0f;
        p->height = 3.6f;
    }
    p->width = mtn_clampf(p->width, MTN_ROOM_MIN_SIZE, MTN_ROOM_MAX_SIZE);
    p->length = mtn_clampf(p->length, MTN_ROOM_MIN_SIZE, MTN_ROOM_MAX_SIZE);
    p->center = mtn_house_cell_center(h, ci, cj);
    vec3 x = side_dir(p->closed), y = v3(0, 1, 0);
    p->xf = m4_from_basis(p->center, x, y, v3_cross(x, y));
}

/* half extent of a planned room along interior x (axis 0) or z (axis 1) */
static float plan_half(const mtn_room_plan *p, int axis) {
    int closed_on_x = p->closed == MTN_SIDE_EAST || p->closed == MTN_SIDE_WEST;
    return 0.5f * ((axis == 0) == closed_on_x ? p->width : p->length);
}

/* ------------------------------------------------------------------ builders */

typedef struct {
    mtn_mesh *solid, *sprites, *glow;
    mtn_room *info;
    mat4 xf;
} pctx;

static void p_box(pctx *c, vec3 mn, vec3 mx, uint32_t color, int tile, float uvs) {
    mtn_material m = mtn_mat(color, tile, 0, uvs);
    mtn_mesh_box(c->solid, c->xf, mn, mx, &m, 63);
}

static void p_glow_box(pctx *c, vec3 mn, vec3 mx, uint32_t color, int tile) {
    mtn_material m = mtn_mat(color, tile, MTN_VF_EMISSIVE, 2.0f);
    mtn_mesh_box(c->glow, c->xf, mn, mx, &m, 63);
}

static void p_cyl(pctx *c, vec3 a, vec3 b, float ra, float rb, int sides, uint32_t color, int tile) {
    mtn_material m = mtn_mat(color, tile, 0, 2.0f);
    mtn_mesh_cylinder(c->solid, c->xf, a, b, ra, rb, sides, &m, 1);
}

static void p_collide(pctx *c, vec3 mn, vec3 mx) {
    if (c->info->collider_count >= 96) return;
    vec3 lo = v3(1e30f, 1e30f, 1e30f), hi = v3(-1e30f, -1e30f, -1e30f);
    for (int i = 0; i < 8; i++) {
        vec3 p = v3(i & 1 ? mx.x : mn.x, i & 2 ? mx.y : mn.y, i & 4 ? mx.z : mn.z);
        p = m4_mul_point(c->xf, p);
        lo = v3(mtn_minf(lo.x, p.x), mtn_minf(lo.y, p.y), mtn_minf(lo.z, p.z));
        hi = v3(mtn_maxf(hi.x, p.x), mtn_maxf(hi.y, p.y), mtn_maxf(hi.z, p.z));
    }
    c->info->colliders[c->info->collider_count].mn = lo;
    c->info->colliders[c->info->collider_count].mx = hi;
    c->info->collider_count++;
}

static void p_light(pctx *c, vec3 p, vec3 color, float radius, float flicker) {
    if (c->info->light_count >= 8) return;
    mtn_light *l = &c->info->lights[c->info->light_count++];
    l->pos = m4_mul_point(c->xf, p);
    l->color = color;
    l->radius = radius;
    l->flicker = flicker;
}

static void p_leaves(pctx *c, vec3 p, float size, int tile, uint32_t color, int count, uint32_t seed) {
    mtn_rng r;
    mtn_rng_seed(&r, seed, 3);
    for (int i = 0; i < count; i++) {
        vec3 q = v3_add(p, v3(mtn_rng_range(&r, -size, size) * 0.4f, mtn_rng_range(&r, 0, size * 0.6f),
                              mtn_rng_range(&r, -size, size) * 0.4f));
        mtn_material m = mtn_mat(mtn_rgb_scale(color, mtn_rng_range(&r, 0.8f, 1.1f)), tile, 0, 1.0f);
        mtn_mesh_billboard(c->sprites, m4_mul_point(c->xf, q), size, size, 0.0f, &m, 0.0f);
    }
}

/* Furniture prefabs: origin on the floor, front facing +z. */
static void prefab(pctx *c, const char *name, int arg, uint32_t seed) {
    uint32_t wood = mtn_rgb(150, 104, 66), dark = mtn_rgb(96, 66, 44), light = mtn_rgb(196, 160, 116);
    vec3 warm = v3(1.0f, 0.75f, 0.45f);
    if (strcmp(name, "bed") == 0) { /* huge cozy bed: 2.4 x 2.7 */
        p_box(c, v3(-1.2f, 0, -1.35f), v3(1.2f, 0.45f, 1.35f), wood, NO_TEXTURE, 1);
        p_box(c, v3(-1.15f, 0.45f, -1.3f), v3(1.15f, 0.7f, 1.3f), mtn_rgb(236, 230, 214), NO_TEXTURE, 1);
        p_box(c, v3(-1.18f, 0.7f, -0.6f), v3(1.18f, 0.82f, 1.32f), WHITE, 8, 1.2f);
        p_box(c, v3(-1.0f, 0.7f, -1.25f), v3(-0.1f, 0.92f, -0.8f), mtn_rgb(245, 242, 235), NO_TEXTURE, 1);
        p_box(c, v3(0.1f, 0.7f, -1.25f), v3(1.0f, 0.92f, -0.8f), mtn_rgb(245, 242, 235), NO_TEXTURE, 1);
        p_box(c, v3(-1.25f, 0, -1.45f), v3(1.25f, 1.5f, -1.32f), WHITE, 2, 1.5f);
        float px[2] = {-1.22f, 1.22f};
        for (int i = 0; i < 2; i++) {
            p_cyl(c, v3(px[i], 0, -1.38f), v3(px[i], 1.75f, -1.38f), 0.08f, 0.07f, 6, dark, NO_TEXTURE);
            p_cyl(c, v3(px[i], 0, 1.3f), v3(px[i], 0.85f, 1.3f), 0.07f, 0.06f, 6, dark, NO_TEXTURE);
        }
        p_collide(c, v3(-1.25f, 0, -1.45f), v3(1.25f, 0.9f, 1.35f));
    } else if (strcmp(name, "kitchen") == 0) { /* counter, stove, shelves, pots */
        p_box(c, v3(-1.6f, 0, -0.35f), v3(0.6f, 0.9f, 0.35f), wood, 2, 1.5f);
        p_box(c, v3(-1.65f, 0.9f, -0.38f), v3(0.65f, 0.96f, 0.38f), light, NO_TEXTURE, 1);
        p_box(c, v3(0.7f, 0, -0.35f), v3(1.5f, 0.85f, 0.35f), WHITE, 19, 2.0f);
        p_cyl(c, v3(1.1f, 0.85f, -0.2f), v3(1.1f, 3.4f, -0.2f), 0.09f, 0.09f, 6, mtn_rgb(40, 38, 36), NO_TEXTURE);
        p_glow_box(c, v3(0.85f, 0.25f, 0.351f), v3(1.35f, 0.5f, 0.36f), mtn_rgb(255, 150, 60), NO_TEXTURE);
        p_box(c, v3(-1.6f, 0.96f, -0.36f), v3(0.6f, 1.9f, -0.3f), WHITE, 11, 2.5f);
        for (int s = 0; s < 2; s++) {
            float y = 1.5f + 0.45f * (float)s;
            p_box(c, v3(-1.6f, y, -0.36f), v3(0.6f, y + 0.04f, -0.08f), dark, NO_TEXTURE, 1);
            for (int j = 0; j < 5; j++) {
                float x = -1.45f + 0.42f * (float)j;
                uint32_t jc = mtn_rgb(120 + 25 * j, 90 + 30 * s, 60 + 20 * ((j + s) % 3));
                p_cyl(c, v3(x, y + 0.04f, -0.22f), v3(x, y + 0.24f, -0.22f), 0.07f, 0.06f, 6, jc, NO_TEXTURE);
            }
        }
        p_cyl(c, v3(0.95f, 0.85f, 0.05f), v3(0.95f, 1.05f, 0.05f), 0.15f, 0.15f, 8, WHITE, 24);
        p_cyl(c, v3(-0.9f, 0.96f, 0.1f), v3(-0.9f, 1.1f, 0.1f), 0.12f, 0.1f, 8, WHITE, 24);
        p_collide(c, v3(-1.65f, 0, -0.4f), v3(1.5f, 1.0f, 0.38f));
    } else if (strcmp(name, "desk") == 0) { /* writer's desk with journal, candle and chair */
        p_box(c, v3(-0.8f, 0.72f, -0.4f), v3(0.8f, 0.78f, 0.4f), wood, NO_TEXTURE, 1);
        float lx[2] = {-0.75f, 0.7f}, lz[2] = {-0.35f, 0.3f};
        for (int i = 0; i < 2; i++)
            for (int k = 0; k < 2; k++) p_box(c, v3(lx[i], 0, lz[k]), v3(lx[i] + 0.05f, 0.72f, lz[k] + 0.05f), dark, NO_TEXTURE, 1);
        p_box(c, v3(-0.75f, 0.78f, -0.4f), v3(0.75f, 1.1f, -0.3f), dark, NO_TEXTURE, 1);
        mtn_material paper = mtn_mat(WHITE, 15, MTN_VF_ABS_UV, 1.0f);
        mtn_mesh_quad(c->solid, c->xf, v3(-0.35f, 0.785f, 0.15f), v3(0.15f, 0.785f, 0.15f), v3(0.15f, 0.785f, -0.2f),
                      v3(-0.35f, 0.785f, -0.2f), &paper, v2(0, 0), v2(1, 1));
        p_cyl(c, v3(0.45f, 0.78f, -0.1f), v3(0.45f, 0.84f, -0.1f), 0.06f, 0.05f, 6, mtn_rgb(40, 40, 60), NO_TEXTURE);
        p_cyl(c, v3(-0.6f, 0.78f, -0.15f), v3(-0.6f, 0.95f, -0.15f), 0.03f, 0.03f, 6, mtn_rgb(240, 230, 200), NO_TEXTURE);
        p_glow_box(c, v3(-0.62f, 0.95f, -0.17f), v3(-0.58f, 1.02f, -0.13f), mtn_rgb(255, 210, 120), NO_TEXTURE);
        p_light(c, v3(-0.6f, 1.1f, -0.1f), v3(1.0f, 0.7f, 0.4f), 3.5f, 0.3f);
        /* chair */
        p_box(c, v3(-0.25f, 0.42f, 0.55f), v3(0.25f, 0.47f, 1.0f), wood, NO_TEXTURE, 1);
        p_box(c, v3(-0.25f, 0.47f, 0.95f), v3(0.25f, 1.05f, 1.0f), wood, NO_TEXTURE, 1);
        p_box(c, v3(-0.22f, 0, 0.58f), v3(-0.18f, 0.42f, 0.62f), dark, NO_TEXTURE, 1);
        p_box(c, v3(0.18f, 0, 0.58f), v3(0.22f, 0.42f, 0.62f), dark, NO_TEXTURE, 1);
        p_collide(c, v3(-0.8f, 0, -0.4f), v3(0.8f, 0.8f, 0.4f));
    } else if (strcmp(name, "bookshelf") == 0) {
        float w = 1.0f;
        p_box(c, v3(-w, 0, -0.2f), v3(w, 2.4f, -0.17f), dark, NO_TEXTURE, 1);
        p_box(c, v3(-w - 0.05f, 0, -0.2f), v3(-w, 2.45f, 0.2f), wood, NO_TEXTURE, 1);
        p_box(c, v3(w, 0, -0.2f), v3(w + 0.05f, 2.45f, 0.2f), wood, NO_TEXTURE, 1);
        p_box(c, v3(-w - 0.05f, 2.4f, -0.2f), v3(w + 0.05f, 2.48f, 0.22f), wood, NO_TEXTURE, 1);
        mtn_material books = mtn_mat(WHITE, 10, 0, 1.6f);
        for (int s = 0; s < 4; s++) {
            float y = 0.05f + 0.6f * (float)s;
            p_box(c, v3(-w, y - 0.04f, -0.2f), v3(w, y, 0.2f), wood, NO_TEXTURE, 1);
            mtn_mesh_quad(c->solid, c->xf, v3(-w, y, 0.15f), v3(w, y, 0.15f), v3(w, y + 0.52f, 0.15f), v3(-w, y + 0.52f, 0.15f), &books,
                          v2(0, 0), v2(w * 2.0f * 1.6f / 1.6f, 1));
        }
        p_collide(c, v3(-w - 0.05f, 0, -0.2f), v3(w + 0.05f, 2.5f, 0.22f));
    } else if (strcmp(name, "fireplace") == 0) {
        p_box(c, v3(-1.0f, 0, -0.5f), v3(-0.55f, 1.3f, 0.3f), WHITE, 12, 2.0f);
        p_box(c, v3(0.55f, 0, -0.5f), v3(1.0f, 1.3f, 0.3f), WHITE, 12, 2.0f);
        p_box(c, v3(-1.0f, 0.9f, -0.5f), v3(1.0f, 1.3f, 0.3f), WHITE, 12, 2.0f);
        p_box(c, v3(-1.1f, 1.3f, -0.5f), v3(1.1f, 1.4f, 0.4f), dark, NO_TEXTURE, 1);
        p_box(c, v3(-0.75f, 1.4f, -0.5f), v3(0.75f, 4.0f, 0.1f), WHITE, 12, 2.0f);
        p_box(c, v3(-0.55f, 0, -0.5f), v3(0.55f, 0.9f, -0.45f), mtn_rgb(30, 24, 20), NO_TEXTURE, 1);
        p_box(c, v3(-0.55f, 0, -0.45f), v3(0.55f, 0.05f, 0.3f), mtn_rgb(60, 55, 50), NO_TEXTURE, 1);
        p_cyl(c, v3(-0.35f, 0.1f, -0.1f), v3(0.35f, 0.12f, -0.15f), 0.07f, 0.07f, 6, WHITE, 0);
        p_cyl(c, v3(-0.3f, 0.12f, 0.05f), v3(0.3f, 0.2f, -0.2f), 0.06f, 0.06f, 6, WHITE, 0);
        /* flames: glowing crossed quads */
        mtn_material fl = mtn_mat(mtn_rgb(255, 170, 60), 45, MTN_VF_EMISSIVE | MTN_VF_ABS_UV, 1.0f);
        mtn_mesh_quad(c->glow, c->xf, v3(-0.35f, 0.1f, -0.1f), v3(0.35f, 0.1f, -0.1f), v3(0.3f, 0.75f, -0.1f), v3(-0.3f, 0.75f, -0.1f), &fl,
                      v2(0, 0), v2(1, 1));
        mtn_mesh_quad(c->glow, c->xf, v3(0, 0.1f, -0.35f), v3(0, 0.1f, 0.15f), v3(0, 0.65f, 0.1f), v3(0, 0.65f, -0.3f), &fl, v2(0, 0),
                      v2(1, 1));
        mtn_mesh_quad(c->glow, c->xf, v3(0, 0.1f, 0.15f), v3(0, 0.1f, -0.35f), v3(0, 0.65f, -0.3f), v3(0, 0.65f, 0.1f), &fl, v2(0, 0),
                      v2(1, 1));
        p_light(c, v3(0, 0.6f, 0.6f), v3(1.0f, 0.55f, 0.25f), 7.0f, 1.0f);
        c->info->has_fire = 1;
        c->info->fire_pos = m4_mul_point(c->xf, v3(0, 0.5f, 0));
        p_collide(c, v3(-1.1f, 0, -0.5f), v3(1.1f, 1.4f, 0.4f));
    } else if (strcmp(name, "armchair") == 0) {
        uint32_t fab = WHITE;
        p_box(c, v3(-0.45f, 0.2f, -0.4f), v3(0.45f, 0.5f, 0.4f), fab, 17, 2);
        p_box(c, v3(-0.45f, 0.5f, -0.45f), v3(0.45f, 1.15f, -0.25f), fab, 17, 2);
        p_box(c, v3(-0.55f, 0.2f, -0.45f), v3(-0.42f, 0.75f, 0.4f), fab, 17, 2);
        p_box(c, v3(0.42f, 0.2f, -0.45f), v3(0.55f, 0.75f, 0.4f), fab, 17, 2);
        p_box(c, v3(-0.5f, 0, -0.4f), v3(0.5f, 0.2f, 0.35f), dark, NO_TEXTURE, 1);
        p_collide(c, v3(-0.55f, 0, -0.45f), v3(0.55f, 1.0f, 0.4f));
    } else if (strcmp(name, "rug") == 0) {
        float s = arg > 0 ? (float)arg * 0.5f : 1.5f;
        p_box(c, v3(-s, 0.0f, -s * 0.7f), v3(s, 0.02f, s * 0.7f), WHITE, 9, 0.6f);
    } else if (strcmp(name, "plant") == 0) {
        p_cyl(c, v3(0, 0, 0), v3(0, 0.45f, 0), 0.18f, 0.24f, 8, WHITE, 21);
        p_leaves(c, v3(0, 0.6f, 0), 0.6f, 8 + (int)(seed % 2u), mtn_rgb(70, 130, 60), 5, seed);
        p_collide(c, v3(-0.25f, 0, -0.25f), v3(0.25f, 0.8f, 0.25f));
    } else if (strcmp(name, "lamp") == 0) {
        p_cyl(c, v3(0, 0, 0), v3(0, 0.05f, 0), 0.2f, 0.2f, 8, dark, NO_TEXTURE);
        p_cyl(c, v3(0, 0.05f, 0), v3(0, 1.5f, 0), 0.025f, 0.025f, 6, mtn_rgb(60, 50, 40), NO_TEXTURE);
        mtn_material shade = mtn_mat(mtn_rgb(255, 214, 150), NO_TEXTURE, MTN_VF_EMISSIVE, 1.0f);
        mtn_mesh_cylinder(c->glow, c->xf, v3(0, 1.4f, 0), v3(0, 1.75f, 0), 0.3f, 0.18f, 8, &shade, 1);
        p_light(c, v3(0, 1.6f, 0), warm, 6.0f, 0.0f);
    } else if (strcmp(name, "table") == 0) {
        p_box(c, v3(-0.8f, 0.72f, -0.5f), v3(0.8f, 0.78f, 0.5f), wood, 2, 1.5f);
        float lx[2] = {-0.72f, 0.67f}, lz[2] = {-0.45f, 0.4f};
        for (int i = 0; i < 2; i++)
            for (int k = 0; k < 2; k++) p_box(c, v3(lx[i], 0, lz[k]), v3(lx[i] + 0.06f, 0.72f, lz[k] + 0.06f), dark, NO_TEXTURE, 1);
        p_cyl(c, v3(0, 0.78f, 0), v3(0, 0.95f, 0), 0.12f, 0.08f, 8, WHITE, 25);
        p_collide(c, v3(-0.8f, 0, -0.5f), v3(0.8f, 0.8f, 0.5f));
    } else if (strcmp(name, "chair") == 0) {
        p_box(c, v3(-0.25f, 0.42f, -0.22f), v3(0.25f, 0.47f, 0.22f), wood, NO_TEXTURE, 1);
        p_box(c, v3(-0.25f, 0.47f, -0.22f), v3(0.25f, 1.0f, -0.18f), wood, NO_TEXTURE, 1);
        p_box(c, v3(-0.2f, 0, -0.18f), v3(0.2f, 0.42f, 0.18f), dark, NO_TEXTURE, 1);
        p_collide(c, v3(-0.25f, 0, -0.22f), v3(0.25f, 0.5f, 0.22f));
    } else if (strcmp(name, "tea_table") == 0) {
        p_cyl(c, v3(0, 0.3f, 0), v3(0, 0.36f, 0), 0.6f, 0.6f, 10, wood, NO_TEXTURE);
        p_cyl(c, v3(0, 0, 0), v3(0, 0.3f, 0), 0.08f, 0.08f, 6, dark, NO_TEXTURE);
        p_cyl(c, v3(0.1f, 0.36f, 0.05f), v3(0.1f, 0.55f, 0.05f), 0.11f, 0.08f, 8, WHITE, 25);
        for (int i = 0; i < 3; i++) {
            float a = (float)i * 2.1f;
            p_cyl(c, v3(cosf(a) * 0.35f, 0.36f, sinf(a) * 0.35f), v3(cosf(a) * 0.35f, 0.43f, sinf(a) * 0.35f), 0.05f, 0.045f, 6, WHITE, 25);
        }
        float cx[4] = {-1, 1, 0, 0}, cz[4] = {0, 0, -1, 1};
        for (int i = 0; i < 4; i++)
            p_box(c, v3(cx[i] * 0.95f - 0.3f, 0, cz[i] * 0.95f - 0.3f), v3(cx[i] * 0.95f + 0.3f, 0.18f, cz[i] * 0.95f + 0.3f), WHITE, 17, 2);
        p_collide(c, v3(-0.6f, 0, -0.6f), v3(0.6f, 0.4f, 0.6f));
    } else if (strcmp(name, "bathtub") == 0) {
        p_box(c, v3(-0.9f, 0.1f, -0.45f), v3(0.9f, 0.6f, 0.45f), mtn_rgb(240, 240, 236), NO_TEXTURE, 1);
        p_box(c, v3(-0.8f, 0.45f, -0.35f), v3(0.8f, 0.58f, 0.35f), WHITE, 28, 1.5f);
        float fx[2] = {-0.8f, 0.8f}, fz[2] = {-0.35f, 0.35f};
        for (int i = 0; i < 2; i++)
            for (int k = 0; k < 2; k++) p_cyl(c, v3(fx[i], 0, fz[k]), v3(fx[i], 0.12f, fz[k]), 0.06f, 0.05f, 6, WHITE, 24);
        p_cyl(c, v3(-0.85f, 0.6f, 0), v3(-0.85f, 0.95f, 0), 0.03f, 0.03f, 6, WHITE, 24);
        p_collide(c, v3(-0.9f, 0, -0.45f), v3(0.9f, 0.6f, 0.45f));
    } else if (strcmp(name, "piano") == 0) {
        p_box(c, v3(-0.75f, 0, -0.3f), v3(0.75f, 1.25f, 0.0f), mtn_rgb(30, 22, 20), NO_TEXTURE, 1);
        p_box(c, v3(-0.75f, 0.7f, 0.0f), v3(0.75f, 0.78f, 0.25f), mtn_rgb(30, 22, 20), NO_TEXTURE, 1);
        p_box(c, v3(-0.7f, 0.78f, 0.02f), v3(0.7f, 0.8f, 0.2f), mtn_rgb(240, 238, 228), NO_TEXTURE, 1);
        p_box(c, v3(-0.35f, 0.42f, 0.55f), v3(0.35f, 0.48f, 0.85f), mtn_rgb(40, 30, 26), NO_TEXTURE, 1);
        p_collide(c, v3(-0.75f, 0, -0.3f), v3(0.75f, 1.3f, 0.3f));
    } else if (strcmp(name, "telescope") == 0) {
        for (int i = 0; i < 3; i++) {
            float a = (float)i * MTN_TAU / 3.0f;
            p_cyl(c, v3(cosf(a) * 0.4f, 0, sinf(a) * 0.4f), v3(0, 1.1f, 0), 0.025f, 0.02f, 4, dark, NO_TEXTURE);
        }
        p_cyl(c, v3(-0.5f, 0.95f, 0), v3(0.7f, 1.45f, 0), 0.07f, 0.11f, 8, WHITE, 24);
        p_collide(c, v3(-0.45f, 0, -0.45f), v3(0.45f, 1.3f, 0.45f));
    } else if (strcmp(name, "planter") == 0) {
        p_box(c, v3(-1.2f, 0, -0.4f), v3(1.2f, 0.6f, 0.4f), wood, 2, 1.5f);
        p_box(c, v3(-1.15f, 0.6f, -0.35f), v3(1.15f, 0.62f, 0.35f), mtn_rgb(70, 50, 36), NO_TEXTURE, 1);
        for (int i = 0; i < 5; i++) {
            int tile = 16 + (int)((seed + (uint32_t)i) % 8u);
            mtn_material f = mtn_mat(WHITE, tile, 0, 1.0f);
            mtn_mesh_billboard(c->sprites, m4_mul_point(c->xf, v3(-0.9f + 0.45f * (float)i, 0.62f, 0)), 0.55f, 0.55f, 1.0f, &f, 0.0f);
        }
        p_leaves(c, v3(0, 0.75f, 0), 0.5f, 14, mtn_rgb(90, 150, 70), 4, seed + 1u);
        p_collide(c, v3(-1.2f, 0, -0.4f), v3(1.2f, 0.7f, 0.4f));
    } else if (strcmp(name, "chest") == 0) {
        p_box(c, v3(-0.5f, 0, -0.3f), v3(0.5f, 0.55f, 0.3f), WHITE, 14, 1.5f);
        p_box(c, v3(-0.52f, 0.5f, -0.32f), v3(0.52f, 0.58f, 0.32f), dark, NO_TEXTURE, 1);
        p_collide(c, v3(-0.5f, 0, -0.3f), v3(0.5f, 0.6f, 0.3f));
    } else if (strcmp(name, "candles") == 0) {
        for (int i = 0; i < 3; i++) {
            float x = -0.12f + 0.12f * (float)i, hgt = 0.15f + 0.08f * (float)(i % 2);
            p_cyl(c, v3(x, 0.75f, 0), v3(x, 0.75f + hgt, 0), 0.03f, 0.03f, 6, mtn_rgb(245, 238, 210), NO_TEXTURE);
            p_glow_box(c, v3(x - 0.015f, 0.75f + hgt, -0.015f), v3(x + 0.015f, 0.8f + hgt, 0.015f), mtn_rgb(255, 210, 120), NO_TEXTURE);
        }
        p_box(c, v3(-0.3f, 0, -0.25f), v3(0.3f, 0.75f, 0.25f), wood, NO_TEXTURE, 1);
        p_light(c, v3(0, 1.0f, 0), v3(1.0f, 0.72f, 0.42f), 4.0f, 0.4f);
        p_collide(c, v3(-0.3f, 0, -0.25f), v3(0.3f, 0.8f, 0.25f));
    } else if (strcmp(name, "photo") == 0) {
        /* picture frame on the wall behind (local -z); arg = how recent the photo is */
        mtn_room *ri = c->info;
        if (ri->photo_count < 8) {
            mtn_photo_slot *s = &ri->photos[ri->photo_count++];
            s->center = m4_mul_point(c->xf, v3(0, 1.6f, -0.02f));
            s->right = v3_norm(m4_mul_dir(c->xf, v3(1, 0, 0)));
            s->up = v3(0, 1, 0);
            s->photo = arg;
        }
        mtn_material fr = mtn_mat(WHITE, 20, MTN_VF_ABS_UV, 1.0f);
        mtn_mesh_quad(c->solid, c->xf, v3(-0.75f, 1.12f, 0.0f), v3(0.75f, 1.12f, 0.0f), v3(0.75f, 2.08f, 0.0f), v3(-0.75f, 2.08f, 0.0f),
                      &fr, v2(0, 0), v2(1, 1));
    } else if (strcmp(name, "star_chart") == 0) {
        mtn_material sc = mtn_mat(WHITE, 26, MTN_VF_ABS_UV, 1.0f);
        mtn_mesh_quad(c->solid, c->xf, v3(-0.8f, 1.0f, 0.0f), v3(0.8f, 1.0f, 0.0f), v3(0.8f, 2.2f, 0.0f), v3(-0.8f, 2.2f, 0.0f), &sc,
                      v2(0, 0), v2(1, 1));
    } else if (strcmp(name, "window_seat") == 0) {
        p_box(c, v3(-1.6f, 0, -0.35f), v3(1.6f, 0.45f, 0.35f), wood, 2, 1.5f);
        p_box(c, v3(-1.55f, 0.45f, -0.3f), v3(1.55f, 0.58f, 0.3f), WHITE, 17, 2);
        for (int i = 0; i < 3; i++) p_box(c, v3(-1.2f + 1.0f * (float)i, 0.58f, -0.3f), v3(-0.8f + 1.0f * (float)i, 0.85f, -0.15f), WHITE, 8, 2);
        p_collide(c, v3(-1.6f, 0, -0.35f), v3(1.6f, 0.6f, 0.35f));
    } else if (strcmp(name, "barrel") == 0) {
        p_cyl(c, v3(0, 0, 0), v3(0, 0.9f, 0), 0.32f, 0.32f, 10, WHITE, 2);
        p_collide(c, v3(-0.32f, 0, -0.32f), v3(0.32f, 0.9f, 0.32f));
    } else if (strcmp(name, "basket") == 0) {
        p_cyl(c, v3(0, 0, 0), v3(0, 0.35f, 0), 0.25f, 0.3f, 8, WHITE, 30);
    } else if (strcmp(name, "sauna_stove") == 0) { /* stone stove with hot rocks */
        uint32_t iron = mtn_rgb(52, 50, 48);
        p_box(c, v3(-0.45f, 0, -0.4f), v3(0.45f, 0.75f, 0.4f), iron, NO_TEXTURE, 1);
        p_box(c, v3(-0.5f, 0.75f, -0.45f), v3(0.5f, 0.85f, 0.45f), iron, NO_TEXTURE, 1);
        for (int i = 0; i < 9; i++) {
            float x = -0.3f + 0.3f * (float)(i % 3), z = -0.3f + 0.3f * (float)(i / 3);
            uint32_t g = mtn_rgb(110 + (int)((seed + (uint32_t)i * 7u) % 40u), 104, 98);
            p_box(c, v3(x - 0.12f, 0.85f, z - 0.12f), v3(x + 0.12f, 1.0f + 0.04f * (float)(i % 2), z + 0.12f), g, 5, 4.0f);
        }
        p_glow_box(c, v3(-0.25f, 0.15f, 0.401f), v3(0.25f, 0.45f, 0.41f), mtn_rgb(255, 120, 40), NO_TEXTURE);
        p_cyl(c, v3(0.3f, 0.85f, -0.3f), v3(0.3f, 4.0f, -0.3f), 0.08f, 0.08f, 6, iron, NO_TEXTURE);
        p_light(c, v3(0, 0.6f, 0.7f), v3(1.0f, 0.5f, 0.25f), 5.0f, 0.8f);
        p_collide(c, v3(-0.5f, 0, -0.45f), v3(0.5f, 1.1f, 0.45f));
    } else if (strcmp(name, "sauna_bench") == 0) { /* two tiers of slatted benches, 3 m long */
        for (int tier = 0; tier < 2; tier++) {
            float y = 0.45f + 0.45f * (float)tier, z0 = -0.6f + 0.55f * (float)tier;
            for (int sl = 0; sl < 4; sl++) {
                float z = z0 - 0.25f + 0.13f * (float)sl;
                p_box(c, v3(-1.5f, y - 0.04f, z), v3(1.5f, y, z + 0.1f), light, NO_TEXTURE, 1);
            }
            p_box(c, v3(-1.5f, 0, z0 - 0.25f), v3(1.5f, y - 0.04f, z0 - 0.22f), wood, 2, 1.5f);
        }
        p_box(c, v3(-1.5f, 0.9f, -0.95f), v3(1.5f, 1.8f, -0.9f), light, 3, 1.5f);
        p_collide(c, v3(-1.5f, 0, -0.95f), v3(1.5f, 0.95f, 0.3f));
    } else if (strcmp(name, "bucket") == 0) { /* wooden bucket and ladle */
        p_cyl(c, v3(0, 0, 0), v3(0, 0.3f, 0), 0.17f, 0.2f, 8, wood, NO_TEXTURE);
        p_cyl(c, v3(0, 0.25f, 0), v3(0.12f, 0.6f, 0.05f), 0.015f, 0.015f, 4, dark, NO_TEXTURE);
    } else if (strcmp(name, "long_table") == 0) { /* dining table, 3.4 x 1.1, with benches or chairs */
        p_box(c, v3(-1.7f, 0.72f, -0.55f), v3(1.7f, 0.8f, 0.55f), wood, 2, 1.0f);
        float lx[2] = {-1.55f, 1.45f}, lz[2] = {-0.45f, 0.35f};
        for (int i = 0; i < 2; i++)
            for (int k = 0; k < 2; k++) p_box(c, v3(lx[i], 0, lz[k]), v3(lx[i] + 0.1f, 0.72f, lz[k] + 0.1f), dark, NO_TEXTURE, 1);
        for (int i = 0; i < 3; i++) {
            float x = -1.1f + 1.1f * (float)i;
            p_cyl(c, v3(x, 0.8f, 0), v3(x, 0.83f, 0), 0.16f, 0.16f, 8, WHITE, 25);
            if (i == 1) {
                p_cyl(c, v3(x, 0.8f, 0.2f), v3(x, 1.0f, 0.2f), 0.03f, 0.03f, 6, mtn_rgb(240, 230, 200), NO_TEXTURE);
                p_glow_box(c, v3(x - 0.02f, 1.0f, 0.18f), v3(x + 0.02f, 1.07f, 0.22f), mtn_rgb(255, 210, 120), NO_TEXTURE);
            }
        }
        for (int sd = -1; sd <= 1; sd += 2) {
            float z = 0.85f * (float)sd;
            p_box(c, v3(-1.6f, 0.42f, z - 0.17f), v3(1.6f, 0.47f, z + 0.17f), wood, NO_TEXTURE, 1);
            p_box(c, v3(-1.5f, 0, z - 0.04f), v3(-1.42f, 0.42f, z + 0.04f), dark, NO_TEXTURE, 1);
            p_box(c, v3(1.42f, 0, z - 0.04f), v3(1.5f, 0.42f, z + 0.04f), dark, NO_TEXTURE, 1);
        }
        p_light(c, v3(0, 1.4f, 0), v3(1.0f, 0.75f, 0.45f), 4.5f, 0.3f);
        p_collide(c, v3(-1.7f, 0, -1.05f), v3(1.7f, 0.85f, 1.05f));
    } else if (strcmp(name, "sofa") == 0) {
        uint32_t fab = mtn_rgb(150 + (int)(seed % 50u), 90, 70 + (int)(seed % 40u));
        p_box(c, v3(-1.1f, 0.15f, -0.45f), v3(1.1f, 0.45f, 0.45f), fab, 17, 2.0f);
        p_box(c, v3(-1.1f, 0.45f, -0.45f), v3(1.1f, 0.95f, -0.25f), fab, 17, 2.0f);
        p_box(c, v3(-1.25f, 0.15f, -0.45f), v3(-1.1f, 0.7f, 0.45f), fab, 17, 2.0f);
        p_box(c, v3(1.1f, 0.15f, -0.45f), v3(1.25f, 0.7f, 0.45f), fab, 17, 2.0f);
        p_box(c, v3(-1.2f, 0, -0.4f), v3(1.2f, 0.15f, 0.4f), dark, NO_TEXTURE, 1);
        p_box(c, v3(-0.9f, 0.45f, -0.25f), v3(-0.3f, 0.75f, -0.1f), WHITE, 8, 2.0f);
        p_collide(c, v3(-1.25f, 0, -0.45f), v3(1.25f, 0.95f, 0.45f));
    } else if (strcmp(name, "wardrobe") == 0) {
        p_box(c, v3(-0.9f, 0, -0.3f), v3(0.9f, 2.2f, 0.3f), wood, 2, 1.2f);
        p_box(c, v3(-0.02f, 0.1f, 0.3f), v3(0.02f, 2.1f, 0.31f), dark, NO_TEXTURE, 1);
        p_box(c, v3(-0.15f, 1.05f, 0.3f), v3(-0.08f, 1.2f, 0.35f), light, NO_TEXTURE, 1);
        p_box(c, v3(0.08f, 1.05f, 0.3f), v3(0.15f, 1.2f, 0.35f), light, NO_TEXTURE, 1);
        p_box(c, v3(-0.95f, 2.2f, -0.32f), v3(0.95f, 2.28f, 0.34f), dark, NO_TEXTURE, 1);
        p_collide(c, v3(-0.95f, 0, -0.32f), v3(0.95f, 2.3f, 0.34f));
    } else if (strcmp(name, "dresser") == 0) {
        p_box(c, v3(-0.7f, 0, -0.25f), v3(0.7f, 0.85f, 0.25f), wood, 2, 1.5f);
        for (int d = 0; d < 3; d++) {
            float y = 0.12f + 0.25f * (float)d;
            p_box(c, v3(-0.12f, y + 0.08f, 0.25f), v3(0.12f, y + 0.12f, 0.29f), light, NO_TEXTURE, 1);
        }
        p_box(c, v3(-0.5f, 0.85f, -0.22f), v3(0.5f, 1.65f, -0.18f), mtn_rgb(200, 210, 220), NO_TEXTURE, 1); /* mirror */
        p_collide(c, v3(-0.7f, 0, -0.25f), v3(0.7f, 0.9f, 0.25f));
    } else if (strcmp(name, "workbench") == 0) { /* heavy bench with a vice, tools and shavings */
        p_box(c, v3(-1.3f, 0.82f, -0.45f), v3(1.3f, 0.92f, 0.45f), light, 2, 1.0f);
        float lx[2] = {-1.2f, 1.1f}, lz[2] = {-0.4f, 0.3f};
        for (int i = 0; i < 2; i++)
            for (int k = 0; k < 2; k++) p_box(c, v3(lx[i], 0, lz[k]), v3(lx[i] + 0.1f, 0.82f, lz[k] + 0.1f), dark, NO_TEXTURE, 1);
        p_box(c, v3(-1.25f, 0.2f, -0.4f), v3(1.25f, 0.25f, 0.4f), wood, NO_TEXTURE, 1);
        p_box(c, v3(0.9f, 0.92f, 0.3f), v3(1.2f, 1.08f, 0.5f), mtn_rgb(70, 70, 76), NO_TEXTURE, 1);
        p_box(c, v3(-0.8f, 0.92f, -0.1f), v3(-0.2f, 0.98f, 0.05f), wood, NO_TEXTURE, 1);
        p_cyl(c, v3(0.0f, 0.95f, 0.1f), v3(0.35f, 0.95f, 0.2f), 0.02f, 0.02f, 4, mtn_rgb(150, 150, 155), NO_TEXTURE);
        p_cyl(c, v3(0.2f, 0.93f, -0.25f), v3(0.6f, 0.93f, -0.2f), 0.03f, 0.03f, 6, wood, NO_TEXTURE);
        p_collide(c, v3(-1.3f, 0, -0.45f), v3(1.3f, 1.0f, 0.5f));
    } else if (strcmp(name, "tool_rack") == 0) { /* pegboard on the wall with tools */
        p_box(c, v3(-1.2f, 1.0f, -0.05f), v3(1.2f, 2.2f, 0.0f), light, 3, 2.0f);
        for (int i = 0; i < 6; i++) {
            float x = -1.0f + 0.4f * (float)i;
            uint32_t tc = (i % 2) ? mtn_rgb(80, 80, 88) : wood;
            p_box(c, v3(x - 0.03f, 1.3f + 0.1f * (float)(i % 3), 0.0f), v3(x + 0.03f, 1.95f, 0.05f), tc, NO_TEXTURE, 1);
        }
        p_box(c, v3(-1.2f, 0.95f, 0.0f), v3(1.2f, 1.0f, 0.2f), dark, NO_TEXTURE, 1);
    } else if (strcmp(name, "globe") == 0) {
        p_cyl(c, v3(0, 0, 0), v3(0, 0.75f, 0), 0.05f, 0.04f, 6, dark, NO_TEXTURE);
        p_cyl(c, v3(-0.2f, 0, -0.2f), v3(0, 0.15f, 0), 0.03f, 0.03f, 4, dark, NO_TEXTURE);
        p_cyl(c, v3(0.2f, 0, -0.2f), v3(0, 0.15f, 0), 0.03f, 0.03f, 4, dark, NO_TEXTURE);
        p_cyl(c, v3(0, 0, 0.25f), v3(0, 0.15f, 0), 0.03f, 0.03f, 4, dark, NO_TEXTURE);
        mtn_material sea = mtn_mat(mtn_rgb(80, 120, 170), 28, 0, 1.0f);
        mtn_mesh_rock(c->solid, m4_mul(c->xf, m4_translate(v3(0, 1.0f, 0))), 0.28f, 3u, &sea);
        p_collide(c, v3(-0.3f, 0, -0.3f), v3(0.3f, 1.3f, 0.3f));
    } else if (strcmp(name, "chandelier") == 0) { /* hangs from the ceiling: y = arg/10 m, default 3.2 */
        float y = arg > 0 ? (float)arg * 0.1f : 3.2f;
        p_cyl(c, v3(0, y, 0), v3(0, y + 2.0f, 0), 0.02f, 0.02f, 4, dark, NO_TEXTURE);
        p_cyl(c, v3(-0.7f, y, 0), v3(0.7f, y, 0), 0.04f, 0.04f, 6, dark, NO_TEXTURE);
        p_cyl(c, v3(0, y, -0.7f), v3(0, y, 0.7f), 0.04f, 0.04f, 6, dark, NO_TEXTURE);
        float cx[4] = {-0.7f, 0.7f, 0, 0}, cz[4] = {0, 0, -0.7f, 0.7f};
        for (int i = 0; i < 4; i++) {
            p_cyl(c, v3(cx[i], y, cz[i]), v3(cx[i], y + 0.15f, cz[i]), 0.03f, 0.03f, 6, mtn_rgb(240, 230, 200), NO_TEXTURE);
            p_glow_box(c, v3(cx[i] - 0.02f, y + 0.15f, cz[i] - 0.02f), v3(cx[i] + 0.02f, y + 0.22f, cz[i] + 0.02f),
                       mtn_rgb(255, 210, 120), NO_TEXTURE);
        }
        p_light(c, v3(0, y - 0.2f, 0), v3(1.0f, 0.78f, 0.5f), 9.0f, 0.25f);
    }
}

/* ------------------------------------------------------------------ shell */

static void wall_quad(pctx *c, vec3 p0, vec3 p1, vec3 p2, vec3 p3, uint32_t col, int tile, float u, float v) {
    mtn_material m = mtn_mat(col, tile, 0, 1.0f);
    mtn_mesh_quad(c->solid, c->xf, p0, p1, p2, p3, &m, v2(0, 0), v2(u, v));
}

static void add_aabb(mtn_room *r, vec3 mn, vec3 mx) {
    if (r->collider_count >= MTN_ROOM_MAX_COLLIDERS) return;
    r->colliders[r->collider_count].mn = mn;
    r->colliders[r->collider_count].mx = mx;
    r->collider_count++;
}

static void add_walk(mtn_room *r, vec3 mn, vec3 mx) {
    if (r->walk_count >= MTN_ROOM_MAX_WALK) return;
    r->walk[r->walk_count].mn = mn;
    r->walk[r->walk_count].mx = mx;
    r->walk_count++;
}

/* a local box turned into an interior-space AABB */
static mtn_aabb local_aabb(mat4 xf, vec3 mn, vec3 mx) {
    mtn_aabb b = {v3(1e30f, 1e30f, 1e30f), v3(-1e30f, -1e30f, -1e30f)};
    for (int i = 0; i < 8; i++) {
        vec3 p = m4_mul_point(xf, v3(i & 1 ? mx.x : mn.x, i & 2 ? mx.y : mn.y, i & 4 ? mx.z : mn.z));
        b.mn = v3(mtn_minf(b.mn.x, p.x), mtn_minf(b.mn.y, p.y), mtn_minf(b.mn.z, p.z));
        b.mx = v3(mtn_maxf(b.mx.x, p.x), mtn_maxf(b.mx.y, p.y), mtn_maxf(b.mx.z, p.z));
    }
    return b;
}

typedef struct { float u0, u1, v0, v1; } hole;

/* A point on a wall of the room in local space: u along the wall, v up. The walls face
 * into the room (+x wall: normal -x, and so on); "outside" builds the outer face of the
 * +x wall (normal +x). */
static vec3 wall_point(int wall, float W, float L, float u, float v) {
    switch (wall) {
    case MTN_WALL_WINDOW: return v3(W * 0.5f, v, u);
    case MTN_WALL_BACK: return v3(-W * 0.5f, v, -u);
    case MTN_WALL_RIGHT: return v3(-u, v, L * 0.5f);
    default: return v3(u, v, -L * 0.5f);
    }
}

static float wall_half(int wall, float W, float L) { return (wall == MTN_WALL_WINDOW || wall == MTN_WALL_BACK) ? L * 0.5f : W * 0.5f; }

/* A wall with rectangular openings (sorted by u, not overlapping). Collides everywhere
 * except in openings that reach the floor. */
static void wall_with_holes(pctx *c, int wall, float W, float L, float H, const hole *hs, int nh, uint32_t col, int tile) {
    float half = wall_half(wall, W, L), u = -half;
#define WQ(a, b, lo, hi)                                                                                                  \
    wall_quad(c, wall_point(wall, W, L, a, lo), wall_point(wall, W, L, b, lo), wall_point(wall, W, L, b, hi),            \
              wall_point(wall, W, L, a, hi), col, tile, (b) - (a), (hi) - (lo))
    float solid0 = -half;
    for (int i = 0; i < nh; i++) {
        const hole *o = &hs[i];
        if (o->u0 > u) WQ(u, o->u0, 0.0f, H);
        if (o->v0 > 0.0f) WQ(o->u0, o->u1, 0.0f, o->v0);
        if (o->v1 < H) WQ(o->u0, o->u1, o->v1, H);
        u = o->u1;
        if (o->v0 <= 0.0f) { /* a doorway: the wall collides up to it */
            mtn_aabb b = local_aabb(c->xf, wall_point(wall, W, L, solid0, 0), wall_point(wall, W, L, o->u0, H));
            vec3 n = m4_mul_dir(c->xf, wall == MTN_WALL_WINDOW ? v3(1, 0, 0)
                                       : wall == MTN_WALL_BACK ? v3(-1, 0, 0)
                                       : wall == MTN_WALL_RIGHT ? v3(0, 0, 1) : v3(0, 0, -1));
            b.mn = v3_add(b.mn, v3(mtn_minf(n.x, 0) * 0.4f - 0.05f * fabsf(n.x), 0, mtn_minf(n.z, 0) * 0.4f - 0.05f * fabsf(n.z)));
            b.mx = v3_add(b.mx, v3(mtn_maxf(n.x, 0) * 0.4f + 0.05f * fabsf(n.x), 0, mtn_maxf(n.z, 0) * 0.4f + 0.05f * fabsf(n.z)));
            add_aabb(c->info, b.mn, b.mx);
            solid0 = o->u1;
        }
    }
    if (u < half) WQ(u, half, 0.0f, H);
    mtn_aabb b = local_aabb(c->xf, wall_point(wall, W, L, solid0, 0), wall_point(wall, W, L, half, H));
    vec3 n = m4_mul_dir(c->xf, wall == MTN_WALL_WINDOW ? v3(1, 0, 0)
                               : wall == MTN_WALL_BACK ? v3(-1, 0, 0)
                               : wall == MTN_WALL_RIGHT ? v3(0, 0, 1) : v3(0, 0, -1));
    b.mn = v3_add(b.mn, v3(mtn_minf(n.x, 0) * 0.4f - 0.05f * fabsf(n.x), 0, mtn_minf(n.z, 0) * 0.4f - 0.05f * fabsf(n.z)));
    b.mx = v3_add(b.mx, v3(mtn_maxf(n.x, 0) * 0.4f + 0.05f * fabsf(n.x), 0, mtn_maxf(n.z, 0) * 0.4f + 0.05f * fabsf(n.z)));
    add_aabb(c->info, b.mn, b.mx);
#undef WQ
}

/* wooden frame around an opening, on the room side */
static void opening_frame(pctx *c, int wall, float W, float L, const hole *o, int sill) {
    uint32_t fr = mtn_rgb(110, 76, 48);
    float t = 0.1f;
    vec3 in = wall == MTN_WALL_WINDOW ? v3(-1, 0, 0) : wall == MTN_WALL_BACK ? v3(1, 0, 0) : wall == MTN_WALL_RIGHT ? v3(0, 0, -1) : v3(0, 0, 1);
    /* boxes spanning wall points (u, v) to (u', v') pushed 0.1 m into the room */
    struct { float u0, u1, v0, v1; } parts[4] = {{o->u0 - t, o->u0, o->v0, o->v1 + t}, {o->u1, o->u1 + t, o->v0, o->v1 + t},
                                                  {o->u0 - t, o->u1 + t, o->v1, o->v1 + t}, {o->u0 - t, o->u1 + t, o->v0 - t, o->v0}};
    int n = sill ? 4 : 3;
    for (int i = 0; i < n; i++) {
        vec3 a = wall_point(wall, W, L, parts[i].u0, parts[i].v0), b = wall_point(wall, W, L, parts[i].u1, parts[i].v1);
        vec3 b2 = v3_add(b, v3_scale(in, i == 3 ? 0.3f : 0.1f));
        vec3 mn = v3(mtn_minf(a.x, b2.x), mtn_minf(a.y, b2.y), mtn_minf(a.z, b2.z));
        vec3 mx = v3(mtn_maxf(a.x, b2.x), mtn_maxf(a.y, b2.y), mtn_maxf(a.z, b2.z));
        p_box(c, mn, mx, fr, NO_TEXTURE, 1);
    }
}

static void big_window(pctx *c, float W, float L, const hole *o, int view_index) {
    opening_frame(c, MTN_WALL_WINDOW, W, L, o, 1);
    float hw = W * 0.5f, zc = (o->u0 + o->u1) * 0.5f, yc = (o->v0 + o->v1) * 0.5f;
    uint32_t fr = mtn_rgb(110, 76, 48);
    /* mullions */
    p_box(c, v3(hw - 0.08f, o->v0, zc - 0.04f), v3(hw, o->v1, zc + 0.04f), fr, NO_TEXTURE, 1);
    p_box(c, v3(hw - 0.08f, yc - 0.03f, o->u0), v3(hw, yc + 0.03f, o->u1), fr, NO_TEXTURE, 1);
    /* curtains */
    mtn_material cur = mtn_mat(WHITE, 18, 0, 1.0f);
    float y0 = o->v0 - 0.3f, y1 = o->v1 + 0.2f;
    mtn_mesh_quad(c->solid, c->xf, v3(hw - 0.2f, y0, o->u0 - 0.7f), v3(hw - 0.2f, y0, o->u0 - 0.1f), v3(hw - 0.2f, y1, o->u0 - 0.1f),
                  v3(hw - 0.2f, y1, o->u0 - 0.7f), &cur, v2(0, 0), v2(0.6f, 2.5f));
    mtn_mesh_quad(c->solid, c->xf, v3(hw - 0.2f, y0, o->u1 + 0.1f), v3(hw - 0.2f, y0, o->u1 + 0.7f), v3(hw - 0.2f, y1, o->u1 + 0.7f),
                  v3(hw - 0.2f, y1, o->u1 + 0.1f), &cur, v2(0, 0), v2(0.6f, 2.5f));
    /* glass with a soft reflection (glow mesh, blended after the view) */
    mtn_material glass = mtn_mat(mtn_rgb(255, 255, 255) & 0x40FFFFFFu, 13, MTN_VF_ABS_UV, 1.0f);
    mtn_mesh_quad(c->glow, c->xf, v3(hw - 0.03f, o->v0, o->u0), v3(hw - 0.03f, o->v0, o->u1), v3(hw - 0.03f, o->v1, o->u1),
                  v3(hw - 0.03f, o->v1, o->u0), &glass, v2(0, 0), v2(1, 1));
    c->info->window_frame = m4_mul(c->xf, mtn_portal_frame(v3(hw, yc, zc), v3(1, 0, 0)));
    c->info->window_size = v3((o->u1 - o->u0) * 0.5f, (o->v1 - o->v0) * 0.5f, 0);
    (void)view_index;
}

/* The balcony: a railed platform outside the window wall, the outer face of the wall
 * around it, and the box of portal quads through which the view opens on all sides. */
static void balcony(pctx *c, float W, float L, float H, const hole *door) {
    mtn_room *ri = c->info;
    float x0 = W * 0.5f, x1 = x0 + MTN_BALCONY_DEPTH, zb = mtn_minf(1.8f, L * 0.5f - 0.3f);
    uint32_t wood = mtn_rgb(150, 104, 66), dark = mtn_rgb(96, 66, 44);
    p_box(c, v3(x0, -0.25f, -zb), v3(x1, 0.0f, zb), WHITE, 2, 0.8f);
    p_box(c, v3(x0, -0.6f, -0.1f), v3(x1, -0.25f, 0.1f), dark, NO_TEXTURE, 1); /* bracket */
    /* railing: posts, top and bottom rails on the three open sides */
    for (int sd = 0; sd < 3; sd++) {
        vec3 a = sd == 0 ? v3(x0, 0, -zb) : sd == 1 ? v3(x1, 0, -zb) : v3(x1, 0, zb);
        vec3 b = sd == 0 ? v3(x1, 0, -zb) : sd == 1 ? v3(x1, 0, zb) : v3(x0, 0, zb);
        float len = v3_dist(a, b);
        int posts = (int)(len / 0.5f) + 1;
        for (int i = 0; i <= posts; i++) {
            vec3 p = v3_lerp(a, b, (float)i / (float)posts);
            p_box(c, v3(p.x - 0.03f, 0, p.z - 0.03f), v3(p.x + 0.03f, 1.0f, p.z + 0.03f), wood, NO_TEXTURE, 1);
        }
        p_cyl(c, v3(a.x, 1.02f, a.z), v3(b.x, 1.02f, b.z), 0.05f, 0.05f, 6, dark, NO_TEXTURE);
        p_cyl(c, v3(a.x, 0.12f, a.z), v3(b.x, 0.12f, b.z), 0.03f, 0.03f, 4, dark, NO_TEXTURE);
        mtn_aabb r = local_aabb(c->xf, v3(mtn_minf(a.x, b.x) - 0.06f, 0, mtn_minf(a.z, b.z) - 0.06f),
                                v3(mtn_maxf(a.x, b.x) + 0.06f, 1.1f, mtn_maxf(a.z, b.z) + 0.06f));
        add_aabb(ri, r.mn, r.mx);
    }
    mtn_aabb wa = local_aabb(c->xf, v3(x0 - 0.3f, 0, -zb), v3(x1, 0, zb));
    add_walk(ri, wa.mn, wa.mx);
    /* the outer face of the wall around the balcony, with eaves */
    float zw = zb + 0.6f, yb = -3.0f, yt = H + 1.0f;
    uint32_t logs = mtn_rgb(235, 225, 210);
    hole hs[1] = {*door};
#define OQ(a, b, lo, hi)                                                                                                  \
    wall_quad(c, v3(x0 + 0.005f, lo, -(a)), v3(x0 + 0.005f, lo, -(b)), v3(x0 + 0.005f, hi, -(b)), v3(x0 + 0.005f, hi, -(a)), \
              logs, 0, ((b) - (a)) * 0.3f, ((hi) - (lo)) * 0.3f)
    OQ(-zw, -hs[0].u1, yb, yt);
    OQ(-hs[0].u1, -hs[0].u0, yb, hs[0].v0);
    OQ(-hs[0].u1, -hs[0].u0, hs[0].v1, yt);
    OQ(-hs[0].u0, zw, yb, yt);
#undef OQ
    p_box(c, v3(x0, H + 0.5f, -zw), v3(x0 + 0.7f, H + 0.62f, zw), mtn_rgb(120, 80, 60), 4, 1.0f);
    /* the box of view: front, two sides, top and bottom, all facing inwards */
    float xb = x0 + MTN_BALCONY_DEPTH + 0.4f;
    vec3 q[5][4] = {
        {v3(xb, yb, -zw), v3(xb, yb, zw), v3(xb, yt, zw), v3(xb, yt, -zw)},
        {v3(xb, yb, zw), v3(x0, yb, zw), v3(x0, yt, zw), v3(xb, yt, zw)},
        {v3(x0, yb, -zw), v3(xb, yb, -zw), v3(xb, yt, -zw), v3(x0, yt, -zw)},
        {v3(x0, yt, -zw), v3(xb, yt, -zw), v3(xb, yt, zw), v3(x0, yt, zw)},
        {v3(x0, yb, -zw), v3(x0, yb, zw), v3(xb, yb, zw), v3(xb, yb, -zw)},
    };
    for (int f = 0; f < 5; f++)
        for (int k = 0; k < 4; k++) ri->balcony_quads[f][k] = m4_mul_point(c->xf, q[f][k]);
    ri->window_frame = m4_mul(c->xf, mtn_portal_frame(v3(x0, 1.5f, 0), v3(1, 0, 0)));
    ri->window_size = v3(zw, (yt - yb) * 0.5f, 0);
}

/* A corridor from this room's doorway to the neighbour's, in interior space. */
static void corridor(pctx *c, const mtn_room_type *rt, vec3 a, vec3 b, uint32_t seed) {
    mtn_room *ri = c->info;
    mat4 keep = c->xf;
    vec3 d = v3_sub(b, a);
    float len = v3_len(d);
    if (len < 0.05f) return;
    vec3 x = v3_scale(d, 1.0f / len), y = v3(0, 1, 0), z = v3_cross(x, y);
    c->xf = m4_from_basis(a, x, y, z);
    /* local: x along the corridor from 0 to len, z across */
    float hw = MTN_CORRIDOR_HALF, H = MTN_CORRIDOR_HEIGHT;
    uint32_t wall = rt ? rt->wall_color : WHITE, floor = rt ? rt->floor_color : WHITE;
    int wt = rt ? rt->wall_tile : 3, ft = rt ? rt->floor_tile : 2;
    wall_quad(c, v3(0, 0, hw), v3(len, 0, hw), v3(len, 0, -hw), v3(0, 0, -hw), floor, ft, len, 2 * hw);
    wall_quad(c, v3(0, H, -hw), v3(len, H, -hw), v3(len, H, hw), v3(0, H, hw), floor, ft, len, 2 * hw);
    wall_quad(c, v3(0, 0, -hw), v3(len, 0, -hw), v3(len, H, -hw), v3(0, H, -hw), wall, wt, len, H);
    wall_quad(c, v3(len, 0, hw), v3(0, 0, hw), v3(0, H, hw), v3(len, H, hw), wall, wt, len, H);
    /* end caps around the doorways, seen from inside the corridor */
    for (int e = 0; e < 2; e++) {
        float ex = e ? len : 0.0f, s = e ? -1.0f : 1.0f;
        vec3 p[3][4] = {{v3(ex, 0, -hw), v3(ex, 0, -DOOR_HALF), v3(ex, H, -DOOR_HALF), v3(ex, H, -hw)},
                        {v3(ex, 0, DOOR_HALF), v3(ex, 0, hw), v3(ex, H, hw), v3(ex, H, DOOR_HALF)},
                        {v3(ex, DOOR_TOP, -DOOR_HALF), v3(ex, DOOR_TOP, DOOR_HALF), v3(ex, H, DOOR_HALF), v3(ex, H, -DOOR_HALF)}};
        for (int k = 0; k < 3; k++) {
            if (s > 0) wall_quad(c, p[k][1], p[k][0], p[k][3], p[k][2], wall, wt, 0.5f, H);
            else wall_quad(c, p[k][0], p[k][1], p[k][2], p[k][3], wall, wt, 0.5f, H);
        }
    }
    /* a runner rug and a lamp half way */
    mtn_material rug = mtn_mat(mtn_rgb(200, 150 + (int)(seed % 60u), 120), 9, 0, 1.0f);
    mtn_mesh_quad(c->solid, c->xf, v3(0.3f, 0.01f, 0.45f), v3(len - 0.3f, 0.01f, 0.45f), v3(len - 0.3f, 0.01f, -0.45f), v3(0.3f, 0.01f, -0.45f),
                  &rug, v2(0, 0), v2(1, len * 0.5f));
    mtn_material lamp = mtn_mat(mtn_rgb(255, 214, 150), NO_TEXTURE, MTN_VF_EMISSIVE, 1.0f);
    mtn_mesh_cylinder(c->glow, c->xf, v3(len * 0.5f, H - 0.3f, 0), v3(len * 0.5f, H - 0.05f, 0), 0.18f, 0.1f, 6, &lamp, 1);
    p_light(c, v3(len * 0.5f, H - 0.5f, 0), v3(1.0f, 0.78f, 0.5f), 4.0f, 0.0f);
    mtn_aabb s0 = local_aabb(c->xf, v3(0, 0, -hw - 0.3f), v3(len, H, -hw)), s1 = local_aabb(c->xf, v3(0, 0, hw), v3(len, H, hw + 0.3f));
    add_aabb(ri, s0.mn, s0.mx);
    add_aabb(ri, s1.mn, s1.mx);
    mtn_aabb w = local_aabb(c->xf, v3(-0.3f, 0, -hw), v3(len + 0.3f, 0, hw));
    add_walk(ri, w.mn, w.mx);
    c->xf = keep;
}

static int skip_for_doors(float W, float L, int closed_door, vec3 p) {
    /* nothing stands in front of a doorway: back, right, left and maybe the window side */
    const float clear_u = DOOR_HALF + 0.5f, clear_d = 1.4f;
    if (fabsf(p.z) < clear_u && p.x < -W * 0.5f + clear_d) return 1;
    if (fabsf(p.x) < clear_u && p.z > L * 0.5f - clear_d) return 1;
    if (fabsf(p.x) < clear_u && p.z < -L * 0.5f + clear_d) return 1;
    if (closed_door && fabsf(p.z) < clear_u + 0.5f && p.x > W * 0.5f - clear_d) return 1;
    return 0;
}

static vec3 item_position(const mtn_room_item *it, float W, float L, float *rot) {
    float hw = W * 0.5f, hl = L * 0.5f;
    vec3 p = v3(it->x + it->fx * hw, 0, it->z + it->fz * hl);
    *rot = it->rot;
    switch (it->wall) {
    case MTN_WALL_WINDOW: p = v3(hw - it->inset, 0, it->along * hl + it->at); *rot += -90.0f; break;
    case MTN_WALL_BACK: p = v3(-hw + it->inset, 0, it->along * hl + it->at); *rot += 90.0f; break;
    case MTN_WALL_RIGHT: p = v3(it->along * hw + it->at, 0, hl - it->inset); *rot += 180.0f; break;
    case MTN_WALL_LEFT: p = v3(it->along * hw + it->at, 0, -hl + it->inset); break;
    default: break;
    }
    p.x = mtn_clampf(p.x, -hw + 0.15f, hw - 0.15f);
    p.z = mtn_clampf(p.z, -hl + 0.15f, hl - 0.15f);
    return p;
}

void mtn_house_build_room(const mtn_house *h, const mtn_terrain *t, int ci, int cj, mtn_room *info, mtn_mesh *solid,
                          mtn_mesh *sprites, mtn_mesh *glow) {
    memset(info, 0, sizeof *info);
    mtn_room_plan pl;
    mtn_house_room_plan(h, ci, cj, &pl);
    mtn_room_type fallback;
    mtn_room_type_defaults(&fallback);
    const mtn_room_type *rt = pl.type >= 0 ? &h->types[pl.type] : &fallback;
    float W = pl.width, L = pl.length, H = pl.height, hw = W * 0.5f, hl = L * 0.5f;
    int cabin = rt->is_cabin && ci == 0 && cj == 0;
    info->built = 1;
    info->ci = ci;
    info->cj = cj;
    info->type = pl.type;
    info->closed = pl.closed;
    info->balcony = pl.balcony;
    info->width = W;
    info->length = L;
    info->height = H;
    info->center = pl.center;
    info->xf = pl.xf;
    int vc = t && t->vp_count > 0 ? t->vp_count : 24;
    int vb = vc == 2 ? 1 : 2;
    info->view = cabin ? -1 : (int)((((int64_t)(h->seed % 97u) + ci + (int64_t)vb * cj) % vc + vc) % vc);
    pctx c = {solid, sprites, glow, info, pl.xf};
    uint32_t wc = rt->wall_color;

    /* floor, ceiling and beams */
    wall_quad(&c, v3(-hw, 0, hl), v3(hw, 0, hl), v3(hw, 0, -hl), v3(-hw, 0, -hl), rt->floor_color, rt->floor_tile, W, L);
    wall_quad(&c, v3(-hw, H, -hl), v3(hw, H, -hl), v3(hw, H, hl), v3(-hw, H, hl), rt->floor_color, rt->floor_tile, W, L);
    for (float bz = -hl + 1.5f; bz < hl - 0.5f; bz += 2.0f)
        p_box(&c, v3(-hw, H - 0.25f, bz - 0.12f), v3(hw, H, bz + 0.12f), mtn_rgb(120, 84, 54), 0, 1.0f);
    mtn_aabb fl = local_aabb(pl.xf, v3(-hw, 0, -hl), v3(hw, 0, hl));
    add_walk(info, fl.mn, fl.mx);

    /* three walls with doorways */
    hole door = {-DOOR_HALF, DOOR_HALF, 0.0f, DOOR_TOP};
    for (int w = MTN_WALL_BACK; w <= MTN_WALL_LEFT; w++) {
        wall_with_holes(&c, w, W, L, H, &door, 1, wc, rt->wall_tile);
        opening_frame(&c, w, W, L, &door, 0);
    }
    /* the fourth: the front door and a window (cabin), a balcony door or a big window */
    if (cabin) {
        hole hs[2] = {{-DOOR_HALF, DOOR_HALF, 0.0f, DOOR_TOP}, {1.3f, 2.7f, 0.9f, 2.2f}};
        wall_with_holes(&c, MTN_WALL_WINDOW, W, L, H, hs, 2, wc, rt->wall_tile);
        opening_frame(&c, MTN_WALL_WINDOW, W, L, &hs[0], 0);
        big_window(&c, W, L, &hs[1], -1);
    } else if (pl.balcony) {
        hole bd = {-1.0f, 1.0f, 0.0f, 2.4f};
        wall_with_holes(&c, MTN_WALL_WINDOW, W, L, H, &bd, 1, wc, rt->wall_tile);
        opening_frame(&c, MTN_WALL_WINDOW, W, L, &bd, 0);
        balcony(&c, W, L, H, &bd);
    } else {
        float ww = mtn_minf(2.0f, hl - 0.6f), wy1 = mtn_minf(3.0f, H - 0.4f);
        hole win = {-ww, ww, 0.8f, wy1};
        wall_with_holes(&c, MTN_WALL_WINDOW, W, L, H, &win, 1, wc, rt->wall_tile);
        big_window(&c, W, L, &win, info->view);
    }

    /* corridors to the neighbours east and north (west and south belong to them) */
    for (int s = MTN_SIDE_EAST; s <= MTN_SIDE_NORTH; s += 2) {
        if (pl.closed == s) continue;
        mtn_room_plan nb;
        mtn_house_room_plan(h, ci + SIDE_DI[s], cj + SIDE_DJ[s], &nb);
        int axis = s == MTN_SIDE_EAST ? 0 : 1;
        vec3 dir = side_dir(s);
        vec3 a = v3_add(pl.center, v3_scale(dir, plan_half(&pl, axis)));
        vec3 b = v3_sub(nb.center, v3_scale(dir, plan_half(&nb, axis)));
        corridor(&c, rt, a, b, h->seed + (uint32_t)(ci * 31 + cj));
    }

    /* ceiling lights: one per 40 m2 or so */
    mtn_material lamp = mtn_mat(mtn_rgb(255, 220, 160), NO_TEXTURE, MTN_VF_EMISSIVE, 1.0f);
    int nl = W * L > 80.0f ? 2 : 1;
    for (int i = 0; i < nl; i++) {
        float z = nl == 1 ? 0.0f : (i ? hl * 0.45f : -hl * 0.45f);
        mtn_mesh_cylinder(glow, c.xf, v3(0, H - 0.6f, z), v3(0, H - 0.35f, z), 0.25f, 0.12f, 8, &lamp, 1);
        p_light(&c, v3(0, H - 0.8f, z), rt->light_color, 7.0f + 0.25f * (W + L) / (float)nl, 0.0f);
    }

    /* furniture */
    for (int i = 0; i < rt->item_count; i++) {
        const mtn_room_item *it = &rt->items[i];
        float rot;
        vec3 p = item_position(it, W, L, &rot);
        if (skip_for_doors(W, L, cabin || pl.balcony, p)) continue;
        c.xf = m4_mul(pl.xf, m4_mul(m4_translate(p), m4_mul(m4_rotate_y(rot * MTN_DEG2RAD), m4_scale(v3(it->scale, it->scale, it->scale)))));
        prefab(&c, it->prefab, it->arg, h->seed + (uint32_t)i * 13u + (uint32_t)(ci * 7 + cj * 131));
    }
}

int mtn_room_contains(const mtn_room *r, vec3 p) {
    for (int i = 0; i < r->walk_count; i++) {
        const mtn_aabb *b = &r->walk[i];
        if (p.x >= b->mn.x && p.x <= b->mx.x && p.z >= b->mn.z && p.z <= b->mx.z) return 1;
    }
    return 0;
}

int mtn_room_collide(const mtn_room *r, vec3 *pos, float radius) {
    int hit = 0;
    for (int i = 0; i < r->collider_count; i++) {
        const mtn_aabb *b = &r->colliders[i];
        if (b->mx.y < pos->y + 0.25f || b->mn.y > pos->y + 1.7f) continue;
        float x0 = b->mn.x - radius, x1 = b->mx.x + radius, z0 = b->mn.z - radius, z1 = b->mx.z + radius;
        if (pos->x <= x0 || pos->x >= x1 || pos->z <= z0 || pos->z >= z1) continue;
        float dl = pos->x - x0, dr = x1 - pos->x, dn = pos->z - z0, df = z1 - pos->z;
        float m = mtn_minf(mtn_minf(dl, dr), mtn_minf(dn, df));
        if (m == dl) pos->x = x0;
        else if (m == dr) pos->x = x1;
        else if (m == dn) pos->z = z0;
        else pos->z = z1;
        hit = 1;
    }
    return hit;
}
