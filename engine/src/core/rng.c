/* Mountain engine - deterministic random numbers, hashing and noise. MIT License. */
#include "mtn/rng.h"

#include <math.h>

void mtn_rng_seed(mtn_rng *r, uint64_t seed, uint64_t seq) {
    r->state = 0u;
    r->inc = (seq << 1u) | 1u;
    mtn_rng_u32(r);
    r->state += seed;
    mtn_rng_u32(r);
}

uint32_t mtn_rng_u32(mtn_rng *r) {
    uint64_t old = r->state;
    r->state = old * 6364136223846793005ULL + r->inc;
    uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    uint32_t rot = (uint32_t)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

float mtn_rng_float(mtn_rng *r) { return (float)(mtn_rng_u32(r) >> 8) * (1.0f / 16777216.0f); }

float mtn_rng_range(mtn_rng *r, float a, float b) { return a + (b - a) * mtn_rng_float(r); }

int mtn_rng_int(mtn_rng *r, int lo, int hi) {
    if (hi <= lo) return lo;
    uint32_t span = (uint32_t)(hi - lo) + 1u;
    return lo + (int)(mtn_rng_u32(r) % span);
}

uint32_t mtn_hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

uint32_t mtn_hash2i(int x, int y, uint32_t seed) {
    return mtn_hash_u32((uint32_t)x * 0x9E3779B1u ^ mtn_hash_u32((uint32_t)y * 0x85EBCA77u ^ seed));
}

uint32_t mtn_hash3i(int x, int y, int z, uint32_t seed) {
    return mtn_hash2i(x, y, mtn_hash_u32((uint32_t)z * 0xC2B2AE3Du ^ seed));
}

float mtn_hash_unit(uint32_t h) { return (float)(h >> 8) * (1.0f / 16777216.0f); }

uint32_t mtn_hash_str(const char *s) {
    uint32_t h = 2166136261u;
    while (*s) {
        h ^= (uint8_t)*s++;
        h *= 16777619u;
    }
    return h;
}

static float grad2(uint32_t h, float x, float y) {
    /* 8 gradient directions */
    switch (h & 7u) {
    case 0: return x + y;
    case 1: return -x + y;
    case 2: return x - y;
    case 3: return -x - y;
    case 4: return x * 1.4142135f;
    case 5: return -x * 1.4142135f;
    case 6: return y * 1.4142135f;
    default: return -y * 1.4142135f;
    }
}

static float quintic(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

float mtn_noise2(float x, float y, uint32_t seed) {
    float fx = floorf(x), fy = floorf(y);
    int ix = (int)fx, iy = (int)fy;
    float dx = x - fx, dy = y - fy;
    float n00 = grad2(mtn_hash2i(ix, iy, seed), dx, dy);
    float n10 = grad2(mtn_hash2i(ix + 1, iy, seed), dx - 1.0f, dy);
    float n01 = grad2(mtn_hash2i(ix, iy + 1, seed), dx, dy - 1.0f);
    float n11 = grad2(mtn_hash2i(ix + 1, iy + 1, seed), dx - 1.0f, dy - 1.0f);
    float u = quintic(dx), v = quintic(dy);
    float a = n00 + (n10 - n00) * u;
    float b = n01 + (n11 - n01) * u;
    return (a + (b - a) * v) * 0.7071f;
}

float mtn_fbm2(float x, float y, uint32_t seed, int octaves, float lac, float gain) {
    float sum = 0.0f, amp = 1.0f, norm = 0.0f;
    for (int i = 0; i < octaves; i++) {
        sum += amp * mtn_noise2(x, y, seed + (uint32_t)i * 101u);
        norm += amp;
        amp *= gain;
        x *= lac;
        y *= lac;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}

float mtn_ridged2(float x, float y, uint32_t seed, int octaves, float lac, float gain) {
    float sum = 0.0f, amp = 1.0f, norm = 0.0f, weight = 1.0f;
    for (int i = 0; i < octaves; i++) {
        float n = 1.0f - fabsf(mtn_noise2(x, y, seed + (uint32_t)i * 131u));
        n *= n;
        n *= weight;
        weight = n * 1.5f;
        if (weight > 1.0f) weight = 1.0f;
        sum += n * amp;
        norm += amp;
        amp *= gain;
        x *= lac;
        y *= lac;
    }
    return norm > 0.0f ? sum / norm : 0.0f;
}
