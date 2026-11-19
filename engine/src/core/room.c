/* Mountain engine - the endless interior of the magic house: room sequence,
 * room shells with doorways and a big window, furniture prefabs, lights and
 * colliders. Rooms are laid end to end along +z in their own "interior space". MIT License. */
#include "mtn/house.h"
#include "mtn/rng.h"

#include <math.h>
#include <string.h>

#define NO_TEXTURE 255
#define WHITE 0xFFFFFFFFu

/* ------------------------------------------------------------------ sequence */

static int non_cabin_types(const mtn_house *h, int *out) {
    int k = 0;
    for (int i = 0; i < h->type_count; i++)
        if (!h->types[i].is_cabin) out[k++] = i;
    return k;
}

static void block_perm(const mtn_house *h, int block, const int *types, int k, int *perm) {
    for (int i = 0; i < k; i++) perm[i] = types[i];
    mtn_rng r;
    mtn_rng_seed(&r, h->seed ^ 0xBEEFu, (uint64_t)block);
    for (int i = k - 1; i > 0; i--) {
        int j = mtn_rng_int(&r, 0, i);
        int t = perm[i];
        perm[i] = perm[j];
        perm[j] = t;
    }
}

int mtn_house_room_type_for(const mtn_house *h, int index) {
    if (h->type_count == 0) return -1;
    int cabin = 0;
    for (int i = 0; i < h->type_count; i++)
        if (h->types[i].is_cabin) { cabin = i; break; }
    if (index <= 0) return cabin;
    int types[MTN_MAX_ROOM_TYPES];
    int k = non_cabin_types(h, types);
    if (k == 0) return cabin;
    if (k == 1) return types[0];
    int i = index - 1;
    int block = i / k, pos = i % k;
    /* two types can only alternate (the swap below would change the previous block's last room) */
    if (k == 2) block = 0;
    int perm[MTN_MAX_ROOM_TYPES], prev[MTN_MAX_ROOM_TYPES];
    block_perm(h, block, types, k, perm);
    if (block > 0) {
        block_perm(h, block - 1, types, k, prev);
        if (perm[0] == prev[k - 1]) {
            int t = perm[0];
            perm[0] = perm[1];
            perm[1] = t;
        }
    }
    return perm[pos];
}

int mtn_house_room_at(float z) { return z < 0.0f ? 0 : (int)floorf(z / MTN_ROOM_LENGTH); }

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
    }
}

/* ------------------------------------------------------------------ shell */

static void wall_quad(pctx *c, vec3 p0, vec3 p1, vec3 p2, vec3 p3, uint32_t col, int tile, float u, float v) {
    mtn_material m = mtn_mat(col, tile, 0, 1.0f);
    mtn_mesh_quad(c->solid, c->xf, p0, p1, p2, p3, &m, v2(0, 0), v2(u, v));
}

static void add_aabb(mtn_room *r, vec3 mn, vec3 mx) {
    if (r->collider_count >= 96) return;
    r->colliders[r->collider_count].mn = mn;
    r->colliders[r->collider_count].mx = mx;
    r->collider_count++;
}

void mtn_house_build_room(const mtn_house *h, const mtn_terrain *t, int index, mtn_room *info, mtn_mesh *solid,
                          mtn_mesh *sprites, mtn_mesh *glow) {
    memset(info, 0, sizeof *info);
    int type = mtn_house_room_type_for(h, index);
    mtn_room_type fallback;
    mtn_room_type_defaults(&fallback);
    const mtn_room_type *rt = type >= 0 ? &h->types[type] : &fallback;
    float W = rt->width, H = rt->height, L = MTN_ROOM_LENGTH, z0 = (float)index * L;
    float hw = W * 0.5f, dw = MTN_DOOR_WIDTH * 0.5f, dh = MTN_DOOR_HEIGHT + 0.1f;
    info->index = index;
    info->type = type;
    info->z0 = z0;
    info->length = L;
    info->width = W;
    info->height = H;
    info->view = index == 0 ? -1 : (int)((h->seed % 97u) + (uint32_t)index - 1u);
    pctx c = {solid, sprites, glow, info, m4_identity()};

    /* floor and ceiling */
    wall_quad(&c, v3(-hw, 0, z0 + L), v3(hw, 0, z0 + L), v3(hw, 0, z0), v3(-hw, 0, z0), rt->floor_color, rt->floor_tile, W, L);
    wall_quad(&c, v3(-hw, H, z0), v3(hw, H, z0), v3(hw, H, z0 + L), v3(-hw, H, z0 + L), rt->floor_color, rt->floor_tile, W, L);
    for (float bz = z0 + 1.5f; bz < z0 + L; bz += 2.0f)
        p_box(&c, v3(-hw, H - 0.22f, bz - 0.11f), v3(hw, H, bz + 0.11f), mtn_rgb(120, 82, 52), NO_TEXTURE, 1);
    /* left wall (solid) */
    wall_quad(&c, v3(-hw, 0, z0 + L), v3(-hw, 0, z0), v3(-hw, H, z0), v3(-hw, H, z0 + L), rt->wall_color, rt->wall_tile, L, H);
    /* right wall with the big window */
    float wz0 = z0 + L * 0.5f - 2.0f, wz1 = z0 + L * 0.5f + 2.0f, wy0 = 0.8f, wy1 = mtn_minf(3.0f, H - 0.4f);
    uint32_t wc = rt->wall_color;
    wall_quad(&c, v3(hw, 0, z0), v3(hw, 0, z0 + L), v3(hw, wy0, z0 + L), v3(hw, wy0, z0), wc, rt->wall_tile, L, wy0);
    wall_quad(&c, v3(hw, wy1, z0), v3(hw, wy1, z0 + L), v3(hw, H, z0 + L), v3(hw, H, z0), wc, rt->wall_tile, L, H - wy1);
    wall_quad(&c, v3(hw, wy0, z0), v3(hw, wy0, wz0), v3(hw, wy1, wz0), v3(hw, wy1, z0), wc, rt->wall_tile, wz0 - z0, wy1 - wy0);
    wall_quad(&c, v3(hw, wy0, wz1), v3(hw, wy0, z0 + L), v3(hw, wy1, z0 + L), v3(hw, wy1, wz1), wc, rt->wall_tile, z0 + L - wz1, wy1 - wy0);
    /* window frame, mullions and sill */
    uint32_t fr = mtn_rgb(110, 76, 48);
    p_box(&c, v3(hw - 0.12f, wy0 - 0.1f, wz0 - 0.1f), v3(hw, wy0, wz1 + 0.1f), fr, NO_TEXTURE, 1);
    p_box(&c, v3(hw - 0.35f, wy0 - 0.06f, wz0 - 0.1f), v3(hw, wy0, wz1 + 0.1f), fr, NO_TEXTURE, 1);
    p_box(&c, v3(hw - 0.12f, wy1, wz0 - 0.1f), v3(hw, wy1 + 0.1f, wz1 + 0.1f), fr, NO_TEXTURE, 1);
    p_box(&c, v3(hw - 0.12f, wy0, wz0 - 0.1f), v3(hw, wy1, wz0), fr, NO_TEXTURE, 1);
    p_box(&c, v3(hw - 0.12f, wy0, wz1), v3(hw, wy1, wz1 + 0.1f), fr, NO_TEXTURE, 1);
    p_box(&c, v3(hw - 0.08f, wy0, z0 + L * 0.5f - 0.04f), v3(hw, wy1, z0 + L * 0.5f + 0.04f), fr, NO_TEXTURE, 1);
    p_box(&c, v3(hw - 0.08f, (wy0 + wy1) * 0.5f - 0.03f, wz0), v3(hw, (wy0 + wy1) * 0.5f + 0.03f, wz1), fr, NO_TEXTURE, 1);
    /* curtains */
    mtn_material cur = mtn_mat(WHITE, 18, 0, 1.0f);
    mtn_mesh_quad(solid, c.xf, v3(hw - 0.2f, wy0 - 0.3f, wz0 - 0.7f), v3(hw - 0.2f, wy0 - 0.3f, wz0 - 0.1f), v3(hw - 0.2f, wy1 + 0.2f, wz0 - 0.1f),
                  v3(hw - 0.2f, wy1 + 0.2f, wz0 - 0.7f), &cur, v2(0, 0), v2(0.6f, 2.5f));
    mtn_mesh_quad(solid, c.xf, v3(hw - 0.2f, wy0 - 0.3f, wz1 + 0.1f), v3(hw - 0.2f, wy0 - 0.3f, wz1 + 0.7f), v3(hw - 0.2f, wy1 + 0.2f, wz1 + 0.7f),
                  v3(hw - 0.2f, wy1 + 0.2f, wz1 + 0.1f), &cur, v2(0, 0), v2(0.6f, 2.5f));
    /* window glass with a soft reflection (glow mesh = materials atlas, blended, drawn after the view) */
    mtn_material glass = mtn_mat(mtn_rgb(255, 255, 255) & 0x40FFFFFFu, 13, MTN_VF_ABS_UV, 1.0f);
    mtn_mesh_quad(glow, c.xf, v3(hw - 0.03f, wy0, wz0), v3(hw - 0.03f, wy0, wz1), v3(hw - 0.03f, wy1, wz1), v3(hw - 0.03f, wy1, wz0), &glass,
                  v2(0, 0), v2(1, 1));
    info->window_frame = mtn_portal_frame(v3(hw, (wy0 + wy1) * 0.5f, z0 + L * 0.5f), v3(1, 0, 0));
    info->window_size = v3(2.0f, (wy1 - wy0) * 0.5f, 0);

    /* front and back walls with doorways (single sided, facing into the room) */
    for (int s = 0; s < 2; s++) {
        float z = s ? z0 + L : z0;
        if (s == 0) {
            wall_quad(&c, v3(-hw, 0, z), v3(-dw, 0, z), v3(-dw, H, z), v3(-hw, H, z), wc, rt->wall_tile, hw - dw, H);
            wall_quad(&c, v3(dw, 0, z), v3(hw, 0, z), v3(hw, H, z), v3(dw, H, z), wc, rt->wall_tile, hw - dw, H);
            wall_quad(&c, v3(-dw, dh, z), v3(dw, dh, z), v3(dw, H, z), v3(-dw, H, z), wc, rt->wall_tile, 2 * dw, H - dh);
        } else {
            wall_quad(&c, v3(-dw, 0, z), v3(-hw, 0, z), v3(-hw, H, z), v3(-dw, H, z), wc, rt->wall_tile, hw - dw, H);
            wall_quad(&c, v3(hw, 0, z), v3(dw, 0, z), v3(dw, H, z), v3(hw, H, z), wc, rt->wall_tile, hw - dw, H);
            wall_quad(&c, v3(dw, dh, z), v3(-dw, dh, z), v3(-dw, H, z), v3(dw, H, z), wc, rt->wall_tile, 2 * dw, H - dh);
        }
        float zi = s ? z - 0.1f : z;
        p_box(&c, v3(-dw - 0.12f, 0, zi), v3(-dw, dh + 0.1f, zi + 0.1f), fr, NO_TEXTURE, 1);
        p_box(&c, v3(dw, 0, zi), v3(dw + 0.12f, dh + 0.1f, zi + 0.1f), fr, NO_TEXTURE, 1);
        p_box(&c, v3(-dw - 0.12f, dh, zi), v3(dw + 0.12f, dh + 0.12f, zi + 0.1f), fr, NO_TEXTURE, 1);
        /* walls collide everywhere except in the doorway */
        add_aabb(info, v3(-hw - 1.0f, 0, z - 0.15f), v3(-dw, H, z + 0.15f));
        add_aabb(info, v3(dw, 0, z - 0.15f), v3(hw + 1.0f, H, z + 0.15f));
    }
    /* skirting boards */
    p_box(&c, v3(-hw, 0, z0), v3(-hw + 0.04f, 0.12f, z0 + L), fr, NO_TEXTURE, 1);
    p_box(&c, v3(hw - 0.04f, 0, z0), v3(hw, 0.12f, z0 + L), fr, NO_TEXTURE, 1);
    add_aabb(info, v3(-hw - 1.0f, 0, z0 - 0.2f), v3(-hw, H, z0 + L + 0.2f));
    add_aabb(info, v3(hw, 0, z0 - 0.2f), v3(hw + 1.0f, H, z0 + L + 0.2f));
    /* the front door itself (open or closed) is handled dynamically by the game's collider */

    /* ceiling light */
    mtn_material lamp = mtn_mat(mtn_rgb(255, 220, 160), NO_TEXTURE, MTN_VF_EMISSIVE, 1.0f);
    mtn_mesh_cylinder(glow, c.xf, v3(0, H - 0.6f, z0 + L * 0.5f), v3(0, H - 0.35f, z0 + L * 0.5f), 0.25f, 0.12f, 8, &lamp, 1);
    p_light(&c, v3(0, H - 0.8f, z0 + L * 0.5f), rt->light_color, 9.0f, 0.0f);

    /* furniture */
    for (int i = 0; i < rt->item_count; i++) {
        const mtn_room_item *it = &rt->items[i];
        c.xf = m4_mul(m4_translate(v3(it->x, 0, z0 + it->z)),
                      m4_mul(m4_rotate_y(it->rot * MTN_DEG2RAD), m4_scale(v3(it->scale, it->scale, it->scale))));
        prefab(&c, it->prefab, it->arg, h->seed + (uint32_t)i * 13u + (uint32_t)index);
    }
    (void)t;
}

int mtn_room_contains(const mtn_room *r, vec3 p) {
    return p.x >= -r->width * 0.5f && p.x <= r->width * 0.5f && p.z >= r->z0 && p.z <= r->z0 + r->length;
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
