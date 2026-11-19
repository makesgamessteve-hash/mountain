/* Mountain engine - plants: species data, placement, LOD meshes, ground cover. MIT License. */
#include "mtn/flora.h"
#include "mtn/rng.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define NO_TEXTURE 255

void mtn_flora_init(mtn_flora *f) { memset(f, 0, sizeof *f); }

void mtn_flora_free(mtn_flora *f) {
    free(f->plants);
    free(f->chunk_start);
    free(f->chunk_count);
    free(f->chunk_version);
    int sc = f->species_count;
    mtn_plant_species keep[MTN_MAX_SPECIES];
    memcpy(keep, f->species, sizeof keep);
    memset(f, 0, sizeof *f);
    memcpy(f->species, keep, sizeof keep);
    f->species_count = sc;
}

void mtn_flora_clear_species(mtn_flora *f) { f->species_count = 0; }

void mtn_species_defaults(mtn_plant_species *s) {
    memset(s, 0, sizeof *s);
    strcpy(s->name, "plant");
    s->kind = MTN_PLANT_TREE;
    s->crown = MTN_CROWN_ROUND;
    s->height_min = 6.0f;
    s->height_max = 12.0f;
    s->trunk_radius = 0.3f;
    s->crown_width = 0.3f;
    s->leaf_tile_count = 1;
    s->bark_tile = 6;
    s->bark_color = v3(0.45f, 0.35f, 0.27f);
    for (int k = 0; k < MTN_SEASON_COUNT; k++) s->leaf_color[k] = v3(0.35f, 0.55f, 0.25f);
    s->blossom_tile = -1;
    s->alt_min = 0.0f;
    s->alt_max = 1.0f;
    s->slope_max = 0.6f;
    s->moist_min = 0.0f;
    s->moist_max = 1.0f;
    s->biome_mask = 0xFE; /* everything but water */
    s->density = 0.5f;
    s->patchiness = 0.3f;
}

int mtn_flora_add_species(mtn_flora *f, const mtn_plant_species *s) {
    if (f->species_count >= MTN_MAX_SPECIES) return -1;
    f->species[f->species_count] = *s;
    if (f->species[f->species_count].leaf_tile_count < 1) f->species[f->species_count].leaf_tile_count = 1;
    if (f->species[f->species_count].leaf_tile_count > 4) f->species[f->species_count].leaf_tile_count = 4;
    return f->species_count++;
}

int mtn_flora_find_species(const mtn_flora *f, const char *name) {
    for (int i = 0; i < f->species_count; i++)
        if (strcmp(f->species[i].name, name) == 0) return i;
    return -1;
}

static float band(float v, float lo, float hi, float soft) {
    return mtn_smoothstep(lo - soft, lo + soft, v) * (1.0f - mtn_smoothstep(hi - soft, hi + soft, v));
}

float mtn_flora_habitat(const mtn_plant_species *s, const mtn_terrain *t, float x, float z) {
    mtn_biome b = mtn_terrain_biome(t, x, z);
    if (!(s->biome_mask & (1 << b))) return 0.0f;
    if (mtn_terrain_water(t, x, z) != MTN_NO_WATER) return 0.0f;
    float a = mtn_terrain_altitude_frac(t, mtn_terrain_height(t, x, z));
    float sl = mtn_terrain_slope(t, x, z);
    float m = mtn_terrain_moisture(t, x, z);
    float fa = band(a, s->alt_min, s->alt_max, 0.03f);
    float fs = 1.0f - mtn_smoothstep(s->slope_max - 0.05f, s->slope_max + 0.05f, sl);
    float fm = band(m, s->moist_min, s->moist_max, 0.1f);
    float keep_clear = s->kind == MTN_PLANT_GROUND ? 0.8f : 3.0f;
    float ft = mtn_smoothstep(keep_clear, keep_clear + 2.5f, mtn_terrain_trail_distance(t, x, z));
    return fa * fs * fm * ft;
}

int mtn_flora_chunk_of(const mtn_flora *f, float x, float z) {
    int cx = mtn_clampi((int)floorf(x / MTN_FLORA_CHUNK), 0, f->chunks - 1);
    int cz = mtn_clampi((int)floorf(z / MTN_FLORA_CHUNK), 0, f->chunks - 1);
    return cz * f->chunks + cx;
}

static float patch(const mtn_plant_species *s, int si, float x, float z, uint32_t seed) {
    float n = mtn_fbm2(x * 0.008f, z * 0.008f, seed + (uint32_t)si * 7u, 3, 2.0f, 0.5f) * 0.5f + 0.5f;
    return mtn_lerpf(1.0f, mtn_smoothstep(0.38f, 0.62f, n), s->patchiness);
}

static void add_plant(mtn_flora *f, const mtn_plant *p) {
    if (f->count == f->cap) {
        f->cap = f->cap ? f->cap * 2 : 1024;
        f->plants = realloc(f->plants, sizeof(mtn_plant) * (size_t)f->cap);
    }
    f->plants[f->count++] = *p;
}

static void place_pass(mtn_flora *f, const mtn_terrain *t, int kind, float cs, uint32_t seed) {
    float limit = t->p.playable_radius * t->world_size * 0.5f + 350.0f;
    int ncell = (int)(t->world_size / cs);
    float suit[MTN_MAX_SPECIES];
    for (int cz = 0; cz < ncell; cz++)
        for (int cx = 0; cx < ncell; cx++) {
            uint32_t h = mtn_hash2i(cx, cz, seed ^ ((uint32_t)kind * 0x51ED27u));
            float x = ((float)cx + 0.1f + 0.8f * mtn_hash_unit(h)) * cs;
            float z = ((float)cz + 0.1f + 0.8f * mtn_hash_unit(mtn_hash_u32(h + 1u))) * cs;
            if (v3_dist_xz(v3(x, 0, z), t->center) > limit) continue;
            float total = 0.0f;
            for (int i = 0; i < f->species_count; i++) {
                const mtn_plant_species *s = &f->species[i];
                suit[i] = 0.0f;
                if (s->kind != kind) continue;
                suit[i] = s->density * mtn_flora_habitat(s, t, x, z) * patch(s, i, x, z, seed);
                total += suit[i];
            }
            if (total <= 0.0f) continue;
            float r = mtn_hash_unit(mtn_hash_u32(h + 2u));
            if (r >= mtn_minf(total, 1.0f)) continue;
            r *= total / mtn_minf(total, 1.0f);
            int chosen = -1;
            for (int i = 0; i < f->species_count; i++) {
                if (suit[i] <= 0.0f) continue;
                chosen = i;
                if (r < suit[i]) break;
                r -= suit[i];
            }
            const mtn_plant_species *s = &f->species[chosen];
            mtn_plant p;
            p.pos = v3(x, mtn_terrain_height(t, x, z), z);
            p.scale = mtn_lerpf(s->height_min, s->height_max, mtn_hash_unit(mtn_hash_u32(h + 3u)));
            p.rot = mtn_hash_unit(mtn_hash_u32(h + 4u)) * MTN_TAU;
            p.species = (uint16_t)chosen;
            p.hidden = 0;
            p.variant = (uint8_t)(mtn_hash_u32(h + 5u) & 255u);
            add_plant(f, &p);
        }
}

static int g_chunks_side;
static float chunk_key(const mtn_plant *p) {
    int cx = mtn_clampi((int)floorf(p->pos.x / MTN_FLORA_CHUNK), 0, g_chunks_side - 1);
    int cz = mtn_clampi((int)floorf(p->pos.z / MTN_FLORA_CHUNK), 0, g_chunks_side - 1);
    return (float)(cz * g_chunks_side + cx);
}

void mtn_flora_populate(mtn_flora *f, const mtn_terrain *t, uint32_t seed) {
    mtn_flora_free(f);
    f->seed = seed;
    f->chunks = (int)ceilf(t->world_size / MTN_FLORA_CHUNK);
    if (f->chunks < 1) f->chunks = 1;
    place_pass(f, t, MTN_PLANT_TREE, 8.0f, seed);
    place_pass(f, t, MTN_PLANT_BUSH, 11.0f, seed + 1u);
    place_pass(f, t, MTN_PLANT_BOULDER, 17.0f, seed + 2u);
    /* counting sort by chunk */
    int nch = f->chunks * f->chunks;
    f->chunk_start = calloc((size_t)nch, sizeof(int));
    f->chunk_count = calloc((size_t)nch, sizeof(int));
    f->chunk_version = calloc((size_t)nch, sizeof(int));
    g_chunks_side = f->chunks;
    for (int i = 0; i < f->count; i++) f->chunk_count[(int)chunk_key(&f->plants[i])]++;
    int acc = 0;
    for (int c = 0; c < nch; c++) {
        f->chunk_start[c] = acc;
        acc += f->chunk_count[c];
    }
    mtn_plant *sorted = malloc(sizeof(mtn_plant) * (size_t)(f->count > 0 ? f->count : 1));
    int *fill = calloc((size_t)nch, sizeof(int));
    for (int i = 0; i < f->count; i++) {
        int c = (int)chunk_key(&f->plants[i]);
        sorted[f->chunk_start[c] + fill[c]++] = f->plants[i];
    }
    free(fill);
    free(f->plants);
    f->plants = sorted;
    f->cap = f->count;
}

void mtn_flora_season_eval(const mtn_flora *f, mtn_season_blend s, float snow, mtn_flora_season *out) {
    for (int i = 0; i < f->species_count; i++) {
        const mtn_plant_species *sp = &f->species[i];
        vec3 c = v3(0, 0, 0);
        for (int k = 0; k < MTN_SEASON_COUNT; k++) c = v3_add(c, v3_scale(sp->leaf_color[k], s.w[k]));
        out->leaf_color[i] = c;
        float amount = 1.0f;
        if (sp->kind == MTN_PLANT_GROUND) {
            if (sp->bloom_mask) {
                amount = 0.0f;
                for (int k = 0; k < MTN_SEASON_COUNT; k++)
                    if (sp->bloom_mask & (1 << k)) amount += s.w[k];
            }
            amount *= 1.0f - 0.85f * snow;
        } else if (sp->deciduous) {
            amount = 1.0f - mtn_smoothstep(0.35f, 0.85f, s.w[MTN_WINTER]);
        }
        out->leaf_amount[i] = mtn_clampf(amount, 0.0f, 1.0f);
        out->blossom[i] = sp->blossom_tile >= 0 ? mtn_smoothstep(0.4f, 0.9f, s.w[MTN_SPRING]) : 0.0f;
    }
}

int mtn_flora_ground_cell(const mtn_flora *f, const mtn_terrain *t, int cx, int cz, mtn_ground_item *out, int max) {
    const float cs = 8.0f;
    int n = 0;
    uint32_t seed = f->seed ^ 0xA5A5u;
    for (int k = 0; k < 28 && n < max; k++) {
        uint32_t h = mtn_hash3i(cx, cz, k, seed);
        float x = ((float)cx + mtn_hash_unit(h)) * cs;
        float z = ((float)cz + mtn_hash_unit(mtn_hash_u32(h + 1u))) * cs;
        float total = 0.0f, suit[MTN_MAX_SPECIES];
        for (int i = 0; i < f->species_count; i++) {
            suit[i] = 0.0f;
            if (f->species[i].kind != MTN_PLANT_GROUND) continue;
            suit[i] = f->species[i].density * mtn_flora_habitat(&f->species[i], t, x, z) *
                      patch(&f->species[i], i, x, z, f->seed);
            total += suit[i];
        }
        float r = mtn_hash_unit(mtn_hash_u32(h + 2u));
        if (total <= 0.0f || r >= mtn_minf(total, 1.0f)) continue;
        r *= total / mtn_minf(total, 1.0f);
        int chosen = -1;
        for (int i = 0; i < f->species_count; i++) {
            if (suit[i] <= 0.0f) continue;
            chosen = i;
            if (r < suit[i]) break;
            r -= suit[i];
        }
        const mtn_plant_species *s = &f->species[chosen];
        memset(&out[n], 0, sizeof out[n]);
        out[n].pos = v3(x, mtn_terrain_height(t, x, z), z);
        out[n].size = mtn_lerpf(s->height_min, s->height_max, mtn_hash_unit(mtn_hash_u32(h + 3u)));
        out[n].species = (uint8_t)chosen;
        out[n].variant = (uint8_t)(mtn_hash_u32(h + 4u) & 255u);
        n++;
    }
    return n;
}

/* ------------------------------------------------------------------ meshes */

static mtn_material leaf_mat(const mtn_plant_species *s, int si, float brightness, int variant) {
    int tile = s->leaf_tiles[variant % s->leaf_tile_count];
    int b = (int)(mtn_clampf(brightness, 0.0f, 1.0f) * 255.0f);
    mtn_material m = mtn_mat(mtn_rgb(b, b, b), tile, MTN_VF_LEAF | MTN_VF_WIND | MTN_VF_SNOW, 1.0f);
    m.extra = (uint8_t)si;
    return m;
}

static mtn_material bark_mat(const mtn_plant_species *s) {
    return mtn_mat(mtn_rgb((int)(s->bark_color.x * 255), (int)(s->bark_color.y * 255), (int)(s->bark_color.z * 255)),
                   s->bark_tile, 0, 0.8f);
}

void mtn_tree_build(const mtn_flora *f, const mtn_plant *p, int lod, mtn_mesh *solid, mtn_mesh *leaves) {
    const mtn_plant_species *s = &f->species[p->species];
    int si = p->species;
    mtn_rng r;
    mtn_rng_seed(&r, mtn_hash2i((int)(p->pos.x * 10), (int)(p->pos.z * 10), 5), p->variant);
    float h = p->scale;
    vec3 base = p->pos;
    mat4 id = m4_identity();

    if (s->kind == MTN_PLANT_BOULDER) {
        if (lod >= 2) return;
        mtn_material m = bark_mat(s);
        m.tile = NO_TEXTURE;
        m.flags = MTN_VF_SNOW;
        mat4 xf = m4_mul(m4_translate(v3(base.x, base.y + h * 0.15f, base.z)), m4_rotate_y(p->rot));
        mtn_mesh_rock(solid, xf, h, p->variant * 31u + 7u, &m);
        return;
    }
    if (s->kind == MTN_PLANT_BUSH || s->kind == MTN_PLANT_GROUND) {
        int n = lod == 0 ? 5 : (lod == 1 ? 2 : 1);
        for (int i = 0; i < n; i++) {
            float a = mtn_rng_range(&r, 0, MTN_TAU), d = mtn_rng_range(&r, 0, h * 0.35f);
            vec3 c = v3(base.x + cosf(a) * d, base.y + h * mtn_rng_range(&r, 0.35f, 0.6f), base.z + sinf(a) * d);
            mtn_material m = leaf_mat(s, si, mtn_rng_range(&r, 0.7f, 1.0f), i + p->variant);
            float sz = h * (n == 1 ? 1.4f : 0.9f);
            mtn_mesh_billboard(leaves, c, sz, sz, 0.0f, &m, 0.2f);
        }
        return;
    }

    float trunk_r = s->trunk_radius * h / 10.0f;
    float cw = s->crown_width * h;
    mtn_material bark = bark_mat(s);

    if (lod >= 2) {
        /* far: one or two upright sprites, no trunk */
        mtn_material m = leaf_mat(s, si, 0.85f, p->variant);
        m.flags |= MTN_VF_UPRIGHT;
        if (s->crown == MTN_CROWN_CONE) {
            mtn_mesh_billboard(leaves, v3(base.x, base.y + h * 0.05f, base.z), cw * 1.7f, h * 0.65f, 1.0f, &m, 0.1f);
            mtn_mesh_billboard(leaves, v3(base.x, base.y + h * 0.55f, base.z), cw * 1.0f, h * 0.5f, 1.0f, &m, 0.2f);
        } else {
            mtn_mesh_billboard(leaves, v3(base.x, base.y + h * 0.25f, base.z), cw * 2.2f, h * 0.8f, 1.0f, &m, 0.2f);
        }
        return;
    }

    int sides = lod == 0 ? 6 : 4;
    if (s->crown == MTN_CROWN_CONE) {
        mtn_mesh_cylinder(solid, id, v3(base.x, base.y - 0.4f, base.z), v3(base.x, base.y + h * 0.92f, base.z),
                          trunk_r, trunk_r * 0.15f, sides, &bark, 0);
        int layers = lod == 0 ? 5 + (int)(h / 4.0f) : 3;
        for (int k = 0; k < layers; k++) {
            float t = layers > 1 ? (float)k / (float)(layers - 1) : 0.0f;
            float y = h * (0.20f + 0.74f * t);
            float rad = cw * (1.0f - t * 0.85f);
            int m = lod == 0 ? (rad > 1.2f ? 4 : 3) : 2;
            float a0 = mtn_rng_range(&r, 0, MTN_TAU);
            for (int j = 0; j < m; j++) {
                float a = a0 + (float)j * MTN_TAU / (float)m;
                vec3 c = v3(base.x + cosf(a) * rad * 0.5f, base.y + y, base.z + sinf(a) * rad * 0.5f);
                mtn_material lm = leaf_mat(s, si, (0.55f + 0.4f * t) * mtn_rng_range(&r, 0.85f, 1.1f), j + k);
                float sz = (lod == 0 ? rad * 1.35f : rad * 1.9f) + 0.6f;
                mtn_mesh_billboard(leaves, c, sz * 1.15f, sz * 0.85f, 0.0f, &lm, 0.15f + 0.6f * t);
            }
        }
        mtn_material top = leaf_mat(s, si, 1.0f, p->variant);
        mtn_mesh_billboard(leaves, v3(base.x, base.y + h * 0.97f, base.z), cw * 0.45f + 0.5f, h * 0.18f + 0.5f, 0.0f,
                           &top, 0.8f);
        return;
    }

    float trunk_top = s->crown == MTN_CROWN_UMBRELLA ? 0.85f : (s->crown == MTN_CROWN_OVAL ? 0.7f : 0.55f);
    vec3 tt = v3(base.x, base.y + h * trunk_top, base.z);
    mtn_mesh_cylinder(solid, id, v3(base.x, base.y - 0.4f, base.z), tt, trunk_r, trunk_r * 0.55f, sides, &bark, 0);
    vec3 cc;
    vec3 rad;
    if (s->crown == MTN_CROWN_UMBRELLA) {
        cc = v3(base.x, base.y + h * 0.88f, base.z);
        rad = v3(cw * 1.1f, h * 0.12f, cw * 1.1f);
    } else if (s->crown == MTN_CROWN_OVAL) {
        cc = v3(base.x, base.y + h * 0.62f, base.z);
        rad = v3(cw * 0.75f, h * 0.36f, cw * 0.75f);
    } else {
        cc = v3(base.x, base.y + h * 0.64f, base.z);
        rad = v3(cw, h * 0.32f, cw);
    }
    if (lod == 0) {
        int nb = 3 + (int)(mtn_rng_u32(&r) % 3u);
        for (int b = 0; b < nb; b++) {
            float a = mtn_rng_range(&r, 0, MTN_TAU);
            vec3 from = v3(tt.x, tt.y - h * 0.08f * (float)b, tt.z);
            vec3 to = v3(cc.x + cosf(a) * rad.x * 0.75f, cc.y + rad.y * mtn_rng_range(&r, -0.1f, 0.5f),
                         cc.z + sinf(a) * rad.z * 0.75f);
            mtn_mesh_cylinder(solid, id, from, to, trunk_r * 0.4f, trunk_r * 0.12f, 4, &bark, 0);
        }
    }
    int n = lod == 0 ? 12 + (int)(h * 0.8f) : 5;
    for (int i = 0; i < n; i++) {
        float u = mtn_rng_range(&r, -1, 1), v = mtn_rng_range(&r, -1, 1), w = mtn_rng_range(&r, -1, 1);
        float l = sqrtf(u * u + v * v + w * w);
        if (l > 1.0f) { u /= l; v /= l; w /= l; }
        float sh = lod == 0 ? 0.8f : 0.55f;
        vec3 c = v3(cc.x + u * rad.x * sh, cc.y + v * rad.y * sh, cc.z + w * rad.z * sh);
        float bright = 0.55f + 0.3f * (v * 0.5f + 0.5f) + 0.15f * mtn_rng_float(&r);
        mtn_material lm = leaf_mat(s, si, bright, i + p->variant);
        float sz = (lod == 0 ? 0.62f : 1.05f) * (rad.x + rad.y) + 0.4f;
        mtn_mesh_billboard(leaves, c, sz, sz * 0.9f, 0.0f, &lm, 0.25f + 0.5f * (v * 0.5f + 0.5f));
        if (s->blossom_tile >= 0 && (i % 2) == 0) {
            mtn_material bm = mtn_mat(mtn_rgb(255, 255, 255), s->blossom_tile, MTN_VF_BLOSSOM | MTN_VF_WIND, 1.0f);
            bm.extra = (uint8_t)si;
            vec3 bc = v3_add(c, v3(0, sz * 0.15f, 0));
            mtn_mesh_billboard(leaves, bc, sz * 0.7f, sz * 0.6f, 0.0f, &bm, 0.3f);
        }
    }
}

void mtn_flora_chunk_build(const mtn_flora *f, int chunk, int lod, mtn_mesh *solid, mtn_mesh *leaves) {
    if (chunk < 0 || chunk >= f->chunks * f->chunks) return;
    int s = f->chunk_start[chunk], e = s + f->chunk_count[chunk];
    for (int i = s; i < e; i++)
        if (!f->plants[i].hidden) mtn_tree_build(f, &f->plants[i], lod, solid, leaves);
}

void mtn_ground_build(const mtn_flora *f, const mtn_ground_item *items, int n, mtn_mesh *leaves) {
    for (int i = 0; i < n; i++) {
        const mtn_ground_item *it = &items[i];
        const mtn_plant_species *s = &f->species[it->species];
        int tile = s->leaf_tiles[it->variant % s->leaf_tile_count];
        int b = 200 + (it->variant % 56);
        int flags = MTN_VF_WIND | MTN_VF_UPRIGHT | MTN_VF_LEAF;
        mtn_material m = mtn_mat(mtn_rgb(b, b, b), tile, flags, 1.0f);
        m.extra = it->species;
        mtn_mesh_billboard(leaves, it->pos, it->size * 1.3f, it->size, 1.0f, &m, 0.35f);
    }
}

/* ------------------------------------------------------------------ interaction */

int mtn_flora_hide_radius(mtn_flora *f, vec3 c, float r, int keep) {
    int hidden = 0;
    for (int i = 0; i < f->count; i++) {
        mtn_plant *p = &f->plants[i];
        if (i == keep || p->hidden) continue;
        float pr = f->species[p->species].kind == MTN_PLANT_TREE ? 0.0f : p->scale * 0.5f;
        if (v3_dist_xz(p->pos, c) < r + pr) {
            p->hidden = 1;
            f->chunk_version[mtn_flora_chunk_of(f, p->pos.x, p->pos.z)]++;
            hidden++;
        }
    }
    return hidden;
}

void mtn_flora_unhide_all(mtn_flora *f) {
    for (int i = 0; i < f->count; i++) {
        if (f->plants[i].hidden) {
            f->plants[i].hidden = 0;
            f->chunk_version[mtn_flora_chunk_of(f, f->plants[i].pos.x, f->plants[i].pos.z)]++;
        }
    }
}

int mtn_flora_nearest_tree(const mtn_flora *f, vec3 p, float max_dist, float min_height) {
    int best = -1;
    float bd = max_dist;
    for (int i = 0; i < f->count; i++) {
        const mtn_plant *q = &f->plants[i];
        if (q->hidden || f->species[q->species].kind != MTN_PLANT_TREE || q->scale < min_height) continue;
        float d = v3_dist_xz(q->pos, p);
        if (d < bd) {
            bd = d;
            best = i;
        }
    }
    return best;
}

int mtn_flora_collide(const mtn_flora *f, vec3 *pos, float radius) {
    if (f->chunks == 0) return 0;
    int hit = 0;
    int c = mtn_flora_chunk_of(f, pos->x, pos->z);
    int ccx = c % f->chunks, ccz = c / f->chunks;
    for (int dz = -1; dz <= 1; dz++)
        for (int dx = -1; dx <= 1; dx++) {
            int x = ccx + dx, z = ccz + dz;
            if (x < 0 || z < 0 || x >= f->chunks || z >= f->chunks) continue;
            int ch = z * f->chunks + x;
            for (int i = f->chunk_start[ch]; i < f->chunk_start[ch] + f->chunk_count[ch]; i++) {
                const mtn_plant *p = &f->plants[i];
                if (p->hidden) continue;
                const mtn_plant_species *s = &f->species[p->species];
                float r;
                if (s->kind == MTN_PLANT_TREE) r = s->trunk_radius * p->scale / 10.0f + 0.1f;
                else if (s->kind == MTN_PLANT_BOULDER) r = p->scale * 0.85f;
                else continue;
                if (pos->y > p->pos.y + (s->kind == MTN_PLANT_BOULDER ? p->scale * 0.9f : p->scale)) continue;
                float d = v3_dist_xz(*pos, p->pos);
                float min_d = r + radius;
                if (d < min_d) {
                    vec3 dir = d > 1e-4f ? v3_scale(v3(pos->x - p->pos.x, 0, pos->z - p->pos.z), 1.0f / d) : v3(1, 0, 0);
                    pos->x = p->pos.x + dir.x * min_d;
                    pos->z = p->pos.z + dir.z * min_d;
                    hit = 1;
                }
            }
        }
    return hit;
}
