/* Mountain engine - procedural mountain terrain: shape, erosion, lakes,
 * moisture, biomes, hiking trail and scenic viewpoints. MIT License. */
#include "mtn/terrain.h"
#include "mtn/rng.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void mtn_terrain_default_params(mtn_terrain_params *p) {
    memset(p, 0, sizeof *p);
    p->seed = 1;
    p->size = 513;
    p->cell = 6.0f;
    p->peak_height = 950.0f;
    p->range_height = 420.0f;
    p->hill_height = 70.0f;
    p->ridge_strength = 0.8f;
    p->erosion_drops = 60000;
    p->lake_min_depth = 1.5f;
    p->lake_min_cells = 24;
    p->sea_level = MTN_NO_WATER;
    p->playable_radius = 0.82f;
    p->tree_line = 0.48f;
    p->snow_line = 0.80f;
}

float mtn_terrain_grid(const mtn_terrain *t, int x, int z) {
    x = mtn_clampi(x, 0, t->n - 1);
    z = mtn_clampi(z, 0, t->n - 1);
    return t->h[z * t->n + x];
}

static float grid_bilinear(const float *a, int n, float gx, float gz) {
    gx = mtn_clampf(gx, 0.0f, (float)(n - 1) - 0.0001f);
    gz = mtn_clampf(gz, 0.0f, (float)(n - 1) - 0.0001f);
    int x0 = (int)gx, z0 = (int)gz;
    float fx = gx - (float)x0, fz = gz - (float)z0;
    const float *r0 = a + z0 * n + x0;
    const float *r1 = r0 + n;
    float a0 = r0[0] + (r0[1] - r0[0]) * fx;
    float a1 = r1[0] + (r1[1] - r1[0]) * fx;
    return a0 + (a1 - a0) * fz;
}

float mtn_terrain_height(const mtn_terrain *t, float x, float z) {
    return grid_bilinear(t->h, t->n, x / t->cell, z / t->cell);
}

vec3 mtn_terrain_normal(const mtn_terrain *t, float x, float z) {
    float e = t->cell;
    float hl = mtn_terrain_height(t, x - e, z), hr = mtn_terrain_height(t, x + e, z);
    float hd = mtn_terrain_height(t, x, z - e), hu = mtn_terrain_height(t, x, z + e);
    return v3_norm(v3(hl - hr, 2.0f * e, hd - hu));
}

float mtn_terrain_slope(const mtn_terrain *t, float x, float z) {
    return 1.0f - mtn_terrain_normal(t, x, z).y;
}

static int grid_index(const mtn_terrain *t, float x, float z) {
    int gx = mtn_clampi((int)floorf(x / t->cell + 0.5f), 0, t->n - 1);
    int gz = mtn_clampi((int)floorf(z / t->cell + 0.5f), 0, t->n - 1);
    return gz * t->n + gx;
}

float mtn_terrain_water(const mtn_terrain *t, float x, float z) {
    int gx = (int)floorf(x / t->cell), gz = (int)floorf(z / t->cell);
    /* water if any surrounding corner is a lake; take the highest surface */
    float best = MTN_NO_WATER;
    for (int dz = 0; dz <= 1; dz++)
        for (int dx = 0; dx <= 1; dx++) {
            int ix = mtn_clampi(gx + dx, 0, t->n - 1), iz = mtn_clampi(gz + dz, 0, t->n - 1);
            float w = t->water[iz * t->n + ix];
            if (w > best) best = w;
        }
    if (best != MTN_NO_WATER && best <= mtn_terrain_height(t, x, z)) return MTN_NO_WATER;
    return best;
}

float mtn_terrain_altitude_frac(const mtn_terrain *t, float h) {
    float r = t->max_h - t->min_h;
    return r > 0 ? mtn_clampf((h - t->min_h) / r, 0.0f, 1.0f) : 0.0f;
}

mtn_biome mtn_terrain_biome(const mtn_terrain *t, float x, float z) {
    return (mtn_biome)t->biome[grid_index(t, x, z)];
}

float mtn_terrain_moisture(const mtn_terrain *t, float x, float z) {
    return grid_bilinear(t->moist, t->n, x / t->cell, z / t->cell);
}

float mtn_terrain_trail_distance(const mtn_terrain *t, float x, float z) {
    return grid_bilinear(t->trail_dist, t->n, x / t->cell, z / t->cell);
}

static float playable_r(const mtn_terrain *t) { return t->p.playable_radius * t->world_size * 0.5f; }

int mtn_terrain_in_playable(const mtn_terrain *t, float x, float z) {
    return v3_dist_xz(v3(x, 0, z), t->center) <= playable_r(t);
}

int mtn_terrain_clamp_playable(const mtn_terrain *t, vec3 *p) {
    float d = v3_dist_xz(*p, t->center);
    float r = playable_r(t);
    if (d <= r) return 0;
    float s = r / d;
    p->x = t->center.x + (p->x - t->center.x) * s;
    p->z = t->center.z + (p->z - t->center.z) * s;
    return 1;
}

int mtn_terrain_raycast(const mtn_terrain *t, vec3 o, vec3 d, float maxdist, vec3 *hit) {
    d = v3_norm(d);
    float step = t->cell * 0.5f;
    float prev = 0.0f;
    for (float s = 0.0f; s <= maxdist; s += step) {
        vec3 p = v3_add(o, v3_scale(d, s));
        if (p.y <= mtn_terrain_height(t, p.x, p.z)) {
            float lo = prev, hi = s;
            for (int i = 0; i < 16; i++) {
                float mid = 0.5f * (lo + hi);
                vec3 q = v3_add(o, v3_scale(d, mid));
                if (q.y <= mtn_terrain_height(t, q.x, q.z)) hi = mid; else lo = mid;
            }
            if (hit) *hit = v3_add(o, v3_scale(d, hi));
            return 1;
        }
        prev = s;
    }
    return 0;
}

const char *mtn_biome_name(mtn_biome b) {
    static const char *names[MTN_BIOME_COUNT] = {"water", "forest", "meadow", "alpine", "scree", "rock", "snow"};
    return (b >= 0 && b < MTN_BIOME_COUNT) ? names[b] : "unknown";
}

/* ------------------------------------------------------------------ shape */

static void generate_shape(mtn_terrain *t) {
    const mtn_terrain_params *p = &t->p;
    int n = t->n;
    uint32_t s = p->seed * 7919u + 17u;
    float half = t->world_size * 0.5f;
    /* Offset the main peak a little so every world looks different. */
    mtn_rng r;
    mtn_rng_seed(&r, p->seed, 3);
    float ox = mtn_rng_range(&r, -0.08f, 0.08f), oz = mtn_rng_range(&r, -0.08f, 0.08f);
    float stretch = mtn_rng_range(&r, 0.8f, 1.25f);
    float rot = mtn_rng_range(&r, 0.0f, MTN_TAU);
    float cr = cosf(rot), sr = sinf(rot);
    /* Outflow valleys cut through the surrounding ranges so that rain drains
     * away instead of forming a moat around the main peak. */
    int nvalleys = 2 + (int)(mtn_rng_u32(&r) % 2u);
    float va[3];
    va[0] = mtn_rng_range(&r, 0.0f, MTN_TAU);
    for (int k = 1; k < nvalleys; k++) va[k] = va[0] + (float)k * MTN_TAU / (float)nvalleys + mtn_rng_range(&r, -0.5f, 0.5f);
    float tdx = cosf(va[0]), tdz = sinf(va[0]);
    for (int z = 0; z < n; z++) {
        for (int x = 0; x < n; x++) {
            float u = ((float)x * t->cell - half) / half;
            float v = ((float)z * t->cell - half) / half;
            float wu = u + 0.18f * mtn_fbm2(u * 1.7f, v * 1.7f, s + 1, 4, 2.0f, 0.5f);
            float wv = v + 0.18f * mtn_fbm2(u * 1.7f + 5.2f, v * 1.7f + 1.3f, s + 2, 4, 2.0f, 0.5f);
            float ru = (wu - ox) * cr - (wv - oz) * sr;
            float rv = ((wu - ox) * sr + (wv - oz) * cr) * stretch;
            float d = sqrtf(ru * ru + rv * rv);
            float m = mtn_maxf(0.0f, 1.0f - d / 0.62f);
            float ridge = mtn_ridged2(wu * 2.4f + 3.1f, wv * 2.4f + 7.7f, s + 3, 6, 2.0f, 0.5f);
            float main_h = p->peak_height * (0.72f * powf(m, 1.55f) +
                                             0.42f * p->ridge_strength * ridge * powf(m, 1.05f));
            float ang = atan2f(wv, wu);
            float vmask = 1.0f;
            for (int k = 0; k < nvalleys; k++) {
                float da = fabsf(mtn_wrap_angle(ang - va[k]));
                vmask = mtn_minf(vmask, mtn_smoothstep(0.10f, 0.55f, da));
            }
            float dd = sqrtf(wu * wu + wv * wv);
            float e = mtn_smoothstep(0.5f, 1.1f, dd);
            float range = p->range_height * e * (0.1f + 0.9f * vmask) *
                          (0.35f + mtn_ridged2(wu * 3.1f + 11.0f, wv * 3.1f + 2.0f, s + 4, 5, 2.0f, 0.5f));
            float tilt = -(wu * tdx + wv * tdz) * 0.12f * p->peak_height;
            float carve = (1.0f - vmask) * mtn_smoothstep(0.3f, 1.2f, dd) * 0.2f * p->peak_height;
            float hills = p->hill_height * (mtn_fbm2(u * 4.0f, v * 4.0f, s + 5, 5, 2.0f, 0.5f) * 0.5f + 0.5f);
            float detail = 6.0f * mtn_fbm2(u * 24.0f, v * 24.0f, s + 6, 3, 2.0f, 0.5f);
            t->h[z * n + x] = main_h + range + hills + detail + tilt - carve + 0.25f * p->peak_height;
        }
    }
}

/* ------------------------------------------------------------------ erosion */

void mtn_terrain_erode(float *h, int n, float cell, int drops, uint32_t seed) {
    if (drops <= 0 || n < 8) return;
    const float inertia = 0.05f, capacity_k = 4.0f, min_cap = 0.01f, erode_k = 0.3f, deposit_k = 0.3f;
    const float evaporate = 0.02f, gravity = 4.0f;
    const int max_life = 40, radius = 2;
    /* brush offsets/weights */
    int bx[32], bz[32];
    float bw[32];
    int bc = 0;
    float wsum = 0.0f;
    for (int dz = -radius; dz <= radius; dz++)
        for (int dx = -radius; dx <= radius; dx++) {
            float d = sqrtf((float)(dx * dx + dz * dz));
            if (d <= (float)radius) {
                bx[bc] = dx; bz[bc] = dz; bw[bc] = (float)radius - d + 0.5f;
                wsum += bw[bc];
                bc++;
            }
        }
    for (int i = 0; i < bc; i++) bw[i] /= wsum;
    float inv_cell = 1.0f / cell;
    for (int i = 0; i < n * n; i++) h[i] *= inv_cell; /* work in cell units */

    mtn_rng r;
    mtn_rng_seed(&r, seed, 11);
    for (int d = 0; d < drops; d++) {
        float px = mtn_rng_range(&r, 2.0f, (float)n - 3.0f);
        float pz = mtn_rng_range(&r, 2.0f, (float)n - 3.0f);
        float dx = 0, dz = 0, speed = 1.0f, water = 1.0f, sed = 0.0f;
        for (int life = 0; life < max_life; life++) {
            int nx = (int)px, nz = (int)pz;
            float fx = px - (float)nx, fz = pz - (float)nz;
            int idx = nz * n + nx;
            float h00 = h[idx], h10 = h[idx + 1], h01 = h[idx + n], h11 = h[idx + n + 1];
            float gx = (h10 - h00) * (1 - fz) + (h11 - h01) * fz;
            float gz = (h01 - h00) * (1 - fx) + (h11 - h10) * fx;
            float hcur = h00 * (1 - fx) * (1 - fz) + h10 * fx * (1 - fz) + h01 * (1 - fx) * fz + h11 * fx * fz;
            dx = dx * inertia - gx * (1 - inertia);
            dz = dz * inertia - gz * (1 - inertia);
            float len = sqrtf(dx * dx + dz * dz);
            if (len < 1e-6f) break;
            dx /= len; dz /= len;
            px += dx; pz += dz;
            if (px < 2.0f || pz < 2.0f || px >= (float)n - 3.0f || pz >= (float)n - 3.0f) break;
            int mx = (int)px, mz = (int)pz;
            float gx2 = px - (float)mx, gz2 = pz - (float)mz;
            int j = mz * n + mx;
            float hnew = h[j] * (1 - gx2) * (1 - gz2) + h[j + 1] * gx2 * (1 - gz2) + h[j + n] * (1 - gx2) * gz2 +
                         h[j + n + 1] * gx2 * gz2;
            float dh = hnew - hcur;
            float cap = mtn_maxf(-dh * speed * water * capacity_k, min_cap);
            if (sed > cap || dh > 0) {
                float dep = dh > 0 ? mtn_minf(dh, sed) : (sed - cap) * deposit_k;
                sed -= dep;
                h[idx] += dep * (1 - fx) * (1 - fz);
                h[idx + 1] += dep * fx * (1 - fz);
                h[idx + n] += dep * (1 - fx) * fz;
                h[idx + n + 1] += dep * fx * fz;
            } else {
                float ero = mtn_minf((cap - sed) * erode_k, -dh);
                for (int b = 0; b < bc; b++) {
                    int ex = nx + bx[b], ez = nz + bz[b];
                    if (ex < 0 || ez < 0 || ex >= n || ez >= n) continue;
                    float amt = ero * bw[b];
                    h[ez * n + ex] -= amt;
                }
                sed += ero;
            }
            speed = sqrtf(mtn_maxf(0.0f, speed * speed + (-dh) * gravity));
            water *= (1.0f - evaporate);
        }
    }
    for (int i = 0; i < n * n; i++) h[i] *= cell;
}

/* ------------------------------------------------------------------ lakes */

typedef struct { float h; int i; } heap_item;
typedef struct { heap_item *a; int n, cap; } heap;

static void heap_push(heap *hp, float h, int i) {
    if (hp->n == hp->cap) {
        hp->cap = hp->cap ? hp->cap * 2 : 1024;
        hp->a = realloc(hp->a, sizeof(heap_item) * (size_t)hp->cap);
    }
    int k = hp->n++;
    while (k > 0) {
        int parent = (k - 1) / 2;
        if (hp->a[parent].h <= h) break;
        hp->a[k] = hp->a[parent];
        k = parent;
    }
    hp->a[k].h = h;
    hp->a[k].i = i;
}

static heap_item heap_pop(heap *hp) {
    heap_item top = hp->a[0];
    heap_item last = hp->a[--hp->n];
    int k = 0;
    for (;;) {
        int c = 2 * k + 1;
        if (c >= hp->n) break;
        if (c + 1 < hp->n && hp->a[c + 1].h < hp->a[c].h) c++;
        if (hp->a[c].h >= last.h) break;
        hp->a[k] = hp->a[c];
        k = c;
    }
    if (hp->n > 0) hp->a[k] = last;
    return top;
}

static const int NB8X[8] = {1, -1, 0, 0, 1, 1, -1, -1};
static const int NB8Z[8] = {0, 0, 1, -1, 1, -1, 1, -1};

/* Priority-flood depression filling; returns filled surface (caller frees). */
static float *priority_flood(const float *h, int n, float eps) {
    float *f = malloc(sizeof(float) * (size_t)n * (size_t)n);
    uint8_t *closed = calloc((size_t)n * (size_t)n, 1);
    heap hp = {0};
    memcpy(f, h, sizeof(float) * (size_t)n * (size_t)n);
    for (int i = 0; i < n; i++) {
        int e[4] = {i, (n - 1) * n + i, i * n, i * n + n - 1};
        for (int k = 0; k < 4; k++)
            if (!closed[e[k]]) {
                closed[e[k]] = 1;
                heap_push(&hp, f[e[k]], e[k]);
            }
    }
    while (hp.n > 0) {
        heap_item c = heap_pop(&hp);
        int cx = c.i % n, cz = c.i / n;
        for (int k = 0; k < 8; k++) {
            int x = cx + NB8X[k], z = cz + NB8Z[k];
            if (x < 0 || z < 0 || x >= n || z >= n) continue;
            int j = z * n + x;
            if (closed[j]) continue;
            closed[j] = 1;
            if (f[j] <= f[c.i] + eps) f[j] = f[c.i] + eps;
            heap_push(&hp, f[j], j);
        }
    }
    free(hp.a);
    free(closed);
    return f;
}

int mtn_terrain_fill_lakes(mtn_terrain *t) {
    int n = t->n;
    size_t nn = (size_t)n * (size_t)n;
    float *f = priority_flood(t->h, n, 0.0f);
    int *stack = malloc(sizeof(int) * nn);
    int *comp = malloc(sizeof(int) * nn);
    uint8_t *seen = calloc(nn, 1);
    int lakes = 0;
    for (size_t i = 0; i < nn; i++) t->water[i] = MTN_NO_WATER;
    if (t->p.sea_level != MTN_NO_WATER) {
        for (size_t i = 0; i < nn; i++)
            if (t->h[i] < t->p.sea_level) t->water[i] = t->p.sea_level;
    }
    for (size_t i = 0; i < nn; i++) {
        if (seen[i] || f[i] - t->h[i] < 0.05f) continue;
        int sp = 0, cc = 0;
        float maxd = 0.0f;
        stack[sp++] = (int)i;
        seen[i] = 1;
        while (sp > 0) {
            int c = stack[--sp];
            comp[cc++] = c;
            float d = f[c] - t->h[c];
            if (d > maxd) maxd = d;
            int cx = c % n, cz = c / n;
            for (int k = 0; k < 4; k++) {
                int x = cx + NB8X[k], z = cz + NB8Z[k];
                if (x < 0 || z < 0 || x >= n || z >= n) continue;
                int j = z * n + x;
                if (seen[j] || f[j] - t->h[j] < 0.05f) continue;
                seen[j] = 1;
                stack[sp++] = j;
            }
        }
        if (maxd >= t->p.lake_min_depth && cc >= t->p.lake_min_cells) {
            lakes++;
            for (int k = 0; k < cc; k++) t->water[comp[k]] = f[comp[k]];
        }
    }
    free(seen);
    free(comp);
    free(stack);
    free(f);
    t->lake_count = lakes;
    return lakes;
}

/* ------------------------------------------------------------------ moisture */

static int cmp_desc(const void *a, const void *b, const float *h) {
    float ha = h[*(const int *)a], hb = h[*(const int *)b];
    return (ha < hb) - (ha > hb);
}
static const float *g_sort_h;
static int cmp_desc_q(const void *a, const void *b) { return cmp_desc(a, b, g_sort_h); }

static void compute_moisture(mtn_terrain *t) {
    int n = t->n;
    size_t nn = (size_t)n * (size_t)n;
    float *f = priority_flood(t->h, n, 0.01f);
    int *order = malloc(sizeof(int) * nn);
    float *acc = malloc(sizeof(float) * nn);
    for (size_t i = 0; i < nn; i++) {
        order[i] = (int)i;
        acc[i] = 1.0f;
    }
    g_sort_h = f;
    qsort(order, nn, sizeof(int), cmp_desc_q);
    for (size_t k = 0; k < nn; k++) {
        int c = order[k];
        int cx = c % n, cz = c / n, best = -1;
        float bh = f[c];
        for (int d = 0; d < 8; d++) {
            int x = cx + NB8X[d], z = cz + NB8Z[d];
            if (x < 0 || z < 0 || x >= n || z >= n) continue;
            int j = z * n + x;
            if (f[j] < bh) {
                bh = f[j];
                best = j;
            }
        }
        if (best >= 0) acc[best] += acc[c];
    }
    float lmax = logf(1.0f + (float)nn);
    uint32_t s = t->p.seed * 31u + 7u;
    for (size_t i = 0; i < nn; i++) {
        float a = logf(1.0f + acc[i]) / lmax;
        int x = (int)(i % (size_t)n), z = (int)(i / (size_t)n);
        float noise = mtn_fbm2((float)x * 0.02f, (float)z * 0.02f, s, 3, 2.0f, 0.5f);
        float m = a * 1.8f + 0.25f + 0.35f * noise;
        if (t->water[i] != MTN_NO_WATER) m = 1.0f;
        t->moist[i] = mtn_clampf(m, 0.0f, 1.0f);
    }
    /* soften: lakes moisten their shores */
    for (int pass = 0; pass < 2; pass++)
        for (int z = 1; z < n - 1; z++)
            for (int x = 1; x < n - 1; x++) {
                int i = z * n + x;
                float m = t->moist[i];
                m = mtn_maxf(m, 0.92f * mtn_maxf(mtn_maxf(t->moist[i - 1], t->moist[i + 1]),
                                                  mtn_maxf(t->moist[i - n], t->moist[i + n])));
                t->moist[i] = m;
            }
    free(acc);
    free(order);
    free(f);
}

/* ------------------------------------------------------------------ biomes */

static void compute_biomes(mtn_terrain *t) {
    int n = t->n;
    uint32_t s = t->p.seed * 131u + 3u;
    for (int z = 0; z < n; z++)
        for (int x = 0; x < n; x++) {
            int i = z * n + x;
            float wx = (float)x * t->cell, wz = (float)z * t->cell;
            float a = mtn_terrain_altitude_frac(t, t->h[i]);
            float sl = mtn_terrain_slope(t, wx, wz);
            float nz = mtn_fbm2(wx * 0.004f, wz * 0.004f, s, 3, 2.0f, 0.5f);
            float m = t->moist[i];
            mtn_biome b;
            if (t->water[i] != MTN_NO_WATER && t->water[i] > t->h[i]) b = MTN_BIOME_WATER;
            else if (sl > 0.50f) b = MTN_BIOME_ROCK;
            else if (a > t->p.snow_line + nz * 0.06f && sl < 0.45f) b = MTN_BIOME_SNOW;
            else if (sl > 0.36f || a > t->p.snow_line - 0.10f + nz * 0.05f) b = MTN_BIOME_SCREE;
            else if (a > t->p.tree_line + nz * 0.07f) b = MTN_BIOME_ALPINE;
            else if (nz > 0.30f || m > 0.90f) b = MTN_BIOME_MEADOW;
            else b = MTN_BIOME_FOREST;
            t->biome[i] = (uint8_t)b;
        }
}

/* ------------------------------------------------------------------ trail */

static const int NB16X[16] = {1, -1, 0, 0, 1, 1, -1, -1, 2, 2, -2, -2, 1, -1, 1, -1};
static const int NB16Z[16] = {0, 0, 1, -1, 1, -1, 1, -1, 1, -1, 1, -1, 2, 2, -2, -2};

static void stamp_trail_distance(mtn_terrain *t) {
    int n = t->n;
    const float cap = 40.0f;
    for (int i = 0; i < n * n; i++) t->trail_dist[i] = cap;
    int rad = (int)ceilf(cap / t->cell);
    for (int k = 0; k < t->trail_len; k++) {
        vec3 p = t->trail[k];
        int cx = (int)floorf(p.x / t->cell + 0.5f), cz = (int)floorf(p.z / t->cell + 0.5f);
        for (int z = cz - rad; z <= cz + rad; z++)
            for (int x = cx - rad; x <= cx + rad; x++) {
                if (x < 0 || z < 0 || x >= n || z >= n) continue;
                float d = v3_dist_xz(p, v3((float)x * t->cell, 0, (float)z * t->cell));
                if (d < t->trail_dist[z * n + x]) t->trail_dist[z * n + x] = d;
            }
    }
}

/* Spatial index of the trail: every grid cell lists the segments that pass within
 * TRAIL_REACH of it (compressed rows: start[cell] .. start[cell + 1]). */
#define TRAIL_REACH (MTN_TRAIL_WIDTH * 0.5f + 1.5f)

static int trail_cells(const mtn_terrain *t, int k, int *x0, int *z0, int *x1, int *z1) {
    vec3 a = t->trail[k], b = t->trail[k + 1];
    *x0 = mtn_clampi((int)floorf((fminf(a.x, b.x) - TRAIL_REACH) / t->cell), 0, t->n - 1);
    *x1 = mtn_clampi((int)floorf((fmaxf(a.x, b.x) + TRAIL_REACH) / t->cell), 0, t->n - 1);
    *z0 = mtn_clampi((int)floorf((fminf(a.z, b.z) - TRAIL_REACH) / t->cell), 0, t->n - 1);
    *z1 = mtn_clampi((int)floorf((fmaxf(a.z, b.z) + TRAIL_REACH) / t->cell), 0, t->n - 1);
    return (*x1 - *x0 + 1) * (*z1 - *z0 + 1);
}

static void index_trail(mtn_terrain *t) {
    int n = t->n;
    size_t nc = (size_t)n * (size_t)n;
    free(t->trail_cell_start);
    free(t->trail_cell_items);
    t->trail_cell_start = calloc(nc + 1, sizeof(int));
    int x0, z0, x1, z1;
    for (int k = 0; k + 1 < t->trail_len; k++) {
        trail_cells(t, k, &x0, &z0, &x1, &z1);
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) t->trail_cell_start[z * n + x + 1]++;
    }
    for (size_t c = 0; c < nc; c++) t->trail_cell_start[c + 1] += t->trail_cell_start[c];
    int *fill = malloc(sizeof(int) * nc);
    memcpy(fill, t->trail_cell_start, sizeof(int) * nc);
    t->trail_cell_items = malloc(sizeof(int) * (size_t)(t->trail_cell_start[nc] + 1));
    for (int k = 0; k + 1 < t->trail_len; k++) {
        trail_cells(t, k, &x0, &z0, &x1, &z1);
        for (int z = z0; z <= z1; z++)
            for (int x = x0; x <= x1; x++) t->trail_cell_items[fill[z * n + x]++] = k;
    }
    free(fill);
}

int mtn_terrain_trail_nearest_level(const mtn_terrain *t, float x, float z, float ref_y, float tol, vec3 *closest) {
    if (!t->trail_cell_start || t->trail_len < 2) return 0;
    int cx = (int)floorf(x / t->cell), cz = (int)floorf(z / t->cell);
    if (cx < 0 || cz < 0 || cx >= t->n || cz >= t->n) return 0;
    int c = cz * t->n + cx;
    float best = 1e30f;
    vec3 q = v3(x, 0, z);
    for (int i = t->trail_cell_start[c]; i < t->trail_cell_start[c + 1]; i++) {
        int k = t->trail_cell_items[i];
        vec3 a = t->trail[k], b = t->trail[k + 1];
        vec3 ab = v3(b.x - a.x, 0, b.z - a.z);
        float l2 = ab.x * ab.x + ab.z * ab.z;
        float f = l2 > 1e-8f ? mtn_clampf(((x - a.x) * ab.x + (z - a.z) * ab.z) / l2, 0.0f, 1.0f) : 0.0f;
        vec3 p = v3_lerp(a, b, f);
        if (fabsf(p.y - ref_y) > tol) continue;
        float d = v3_dist_xz(p, q);
        if (d < best) {
            best = d;
            *closest = p;
        }
    }
    return best < 1e29f;
}

int mtn_terrain_trail_nearest(const mtn_terrain *t, float x, float z, vec3 *closest) {
    return mtn_terrain_trail_nearest_level(t, x, z, 0.0f, 1e30f, closest);
}

int mtn_terrain_trail_surface(const mtn_terrain *t, float x, float z, float ref_y, float *y) {
    if (!t->trail_cell_start || t->trail_len < 2) return 0;
    int cx = (int)floorf(x / t->cell), cz = (int)floorf(z / t->cell);
    if (cx < 0 || cz < 0 || cx >= t->n || cz >= t->n) return 0;
    int c = cz * t->n + cx, n = 0, ks[48];
    float hs[48], ws[48];
    /* every stretch of path within reach: its height and how close it runs */
    for (int i = t->trail_cell_start[c]; i < t->trail_cell_start[c + 1] && n < 48; i++) {
        int k = t->trail_cell_items[i];
        vec3 a = t->trail[k], b = t->trail[k + 1];
        vec3 ab = v3(b.x - a.x, 0, b.z - a.z);
        float l2 = ab.x * ab.x + ab.z * ab.z;
        float f = l2 > 1e-8f ? mtn_clampf(((x - a.x) * ab.x + (z - a.z) * ab.z) / l2, 0.0f, 1.0f) : 0.0f;
        vec3 p = v3_lerp(a, b, f);
        float d = v3_dist_xz(p, v3(x, 0, z));
        if (d > MTN_TRAIL_WALK_HALF) continue;
        hs[n] = p.y;
        ks[n] = k;
        float e = (MTN_TRAIL_WALK_HALF - d) * (MTN_TRAIL_WALK_HALF - d);
        ws[n] = e * e + 1e-9f;
        n++;
    }
    if (!n) return 0;
    /* the stretch nearest the walker's feet, blended with its neighbours along the path:
     * the surface has no steps at bends, and another leg of the path (crossing, or
     * passing above or below) does not count */
    int best = 0;
    for (int i = 1; i < n; i++)
        if (fabsf(hs[i] - ref_y) < fabsf(hs[best] - ref_y)) best = i;
    float sum = 0.0f, wsum = 0.0f;
    for (int i = 0; i < n; i++)
        if (abs(ks[i] - ks[best]) <= 8 && fabsf(hs[i] - hs[best]) < 1.0f) {
            sum += hs[i] * ws[i];
            wsum += ws[i];
        }
    *y = sum / wsum;
    return 1;
}

/* One leg of the trail: A* on the full grid with 16 neighbours. Steep grades are
 * expensive (so the route zigzags), a smooth noise field makes it meander, water
 * and leaving the playable area are avoided. Appends grid indices after start. */
typedef struct {
    float *g;
    int *from;
    uint8_t *closed;
    uint8_t *used; /* grid points next to the legs routed so far */
    int *path;
    int len, cap;
} trail_work;

static float meander(const mtn_terrain *t, float x, float z) {
    return 0.8f + 0.5f * (mtn_fbm2(x * 0.006f, z * 0.006f, t->p.seed * 17u + 5u, 3, 2.0f, 0.5f) * 0.5f + 0.5f);
}

static int route_leg(const mtn_terrain *t, trail_work *w, int start, int goal) {
    int n = t->n;
    size_t nn = (size_t)n * (size_t)n;
    for (size_t i = 0; i < nn; i++) {
        w->g[i] = 1e30f;
        w->from[i] = -1;
    }
    memset(w->closed, 0, nn);
    heap hp = {0};
    int gx = goal % n, gz = goal / n;
    w->g[start] = 0.0f;
    heap_push(&hp, 0.0f, start);
    while (hp.n > 0) {
        heap_item c = heap_pop(&hp);
        if (w->closed[c.i]) continue;
        w->closed[c.i] = 1;
        if (c.i == goal) break;
        int cx = c.i % n, cz = c.i / n;
        float ch = t->h[c.i];
        for (int k = 0; k < 16; k++) {
            int x = cx + NB16X[k], z = cz + NB16Z[k];
            if (x < 0 || z < 0 || x >= n || z >= n) continue;
            int j = z * n + x;
            if (w->closed[j]) continue;
            float wx = (float)x * t->cell, wz = (float)z * t->cell;
            float dist = t->cell * sqrtf((float)(NB16X[k] * NB16X[k] + NB16Z[k] * NB16Z[k]));
            float grade = fabsf(t->h[j] - ch) / dist;
            float cost = dist * meander(t, wx, wz) * (1.0f + 2.0f * grade * grade);
            if (grade > 0.25f) cost *= 1.0f + 80.0f * (grade - 0.25f);
            if (grade > 0.38f) cost *= 40.0f;
            if (t->water[j] != MTN_NO_WATER) cost *= 80.0f;
            if (!mtn_terrain_in_playable(t, wx, wz)) cost *= 20.0f;
            if (t->biome[j] == MTN_BIOME_ROCK) cost *= 1.5f;
            /* keep away from the earlier legs (no crossings), except where this leg starts */
            if (w->used[j] && abs(x - (start % n)) + abs(z - (start / n)) > 4) cost *= 6.0f;
            float ng = w->g[c.i] + cost;
            if (ng < w->g[j]) {
                w->g[j] = ng;
                w->from[j] = c.i;
                float dxg = (float)(x - gx) * t->cell, dzg = (float)(z - gz) * t->cell;
                heap_push(&hp, ng + 0.7f * sqrtf(dxg * dxg + dzg * dzg), j);
            }
        }
    }
    free(hp.a);
    if (start != goal && w->from[goal] < 0) return 0;
    int count = 0;
    for (int c = goal; c != start && c >= 0; c = w->from[c]) count++;
    if (w->len + count > w->cap) {
        w->cap = (w->len + count) * 2 + 64;
        w->path = realloc(w->path, sizeof(int) * (size_t)w->cap);
    }
    int k = w->len + count;
    for (int c = goal; c != start && c >= 0; c = w->from[c]) w->path[--k] = c;
    w->len += count;
    /* mark the new leg and its surroundings for the next legs */
    for (int i = w->len - count; i < w->len; i++) {
        int px = w->path[i] % n, pz = w->path[i] / n;
        for (int dz = -1; dz <= 1; dz++)
            for (int dx = -1; dx <= 1; dx++)
                if (px + dx >= 0 && pz + dz >= 0 && px + dx < n && pz + dz < n) w->used[(pz + dz) * n + px + dx] = 1;
    }
    return 1;
}

/* A waypoint on the ring at a given altitude fraction, near the requested angle around the summit. */
static int find_waypoint(const mtn_terrain *t, float angle, float level, int *out) {
    static const float tries[5] = {0.0f, 0.25f, -0.25f, 0.5f, -0.5f};
    float rmax = t->p.playable_radius * t->world_size * 0.5f;
    for (int k = 0; k < 5; k++) {
        float a = angle + tries[k];
        for (float r = t->cell * 2.0f; r < rmax; r += t->cell) {
            float x = t->summit.x + cosf(a) * r, z = t->summit.z + sinf(a) * r;
            if (!mtn_terrain_in_playable(t, x, z)) break;
            if (mtn_terrain_altitude_frac(t, mtn_terrain_height(t, x, z)) > level) continue;
            if (mtn_terrain_water(t, x, z) != MTN_NO_WATER || mtn_terrain_slope(t, x, z) > 0.3f) continue;
            int gx = mtn_clampi((int)lroundf(x / t->cell), 0, t->n - 1), gz = mtn_clampi((int)lroundf(z / t->cell), 0, t->n - 1);
            *out = gz * t->n + gx;
            return 1;
        }
    }
    return 0;
}

static void place_markers(mtn_terrain *t) {
    /* a cairn at the trailhead, one every 80 m beside the path and one on the summit;
     * painted trail posts every 40 m on alternating sides */
    free(t->cairns);
    free(t->posts);
    t->cairns = malloc(sizeof(vec3) * (size_t)(t->trail_len / 40 + 3));
    t->posts = malloc(sizeof(vec3) * (size_t)(t->trail_len / 20 + 2));
    t->cairn_count = 0;
    t->post_count = 0;
    for (int i = 0; i < t->trail_len; i += 20) {
        vec3 a = t->trail[i > 0 ? i - 1 : 0], b = t->trail[i + 1 < t->trail_len ? i + 1 : i];
        vec3 dir = v3_norm(v3(b.x - a.x, 0, b.z - a.z));
        if (v3_len(dir) < 0.5f) dir = v3(1, 0, 0);
        vec3 side = v3(-dir.z, 0, dir.x);
        if (i % 40 == 0 && i + 1 < t->trail_len) {
            float sgn = (i / 40) % 2 ? 1.0f : -1.0f;
            vec3 c = v3_add(t->trail[i], v3_scale(side, 2.7f * sgn)); /* clear of the walkable edge */
            c.y = mtn_terrain_height(t, c.x, c.z);
            t->cairns[t->cairn_count++] = c;
        }
        if (i > 0 && i % 20 == 0 && i % 40 != 0) {
            float sgn = (i / 20) % 4 == 1 ? 1.0f : -1.0f;
            vec3 c = v3_add(t->trail[i], v3_scale(side, 1.7f * sgn));
            c.y = mtn_terrain_height(t, c.x, c.z);
            t->posts[t->post_count++] = c;
        }
    }
    vec3 top = t->summit;
    top.x += 2.0f;
    top.y = mtn_terrain_height(t, top.x, top.z);
    t->cairns[t->cairn_count++] = top;
    /* signpost at the trailhead, on the other side of the cairn */
    if (t->trail_len > 2) {
        vec3 a = t->trail[0], b = t->trail[2];
        vec3 dir = v3_norm(v3(b.x - a.x, 0, b.z - a.z));
        t->signpost = v3_add(a, v3(dir.z * 2.0f, 0, -dir.x * 2.0f));
        t->signpost.y = mtn_terrain_height(t, t->signpost.x, t->signpost.z);
        t->signpost_yaw = atan2f(dir.x, -dir.z);
    }
}

/* Grade the terrain along the path: level across its width, never above the path. */
static void flatten_along_trail(mtn_terrain *t) {
    int n = t->n;
    size_t nn = (size_t)n * (size_t)n;
    const float r_flat = 8.0f, r_blend = 16.0f;
    float *best = malloc(sizeof(float) * nn);
    int *near = malloc(sizeof(int) * nn);
    for (size_t i = 0; i < nn; i++) {
        best[i] = 1e30f;
        near[i] = -1;
    }
    int rad = (int)ceilf(r_blend / t->cell);
    for (int k = 0; k < t->trail_len; k++) {
        vec3 p = t->trail[k];
        int cx = (int)lroundf(p.x / t->cell), cz = (int)lroundf(p.z / t->cell);
        for (int z = cz - rad; z <= cz + rad; z++)
            for (int x = cx - rad; x <= cx + rad; x++) {
                if (x < 0 || z < 0 || x >= n || z >= n) continue;
                float d = v3_dist_xz(p, v3((float)x * t->cell, 0, (float)z * t->cell));
                int i = z * n + x;
                if (d < best[i]) {
                    best[i] = d;
                    near[i] = k;
                }
            }
    }
    for (size_t i = 0; i < nn; i++) {
        if (best[i] > r_blend) continue;
        float w = 1.0f - mtn_smoothstep(r_flat, r_blend, best[i]);
        t->h[i] = mtn_lerpf(t->h[i], t->trail[near[i]].y - 0.08f, w);
    }
    /* the bilinear terrain must stay below the path surface under the whole strip
     * (only grid points belonging to this stretch of the path are lowered, so the
     * legs of a switchback do not cut into each other) */
    for (int pass = 0; pass < 3; pass++)
        for (int k = 0; k + 1 < t->trail_len; k++) {
            vec3 a = t->trail[k], b = t->trail[k + 1];
            vec3 d = v3_norm(v3(b.x - a.x, 0, b.z - a.z));
            vec3 side = v3(-d.z, 0, d.x);
            for (int s = -2; s <= 2; s++) {
                vec3 q = v3_add(a, v3_scale(side, 0.7f * (float)s));
                if (mtn_terrain_height(t, q.x, q.z) <= a.y - 0.03f) continue;
                int gx = (int)floorf(q.x / t->cell), gz = (int)floorf(q.z / t->cell);
                for (int c = 0; c < 4; c++) {
                    int x = mtn_clampi(gx + (c & 1), 0, n - 1), z = mtn_clampi(gz + (c >> 1), 0, n - 1);
                    int i = z * n + x;
                    if (t->h[i] > a.y - 0.08f) t->h[i] = a.y - 0.08f;
                }
            }
        }
    free(best);
    free(near);
}

/* Can the walker go straight from a to b? Gentle enough, no deep cut or fill, no water. */
static int straight_ok(const mtn_terrain *t, vec3 a, vec3 b) {
    float d = v3_dist_xz(a, b);
    if (d > 54.0f) return 0;
    float ha = mtn_terrain_height(t, a.x, a.z), hb = mtn_terrain_height(t, b.x, b.z);
    if (fabsf(hb - ha) > 0.27f * d) return 0;
    int steps = (int)(d / 2.0f) + 1;
    for (int s = 1; s < steps; s++) {
        float f = (float)s / (float)steps;
        float x = mtn_lerpf(a.x, b.x, f), z = mtn_lerpf(a.z, b.z, f);
        if (fabsf(mtn_terrain_height(t, x, z) - mtn_lerpf(ha, hb, f)) > 2.0f) return 0;
        if (mtn_terrain_water(t, x, z) != MTN_NO_WATER || !mtn_terrain_in_playable(t, x, z)) return 0;
    }
    return 1;
}

/* Replace grid staircases by straight runs where walking straight is fine. */
static int string_pull(const mtn_terrain *t, vec3 *pts, int count) {
    int o = 0, i = 0;
    while (i < count - 1) {
        pts[o++] = pts[i];
        int best = i + 1;
        for (int j = i + 2; j < count && j <= i + 12; j++)
            if (straight_ok(t, pts[i], pts[j])) best = j;
        i = best;
    }
    pts[o++] = pts[count - 1];
    return o;
}

/* The grade-limited profile closest to ys: the largest grade-limited profile below and
 * the smallest above, averaged; both ends stay at their height (the summit is not cut
 * down) when the length allows it. Result in out; tmp is scratch. */
static void grade_limit(const float *ys, const float *s_at, int tl, float y0, float y1, float len, float *out, float *tmp) {
    float *lo = out, *hi = tmp;
    for (int i = 0; i < tl; i++) lo[i] = hi[i] = ys[i];
    for (int i = 1; i < tl; i++) {
        float ds = (s_at[i] - s_at[i - 1]) * MTN_TRAIL_MAX_GRADE;
        lo[i] = fminf(lo[i], lo[i - 1] + ds);
        hi[i] = fmaxf(hi[i], hi[i - 1] - ds);
    }
    for (int i = tl - 2; i >= 0; i--) {
        float ds = (s_at[i + 1] - s_at[i]) * MTN_TRAIL_MAX_GRADE;
        lo[i] = fminf(lo[i], lo[i + 1] + ds);
        hi[i] = fmaxf(hi[i], hi[i + 1] - ds);
    }
    int pin = fabsf(y1 - y0) <= len * MTN_TRAIL_MAX_GRADE;
    for (int i = 0; i < tl; i++) {
        float p = 0.5f * (lo[i] + hi[i]);
        if (pin) {
            float a = s_at[i] * MTN_TRAIL_MAX_GRADE, b = (len - s_at[i]) * MTN_TRAIL_MAX_GRADE;
            p = mtn_clampf(p, fmaxf(y0 - a, y1 - b), fminf(y0 + a, y1 + b));
        }
        out[i] = p;
    }
}

int mtn_terrain_find_trail(mtn_terrain *t) {
    int n = t->n;
    size_t nn = (size_t)n * (size_t)n;
    trail_work w = {0};
    w.g = malloc(sizeof(float) * nn);
    w.from = malloc(sizeof(int) * nn);
    w.closed = malloc(nn);
    w.used = calloc(nn, 1);
    int start = mtn_clampi((int)lroundf(t->trailhead.z / t->cell), 0, n - 1) * n +
                mtn_clampi((int)lroundf(t->trailhead.x / t->cell), 0, n - 1);
    int goal = mtn_clampi((int)lroundf(t->summit.z / t->cell), 0, n - 1) * n +
               mtn_clampi((int)lroundf(t->summit.x / t->cell), 0, n - 1);
    /* waypoints spiral up the mountain at rising altitude */
    int stops[6], ns = 0;
    float a0 = atan2f(t->trailhead.z - t->summit.z, t->trailhead.x - t->summit.x);
    float dir = (t->p.seed & 1u) ? 1.0f : -1.0f;
    float start_level = mtn_terrain_altitude_frac(t, t->trailhead.y);
    static const float levels[2] = {0.30f, 0.62f};
    for (int k = 0; k < 2; k++) {
        if (levels[k] <= start_level + 0.05f) continue;
        int wp;
        if (find_waypoint(t, a0 + dir * (float)(k + 1) * 0.6f, levels[k], &wp)) stops[ns++] = wp;
    }
    stops[ns++] = goal;
    w.cap = 1024;
    w.path = malloc(sizeof(int) * (size_t)w.cap);
    w.path[w.len++] = start;
    int ok = 1, from = start;
    for (int k = 0; k < ns; k++) {
        if (route_leg(t, &w, from, stops[k])) from = stops[k];
        else ok = 0;
    }
    if (from != goal) ok = route_leg(t, &w, from, goal) && ok;
    free(w.g);
    free(w.from);
    free(w.closed);
    free(w.used);
    int count = w.len;
    vec3 *raw = malloc(sizeof(vec3) * (size_t)count);
    for (int i = 0; i < count; i++) raw[i] = v3((float)(w.path[i] % n) * t->cell, 0, (float)(w.path[i] / n) * t->cell);
    free(w.path);
    count = string_pull(t, raw, count);
    /* Chaikin smoothing (3 passes) */
    for (int pass = 0; pass < 3 && count > 2; pass++) {
        int nc = 2 * (count - 1) + 2;
        vec3 *sm = malloc(sizeof(vec3) * (size_t)nc);
        int o = 0;
        sm[o++] = raw[0];
        for (int i = 0; i < count - 1; i++) {
            sm[o++] = v3_lerp(raw[i], raw[i + 1], 0.25f);
            sm[o++] = v3_lerp(raw[i], raw[i + 1], 0.75f);
        }
        sm[o++] = raw[count - 1];
        free(raw);
        raw = sm;
        count = o;
    }
    /* resample every 2 m */
    float total = 0.0f;
    for (int i = 1; i < count; i++) total += v3_dist_xz(raw[i - 1], raw[i]);
    int samples = (int)(total / 2.0f) + 2;
    free(t->trail);
    t->trail = malloc(sizeof(vec3) * (size_t)samples);
    t->trail_len = 0;
    float acc = 0.0f, next = 0.0f;
    for (int i = 1; i < count && t->trail_len < samples; i++) {
        float seg = v3_dist_xz(raw[i - 1], raw[i]);
        while (next <= acc + seg && t->trail_len < samples) {
            float f = seg > 0 ? (next - acc) / seg : 0.0f;
            vec3 p = v3_lerp(raw[i - 1], raw[i], f);
            p.y = mtn_terrain_height(t, p.x, p.z);
            t->trail[t->trail_len++] = p;
            next += 2.0f;
        }
        acc += seg;
    }
    if (t->trail_len == 0) {
        vec3 p = raw[0];
        p.y = mtn_terrain_height(t, p.x, p.z);
        t->trail[t->trail_len++] = p;
    }
    free(raw);
    /* cut out small loops where the path comes back to itself */
    int kept = 0;
    for (int i = 0; i < t->trail_len; i++) {
        t->trail[kept++] = t->trail[i];
        for (int j = mtn_clampi(i + 40, 0, t->trail_len - 1); j > i + 4; j--)
            if (v3_dist_xz(t->trail[i], t->trail[j]) < 3.0f) {
                i = j - 1;
                break;
            }
    }
    t->trail_len = kept;
    /* walk straight up the last metres instead of circling the summit cone */
    if (ok && t->trail_len > 1) {
        int i = 0;
        while (i < t->trail_len - 1 && v3_dist_xz(t->trail[i], t->summit) > 12.0f) i++;
        vec3 a = t->trail[i];
        float d = v3_dist_xz(a, t->summit);
        int steps = (int)ceilf(d / 2.0f);
        t->trail_len = i + 1;
        for (int k = 1; k <= steps && t->trail_len < samples; k++) {
            vec3 q = v3_lerp(a, t->summit, (float)k / (float)steps);
            q.y = mtn_terrain_height(t, q.x, q.z);
            t->trail[t->trail_len++] = q;
        }
    }
    /* smooth the height profile over about 40 m so the path has no bumps */
    int tl = t->trail_len;
    float *ys = malloc(sizeof(float) * (size_t)tl), *lo = malloc(sizeof(float) * (size_t)tl),
          *hi = malloc(sizeof(float) * (size_t)tl), *s_at = malloc(sizeof(float) * (size_t)tl);
    for (int i = 0; i < tl; i++) {
        float sum = 0.0f;
        int c = 0;
        int win = mtn_clampi(tl - 1 - i, 0, mtn_clampi(i, 0, 10));
        for (int k = i - win; k <= i + win; k++) {
            sum += t->trail[k].y;
            c++;
        }
        ys[i] = sum / (float)c;
        s_at[i] = i > 0 ? s_at[i - 1] + v3_dist_xz(t->trail[i - 1], t->trail[i]) : 0.0f;
    }
    /* landings: where the path doubles back on itself (the tip of a switchback) the
     * stretch from one leg to the other is level, so turning there needs no step. The
     * grade limit below sees no length inside a landing, which keeps it level. */
    float *s_lim = malloc(sizeof(float) * (size_t)tl);
    uint8_t *flat = calloc((size_t)tl, 1);
    for (int i = 0; i < tl; i++)
        for (int j = i + 3; j < tl && j <= i + 12; j++)
            if (v3_dist_xz(t->trail[i], t->trail[j]) < 2.0f * MTN_TRAIL_WALK_HALF + 0.3f)
                for (int k = i + 1; k <= j; k++) flat[k] = 1;
    s_lim[0] = 0.0f;
    for (int i = 1; i < tl; i++) s_lim[i] = s_lim[i - 1] + (flat[i] ? 0.0f : s_at[i] - s_at[i - 1]);
    grade_limit(ys, s_lim, tl, ys[0], ys[tl - 1], s_lim[tl - 1], lo, hi);
    for (int i = 0; i < tl; i++) ys[i] = lo[i];
    free(s_lim);
    free(flat);
    for (int i = 0; i < tl; i++) t->trail[i].y = ys[i];
    free(ys);
    free(lo);
    free(hi);
    free(s_at);
    flatten_along_trail(t);
    t->summit.y = mtn_terrain_height(t, t->summit.x, t->summit.z);
    t->trailhead.y = mtn_terrain_height(t, t->trailhead.x, t->trailhead.z);
    place_markers(t);
    stamp_trail_distance(t);
    index_trail(t);
    return ok;
}

/* ------------------------------------------------------------------ viewpoints */

float mtn_terrain_openness(const mtn_terrain *t, vec3 eye, float yaw, float range) {
    vec3 d = v3(sinf(yaw), 0, -cosf(yaw));
    float max_ang = -1.0f;
    for (int i = 1; i <= 24; i++) {
        float s = range * (float)i / 24.0f;
        float x = eye.x + d.x * s, z = eye.z + d.z * s;
        float h = mtn_terrain_height(t, x, z);
        float ang = (h - eye.y) / s;
        if (ang > max_ang) max_ang = ang;
    }
    return 1.0f - mtn_clampf((max_ang + 0.08f) / 0.45f, 0.0f, 1.0f);
}

static int viewpoint_from(const mtn_terrain *t, float x, float z, mtn_viewpoint *vp) {
    vec3 eye = v3(x, mtn_terrain_height(t, x, z) + 1.7f, z);
    float best = -1.0f, besty = 0.0f, sum = 0.0f;
    float to_summit = atan2f(t->summit.x - x, -(t->summit.z - z));
    float sd = v3_dist_xz(eye, t->summit);
    for (int k = 0; k < 16; k++) {
        float yaw = (float)k * MTN_TAU / 16.0f;
        float o = mtn_terrain_openness(t, eye, yaw, 900.0f);
        sum += o;
        float bonus = sd > 300.0f ? 0.35f * mtn_maxf(0.0f, cosf(mtn_wrap_angle(yaw - to_summit))) : 0.0f;
        if (o + bonus > best) {
            best = o + bonus;
            besty = yaw;
        }
    }
    vp->pos = eye;
    vp->yaw = besty;
    vp->score = 0.3f * (sum / 16.0f) + 0.3f * best + 0.4f * mtn_terrain_altitude_frac(t, eye.y - 1.7f);
    return 1;
}

int mtn_terrain_find_viewpoints(mtn_terrain *t) {
    enum { MAXC = 4096 };
    static mtn_viewpoint cand[MAXC];
    int cc = 0;
    int stride = mtn_maxf(2.0f, 48.0f / t->cell) > 2.0f ? (int)(48.0f / t->cell) : 2;
    for (int z = stride; z < t->n - stride && cc < MAXC; z += stride)
        for (int x = stride; x < t->n - stride && cc < MAXC; x += stride) {
            float wx = (float)x * t->cell, wz = (float)z * t->cell;
            if (!mtn_terrain_in_playable(t, wx, wz)) continue;
            if (t->water[z * t->n + x] != MTN_NO_WATER) continue;
            if (mtn_terrain_slope(t, wx, wz) > 0.45f) continue;
            if (mtn_terrain_altitude_frac(t, t->h[z * t->n + x]) < 0.2f) continue;
            viewpoint_from(t, wx, wz, &cand[cc]);
            cand[cc].score += 0.05f * mtn_hash_unit(mtn_hash2i(x, z, t->p.seed));
            cc++;
        }
    t->vp_count = 0;
    /* summit first */
    viewpoint_from(t, t->summit.x, t->summit.z, &t->vps[t->vp_count++]);
    while (t->vp_count < MTN_MAX_VIEWPOINTS) {
        int best = -1;
        for (int i = 0; i < cc; i++) {
            if (cand[i].score < 0) continue;
            if (best < 0 || cand[i].score > cand[best].score) best = i;
        }
        if (best < 0) break;
        int ok = 1;
        for (int k = 0; k < t->vp_count; k++)
            if (v3_dist_xz(t->vps[k].pos, cand[best].pos) < 220.0f) ok = 0;
        if (ok) t->vps[t->vp_count++] = cand[best];
        cand[best].score = -1.0f;
    }
    return t->vp_count;
}

/* ------------------------------------------------------------------ generate */

static void pick_landmarks(mtn_terrain *t) {
    int n = t->n;
    float best = -1e30f;
    t->min_h = 1e30f;
    t->max_h = -1e30f;
    for (int z = 0; z < n; z++)
        for (int x = 0; x < n; x++) {
            float h = t->h[z * n + x];
            if (h < t->min_h) t->min_h = h;
            if (h > t->max_h) t->max_h = h;
            float wx = (float)x * t->cell, wz = (float)z * t->cell;
            if (mtn_terrain_in_playable(t, wx, wz) && h > best) {
                best = h;
                t->summit = v3(wx, h, wz);
            }
        }
    /* trailhead: lowest gentle dry point on a ring around the centre */
    float lowest = 1e30f;
    t->trailhead = t->center;
    for (int k = 0; k < 72 * 3; k++) {
        float r = playable_r(t) * (0.86f - 0.18f * (float)(k / 72));
        float a = (float)k * MTN_TAU / 72.0f;
        float x = t->center.x + cosf(a) * r, z = t->center.z + sinf(a) * r;
        float h = mtn_terrain_height(t, x, z);
        float sl = mtn_terrain_slope(t, x, z);
        if (mtn_terrain_water(t, x, z) != MTN_NO_WATER) continue;
        float score = h + sl * 400.0f;
        if (score < lowest) {
            lowest = score;
            t->trailhead = v3(x, h, z);
        }
    }
}

int mtn_terrain_generate(mtn_terrain *t, const mtn_terrain_params *p) {
    memset(t, 0, sizeof *t);
    t->p = *p;
    t->n = p->size < 17 ? 17 : p->size;
    t->cell = p->cell;
    t->world_size = (float)(t->n - 1) * t->cell;
    t->center = v3(t->world_size * 0.5f, 0, t->world_size * 0.5f);
    size_t nn = (size_t)t->n * (size_t)t->n;
    t->h = malloc(sizeof(float) * nn);
    t->water = malloc(sizeof(float) * nn);
    t->moist = malloc(sizeof(float) * nn);
    t->trail_dist = malloc(sizeof(float) * nn);
    t->biome = malloc(nn);
    generate_shape(t);
    mtn_terrain_erode(t->h, t->n, t->cell, p->erosion_drops, p->seed);
    mtn_terrain_fill_lakes(t);
    pick_landmarks(t);
    t->center.y = mtn_terrain_height(t, t->center.x, t->center.z);
    compute_moisture(t);
    compute_biomes(t);
    int ok = mtn_terrain_find_trail(t);
    mtn_terrain_find_viewpoints(t);
    return ok;
}

static uint32_t fnv(uint32_t h, int32_t v) {
    for (int i = 0; i < 4; i++) {
        h ^= (uint32_t)((v >> (i * 8)) & 0xFF);
        h *= 16777619u;
    }
    return h;
}

static int32_t cm(float v) { return (int32_t)lroundf(v * 100.0f); }

uint32_t mtn_terrain_checksum(const mtn_terrain *t) {
    uint32_t h = 2166136261u;
    size_t nn = (size_t)t->n * (size_t)t->n;
    for (size_t i = 0; i < nn; i++) {
        h = fnv(h, cm(t->h[i]));
        h = fnv(h, t->water[i] == MTN_NO_WATER ? -1 : cm(t->water[i]));
        h = fnv(h, t->biome[i]);
    }
    for (int i = 0; i < t->trail_len; i++) h = fnv(fnv(h, cm(t->trail[i].x)), cm(t->trail[i].z));
    for (int i = 0; i < t->cairn_count; i++) h = fnv(fnv(h, cm(t->cairns[i].x)), cm(t->cairns[i].z));
    for (int i = 0; i < t->post_count; i++) h = fnv(fnv(h, cm(t->posts[i].x)), cm(t->posts[i].z));
    for (int i = 0; i < t->vp_count; i++) h = fnv(fnv(h, cm(t->vps[i].pos.x)), cm(t->vps[i].yaw));
    return h;
}

void mtn_terrain_free(mtn_terrain *t) {
    free(t->h);
    free(t->water);
    free(t->moist);
    free(t->trail_dist);
    free(t->trail_cell_start);
    free(t->trail_cell_items);
    free(t->biome);
    free(t->trail);
    free(t->cairns);
    free(t->posts);
    memset(t, 0, sizeof *t);
}
