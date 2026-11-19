/* Mountain engine - deterministic random numbers, hashing and noise. MIT License. */
#ifndef MTN_RNG_H
#define MTN_RNG_H

#include <stdint.h>

/* PCG32 generator: identical sequences on every platform. */
typedef struct { uint64_t state, inc; } mtn_rng;

void mtn_rng_seed(mtn_rng *r, uint64_t seed, uint64_t seq);
uint32_t mtn_rng_u32(mtn_rng *r);
float mtn_rng_float(mtn_rng *r);               /* [0, 1) */
float mtn_rng_range(mtn_rng *r, float a, float b);
int mtn_rng_int(mtn_rng *r, int lo, int hi);   /* inclusive */

uint32_t mtn_hash_u32(uint32_t x);
uint32_t mtn_hash2i(int x, int y, uint32_t seed);
uint32_t mtn_hash3i(int x, int y, int z, uint32_t seed);
float mtn_hash_unit(uint32_t h);                /* [0, 1) */
uint32_t mtn_hash_str(const char *s);

/* Gradient noise in [-1, 1] (approximately) and fractal variants. */
float mtn_noise2(float x, float y, uint32_t seed);
float mtn_fbm2(float x, float y, uint32_t seed, int octaves, float lacunarity, float gain);
float mtn_ridged2(float x, float y, uint32_t seed, int octaves, float lacunarity, float gain);

#endif
