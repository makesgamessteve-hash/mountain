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

int mtn_terrain_find_trail(mtn_terrain *t) {
    /* A* on a half-resolution grid with 16 neighbours (allows switchbacks). */
    int step = t->n > 300 ? 2 : 1;
    int m = (t->n - 1) / step + 1;
    float cs = t->cell * (float)step;
    size_t mm = (size_t)m * (size_t)m;
    float *g = malloc(sizeof(float) * mm);
    int *from = malloc(sizeof(int) * mm);
    uint8_t *closed = calloc(mm, 1);
    heap hp = {0};
    for (size_t i = 0; i < mm; i++) {
        g[i] = 1e30f;
        from[i] = -1;
    }
    int sx = mtn_clampi((int)(t->trailhead.x / cs + 0.5f), 0, m - 1);
    int sz = mtn_clampi((int)(t->trailhead.z / cs + 0.5f), 0, m - 1);
    int gx = mtn_clampi((int)(t->summit.x / cs + 0.5f), 0, m - 1);
    int gz = mtn_clampi((int)(t->summit.z / cs + 0.5f), 0, m - 1);
    int start = sz * m + sx, goal = gz * m + gx;
    g[start] = 0.0f;
    heap_push(&hp, 0.0f, start);
    while (hp.n > 0) {
        heap_item c = heap_pop(&hp);
        if (closed[c.i]) continue;
        closed[c.i] = 1;
        if (c.i == goal) break;
        int cx = c.i % m, cz = c.i / m;
        float ch = mtn_terrain_grid(t, cx * step, cz * step);
        for (int k = 0; k < 16; k++) {
            int x = cx + NB16X[k], z = cz + NB16Z[k];
            if (x < 0 || z < 0 || x >= m || z >= m) continue;
            int j = z * m + x;
            if (closed[j]) continue;
            float wx = (float)x * cs, wz = (float)z * cs;
            float nh = mtn_terrain_grid(t, x * step, z * step);
            float dist = cs * sqrtf((float)(NB16X[k] * NB16X[k] + NB16Z[k] * NB16Z[k]));
            float grade = fabsf(nh - ch) / dist;
            float cost = dist * (1.0f + 28.0f * grade * grade);
            if (grade > 0.75f) cost *= 40.0f;
            if (t->water[(z * step) * t->n + x * step] != MTN_NO_WATER) cost *= 60.0f;
            if (!mtn_terrain_in_playable(t, wx, wz)) cost *= 20.0f;
            float ng = g[c.i] + cost;
            if (ng < g[j]) {
                g[j] = ng;
                from[j] = c.i;
                float dxg = (float)(x - gx) * cs, dzg = (float)(z - gz) * cs;
                heap_push(&hp, ng + sqrtf(dxg * dxg + dzg * dzg), j);
            }
        }
    }
    free(hp.a);
    /* reconstruct */
    int count = 0;
    for (int c = goal; c >= 0; c = from[c]) count++;
    int ok = from[goal] >= 0 || goal == start;
    vec3 *raw = malloc(sizeof(vec3) * (size_t)(count > 0 ? count : 1));
    int k = count;
    for (int c = goal; c >= 0; c = from[c]) {
        k--;
        raw[k] = v3((float)(c % m) * cs, 0, (float)(c / m) * cs);
    }
    free(g);
    free(from);
    free(closed);
    /* Chaikin smoothing (2 passes) */
    for (int pass = 0; pass < 2 && count > 2; pass++) {
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
    if (t->trail_len == 0 && count > 0) {
        vec3 p = raw[0];
        p.y = mtn_terrain_height(t, p.x, p.z);
        t->trail[t->trail_len++] = p;
    }
    free(raw);
    /* cairns every 80 m beside the path, plus one on the summit */
    free(t->cairns);
    t->cairns = malloc(sizeof(vec3) * (size_t)(t->trail_len / 40 + 2));
    t->cairn_count = 0;
    for (int i = 40; i < t->trail_len - 1; i += 40) {
        vec3 a = t->trail[i - 1], b = t->trail[i + 1];
        vec3 dir = v3_norm(v3(b.x - a.x, 0, b.z - a.z));
        vec3 side = v3(-dir.z, 0, dir.x);
        float sgn = (i / 40) % 2 ? 1.0f : -1.0f;
        vec3 c = v3_add(t->trail[i], v3_scale(side, 1.8f * sgn));
        c.y = mtn_terrain_height(t, c.x, c.z);
        t->cairns[t->cairn_count++] = c;
    }
    vec3 top = t->summit;
    top.x += 2.0f;
    top.y = mtn_terrain_height(t, top.x, top.z);
    t->cairns[t->cairn_count++] = top;
    stamp_trail_distance(t);
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

void mtn_terrain_free(mtn_terrain *t) {
    free(t->h);
    free(t->water);
    free(t->moist);
    free(t->trail_dist);
    free(t->biome);
    free(t->trail);
    free(t->cairns);
    memset(t, 0, sizeof *t);
}
