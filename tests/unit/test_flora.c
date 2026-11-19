#include "test.h"
#include "mtn/flora.h"

#include <stdlib.h>

/* ------------------------------------------------------------------ fixtures */

/* A flat, uniform terrain (n*n grid) with exact control over every layer. */
static void flat_terrain(mtn_terrain *t, int n, float cell, float height, mtn_biome b) {
    memset(t, 0, sizeof *t);
    mtn_terrain_default_params(&t->p);
    t->n = n;
    t->cell = cell;
    t->world_size = (float)(n - 1) * cell;
    t->center = v3(t->world_size * 0.5f, height, t->world_size * 0.5f);
    size_t nn = (size_t)n * (size_t)n;
    t->h = malloc(sizeof(float) * nn);
    t->water = malloc(sizeof(float) * nn);
    t->moist = malloc(sizeof(float) * nn);
    t->trail_dist = malloc(sizeof(float) * nn);
    t->biome = malloc(nn);
    for (size_t i = 0; i < nn; i++) {
        t->h[i] = height;
        t->water[i] = MTN_NO_WATER;
        t->moist[i] = 0.5f;
        t->trail_dist[i] = 1000.0f;
        t->biome[i] = (uint8_t)b;
    }
    t->min_h = 0.0f;
    t->max_h = 100.0f;
}

static int gi(const mtn_terrain *t, int x, int z) { return z * t->n + x; }

static mtn_terrain g_terrain; /* small generated mountain shared by the placement tests */

static void add_species(mtn_flora *f) {
    mtn_plant_species s;
    mtn_species_defaults(&s);
    strcpy(s.name, "spruce");
    s.crown = MTN_CROWN_CONE;
    s.biome_mask = 1 << MTN_BIOME_FOREST;
    s.height_min = 10.0f;
    s.height_max = 20.0f;
    s.density = 0.6f;
    s.leaf_tiles[0] = 2;
    mtn_flora_add_species(f, &s);

    mtn_species_defaults(&s);
    strcpy(s.name, "beech");
    s.deciduous = 1;
    s.biome_mask = (1 << MTN_BIOME_FOREST) | (1 << MTN_BIOME_MEADOW);
    s.leaf_color[MTN_AUTUMN] = v3(0.8f, 0.4f, 0.1f);
    s.leaf_tiles[0] = 3;
    s.leaf_tiles[1] = 4;
    s.leaf_tile_count = 2;
    s.density = 0.4f;
    mtn_flora_add_species(f, &s);

    mtn_species_defaults(&s);
    strcpy(s.name, "juniper");
    s.kind = MTN_PLANT_BUSH;
    s.biome_mask = (1 << MTN_BIOME_ALPINE) | (1 << MTN_BIOME_MEADOW);
    s.height_min = 0.5f;
    s.height_max = 1.5f;
    s.density = 0.7f;
    mtn_flora_add_species(f, &s);

    mtn_species_defaults(&s);
    strcpy(s.name, "boulder");
    s.kind = MTN_PLANT_BOULDER;
    s.biome_mask = (1 << MTN_BIOME_SCREE) | (1 << MTN_BIOME_ROCK) | (1 << MTN_BIOME_ALPINE);
    s.slope_max = 1.0f;
    s.height_min = 0.8f;
    s.height_max = 2.5f;
    s.density = 0.8f;
    mtn_flora_add_species(f, &s);

    mtn_species_defaults(&s);
    strcpy(s.name, "grass");
    s.kind = MTN_PLANT_GROUND;
    s.biome_mask = (1 << MTN_BIOME_FOREST) | (1 << MTN_BIOME_MEADOW) | (1 << MTN_BIOME_ALPINE);
    s.height_min = 0.3f;
    s.height_max = 0.6f;
    s.density = 0.9f;
    mtn_flora_add_species(f, &s);

    mtn_species_defaults(&s);
    strcpy(s.name, "gentian");
    s.kind = MTN_PLANT_GROUND;
    s.bloom_mask = (1 << MTN_SPRING) | (1 << MTN_SUMMER);
    s.biome_mask = (1 << MTN_BIOME_ALPINE) | (1 << MTN_BIOME_MEADOW) | (1 << MTN_BIOME_FOREST);
    s.height_min = 0.1f;
    s.height_max = 0.2f;
    s.density = 0.6f;
    mtn_flora_add_species(f, &s);
}

/* Builds a flora directly from a plant list (sorted into chunks like mtn_flora_populate does). */
static void flora_from_plants(mtn_flora *f, const mtn_plant *pl, int n, int chunks) {
    f->chunks = chunks;
    int nch = chunks * chunks;
    f->chunk_start = calloc((size_t)nch, sizeof(int));
    f->chunk_count = calloc((size_t)nch, sizeof(int));
    f->chunk_version = calloc((size_t)nch, sizeof(int));
    f->plants = malloc(sizeof(mtn_plant) * (size_t)(n > 0 ? n : 1));
    f->count = f->cap = n;
    int k = 0;
    for (int c = 0; c < nch; c++) {
        f->chunk_start[c] = k;
        for (int i = 0; i < n; i++)
            if (mtn_flora_chunk_of(f, pl[i].pos.x, pl[i].pos.z) == c) f->plants[k++] = pl[i];
        f->chunk_count[c] = k - f->chunk_start[c];
    }
}

static mtn_plant plant_at(float x, float y, float z, float scale, int species) {
    mtn_plant p;
    memset(&p, 0, sizeof p);
    p.pos = v3(x, y, z);
    p.scale = scale;
    p.species = (uint16_t)species;
    p.variant = 3;
    return p;
}

static mtn_season_blend pure(mtn_season s) {
    mtn_season_blend b;
    memset(&b, 0, sizeof b);
    b.w[s] = 1.0f;
    b.dominant = s;
    return b;
}

/* ------------------------------------------------------------------ tests */

static void test_species(void) {
    mtn_flora f;
    mtn_flora_init(&f);
    CHECK(f.species_count == 0 && f.plants == NULL && f.chunks == 0);
    mtn_plant_species s;
    mtn_species_defaults(&s);
    CHECK_STR(s.name, "plant");
    CHECK(s.kind == MTN_PLANT_TREE);
    CHECK(s.blossom_tile == -1);
    CHECK(s.biome_mask == 0xFE);
    CHECK(!(s.biome_mask & (1 << MTN_BIOME_WATER)));

    s.leaf_tile_count = 0;
    CHECK(mtn_flora_add_species(&f, &s) == 0);
    CHECK(f.species[0].leaf_tile_count == 1);
    strcpy(s.name, "many");
    s.leaf_tile_count = 9;
    CHECK(mtn_flora_add_species(&f, &s) == 1);
    CHECK(f.species[1].leaf_tile_count == 4);
    CHECK(mtn_flora_find_species(&f, "many") == 1);
    CHECK(mtn_flora_find_species(&f, "plant") == 0);
    CHECK(mtn_flora_find_species(&f, "baobab") == -1);
    for (int i = 2; i < MTN_MAX_SPECIES; i++) CHECK(mtn_flora_add_species(&f, &s) == i);
    CHECK(mtn_flora_add_species(&f, &s) == -1);
    CHECK(f.species_count == MTN_MAX_SPECIES);

    /* freeing the plants keeps the species definitions */
    mtn_flora_free(&f);
    CHECK(f.species_count == MTN_MAX_SPECIES);
    CHECK_STR(f.species[1].name, "many");
    mtn_flora_clear_species(&f);
    CHECK(f.species_count == 0);
    CHECK(mtn_flora_find_species(&f, "many") == -1);
}

static void test_habitat(void) {
    mtn_terrain t;
    flat_terrain(&t, 33, 4.0f, 10.0f, MTN_BIOME_FOREST);
    mtn_plant_species s;
    mtn_species_defaults(&s);
    float x = 40.0f, z = 40.0f;
    CHECK_NEAR(mtn_flora_habitat(&s, &t, x, z), 1.0f, 1e-5);

    mtn_plant_species m = s;
    m.biome_mask = 1 << MTN_BIOME_MEADOW;
    CHECK_NEAR(mtn_flora_habitat(&m, &t, x, z), 0.0f, 0);
    m = s;
    m.alt_min = 0.5f; /* ground is at altitude 0.1 */
    CHECK_NEAR(mtn_flora_habitat(&m, &t, x, z), 0.0f, 1e-6);
    m = s;
    m.alt_max = 0.1f; /* right at the edge: soft */
    float edge = mtn_flora_habitat(&m, &t, x, z);
    CHECK(edge > 0.2f && edge < 0.8f);
    m = s;
    m.moist_min = 0.8f;
    CHECK_NEAR(mtn_flora_habitat(&m, &t, x, z), 0.0f, 1e-6);

    /* close to the trail: trees keep 3 m clear, ground cover only 0.8 m */
    for (int i = 0; i < 33 * 33; i++) t.trail_dist[i] = 2.0f;
    CHECK_NEAR(mtn_flora_habitat(&s, &t, x, z), 0.0f, 1e-6);
    mtn_plant_species g = s;
    g.kind = MTN_PLANT_GROUND;
    CHECK(mtn_flora_habitat(&g, &t, x, z) > 0.3f);
    for (int i = 0; i < 33 * 33; i++) t.trail_dist[i] = 1000.0f;

    /* lakes are never habitat */
    for (int zz = 8; zz <= 12; zz++)
        for (int xx = 8; xx <= 12; xx++) {
            t.water[gi(&t, xx, zz)] = 12.0f;
            t.biome[gi(&t, xx, zz)] = MTN_BIOME_FOREST; /* even if the biome would fit */
        }
    CHECK_NEAR(mtn_flora_habitat(&s, &t, 40.0f, 40.0f), 0.0f, 0);
    CHECK_NEAR(mtn_flora_habitat(&s, &t, 100.0f, 100.0f), 1.0f, 1e-5);

    /* steep slopes: a 45 degree ramp is fine for rocks but not for trees with a low limit */
    for (int zz = 0; zz < 33; zz++)
        for (int xx = 20; xx < 33; xx++) t.h[gi(&t, xx, zz)] = 10.0f + (float)(xx - 20) * 4.0f * 2.0f;
    float sx = 26.0f * 4.0f, sz = 60.0f;
    CHECK(mtn_terrain_slope(&t, sx, sz) > 0.4f);
    m = s;
    m.slope_max = 0.2f;
    CHECK_NEAR(mtn_flora_habitat(&m, &t, sx, sz), 0.0f, 1e-6);
    m.slope_max = 1.0f;
    CHECK(mtn_flora_habitat(&m, &t, sx, sz) > 0.9f);
    mtn_terrain_free(&t);
}

static void test_chunk_of(void) {
    mtn_flora f;
    mtn_flora_init(&f);
    f.chunks = 4;
    CHECK(mtn_flora_chunk_of(&f, 10.0f, 10.0f) == 0);
    CHECK(mtn_flora_chunk_of(&f, MTN_FLORA_CHUNK + 1.0f, 10.0f) == 1);
    CHECK(mtn_flora_chunk_of(&f, 10.0f, MTN_FLORA_CHUNK * 2.5f) == 8);
    CHECK(mtn_flora_chunk_of(&f, -500.0f, -500.0f) == 0);
    CHECK(mtn_flora_chunk_of(&f, 1e5f, 1e5f) == 15);
}

static int check_sorted_by_chunk(const mtn_flora *f) {
    int total = 0;
    for (int c = 0; c < f->chunks * f->chunks; c++) {
        if (f->chunk_start[c] != total) return 0;
        for (int i = f->chunk_start[c]; i < f->chunk_start[c] + f->chunk_count[c]; i++)
            if (mtn_flora_chunk_of(f, f->plants[i].pos.x, f->plants[i].pos.z) != c) return 0;
        total += f->chunk_count[c];
    }
    return total == f->count;
}

static void test_populate(void) {
    const mtn_terrain *t = &g_terrain;
    mtn_flora a, b;
    mtn_flora_init(&a);
    mtn_flora_init(&b);
    add_species(&a);
    add_species(&b);
    mtn_flora_populate(&a, t, 5);
    mtn_flora_populate(&b, t, 5);
    CHECK(a.count > 200);
    CHECK(a.count == b.count);
    CHECK(memcmp(a.plants, b.plants, sizeof(mtn_plant) * (size_t)a.count) == 0);
    CHECK(a.chunks == (int)ceilf(t->world_size / MTN_FLORA_CHUNK));
    CHECK(a.seed == 5);
    CHECK(check_sorted_by_chunk(&a));

    int per_species[8] = {0};
    int wrong_biome = 0, wet = 0, off_ground = 0, bad_scale = 0, too_far = 0;
    float limit = t->p.playable_radius * t->world_size * 0.5f + 350.0f;
    for (int i = 0; i < a.count; i++) {
        const mtn_plant *p = &a.plants[i];
        const mtn_plant_species *s = &a.species[p->species];
        per_species[p->species]++;
        if (!(s->biome_mask & (1 << mtn_terrain_biome(t, p->pos.x, p->pos.z)))) wrong_biome++;
        if (mtn_terrain_water(t, p->pos.x, p->pos.z) != MTN_NO_WATER) wet++;
        if (fabsf(p->pos.y - mtn_terrain_height(t, p->pos.x, p->pos.z)) > 1e-3f) off_ground++;
        if (p->scale < s->height_min - 1e-4f || p->scale > s->height_max + 1e-4f) bad_scale++;
        if (v3_dist_xz(p->pos, t->center) > limit) too_far++;
        CHECK(!p->hidden);
        CHECK(p->rot >= 0.0f && p->rot < MTN_TAU);
    }
    CHECK(wrong_biome == 0);
    CHECK(wet == 0);
    CHECK(off_ground == 0);
    CHECK(bad_scale == 0);
    CHECK(too_far == 0);
    CHECK(per_species[0] > 0); /* spruce */
    CHECK(per_species[1] > 0); /* beech */
    CHECK(per_species[2] > 0); /* juniper */
    CHECK(per_species[3] > 0); /* boulders */
    CHECK(per_species[4] == 0 && per_species[5] == 0); /* ground cover is not placed as plants */

    /* another seed gives another forest; repopulating replaces the old one */
    mtn_flora_populate(&b, t, 6);
    CHECK(b.count > 200);
    CHECK(b.count != a.count || memcmp(a.plants, b.plants, sizeof(mtn_plant) * (size_t)a.count) != 0);
    CHECK(check_sorted_by_chunk(&b));

    /* without species there is nothing to place */
    mtn_flora_clear_species(&b);
    mtn_flora_populate(&b, t, 5);
    CHECK(b.count == 0);
    CHECK(b.chunk_start != NULL && b.chunk_count[0] == 0);
    mtn_flora_free(&a);
    mtn_flora_free(&b);
    CHECK(a.plants == NULL && a.chunk_start == NULL && a.count == 0);
}

static void test_populate_flat(void) {
    /* a uniform forest: placement density follows the species density */
    mtn_terrain t;
    flat_terrain(&t, 33, 8.0f, 10.0f, MTN_BIOME_FOREST);
    mtn_flora f;
    mtn_flora_init(&f);
    mtn_plant_species s;
    mtn_species_defaults(&s);
    s.patchiness = 0.0f;
    s.density = 0.5f;
    mtn_flora_add_species(&f, &s);
    mtn_flora_populate(&f, &t, 1);
    int cells = 32 * 32;
    CHECK(f.count > cells * 0.4 && f.count < cells * 0.6);
    CHECK(f.chunks == 3);
    CHECK(check_sorted_by_chunk(&f));
    /* the same terrain as a lake holds nothing */
    for (int i = 0; i < 33 * 33; i++) {
        t.water[i] = 20.0f;
        t.biome[i] = MTN_BIOME_WATER;
    }
    mtn_flora_populate(&f, &t, 1);
    CHECK(f.count == 0);
    mtn_flora_free(&f);
    mtn_terrain_free(&t);
}

static void test_season(void) {
    mtn_flora f;
    mtn_flora_init(&f);
    add_species(&f);
    mtn_plant_species cherry;
    mtn_species_defaults(&cherry);
    strcpy(cherry.name, "cherry");
    cherry.blossom_tile = 20;
    cherry.deciduous = 1;
    int ci = mtn_flora_add_species(&f, &cherry);
    int spruce = mtn_flora_find_species(&f, "spruce"), beech = mtn_flora_find_species(&f, "beech");
    int grass = mtn_flora_find_species(&f, "grass"), gentian = mtn_flora_find_species(&f, "gentian");

    mtn_flora_season fs;
    mtn_flora_season_eval(&f, pure(MTN_SUMMER), 0.0f, &fs);
    CHECK_NEAR(fs.leaf_amount[beech], 1.0f, 1e-6);
    CHECK_NEAR(fs.leaf_amount[spruce], 1.0f, 1e-6);
    CHECK_NEAR(fs.leaf_amount[gentian], 1.0f, 1e-6);
    CHECK_NEAR(fs.leaf_amount[grass], 1.0f, 1e-6);
    CHECK_NEAR(fs.blossom[ci], 0.0f, 1e-6);
    CHECK_NEAR(fs.blossom[beech], 0.0f, 0);

    mtn_flora_season_eval(&f, pure(MTN_WINTER), 1.0f, &fs);
    CHECK_NEAR(fs.leaf_amount[beech], 0.0f, 1e-6);   /* deciduous trees are bare */
    CHECK_NEAR(fs.leaf_amount[spruce], 1.0f, 1e-6);  /* evergreens keep their needles */
    CHECK_NEAR(fs.leaf_amount[gentian], 0.0f, 1e-6); /* not flowering */
    CHECK_NEAR(fs.leaf_amount[grass], 0.15f, 1e-5);  /* flattened by snow */

    mtn_flora_season_eval(&f, pure(MTN_SPRING), 0.5f, &fs);
    CHECK_NEAR(fs.blossom[ci], 1.0f, 1e-6);
    CHECK_NEAR(fs.leaf_amount[gentian], 1.0f - 0.85f * 0.5f, 1e-5);

    /* autumn colours */
    mtn_flora_season_eval(&f, pure(MTN_AUTUMN), 0.0f, &fs);
    CHECK_NEAR(fs.leaf_color[beech].x, 0.8f, 1e-6);
    CHECK_NEAR(fs.leaf_color[spruce].x, 0.35f, 1e-6);

    /* blends: leaves drop gradually as winter takes over */
    float prev = 2.0f;
    for (int i = 0; i <= 10; i++) {
        mtn_season_blend b = pure(MTN_AUTUMN);
        b.w[MTN_AUTUMN] = 1.0f - (float)i / 10.0f;
        b.w[MTN_WINTER] = (float)i / 10.0f;
        mtn_flora_season_eval(&f, b, 0.0f, &fs);
        CHECK(fs.leaf_amount[beech] <= prev);
        prev = fs.leaf_amount[beech];
        vec3 c = fs.leaf_color[beech];
        CHECK_NEAR(c.x, 0.8f * b.w[MTN_AUTUMN] + 0.35f * b.w[MTN_WINTER], 1e-5);
    }
}

static int same_items(const mtn_ground_item *a, const mtn_ground_item *b, int n) {
    for (int i = 0; i < n; i++)
        if (a[i].pos.x != b[i].pos.x || a[i].pos.y != b[i].pos.y || a[i].pos.z != b[i].pos.z || a[i].size != b[i].size ||
            a[i].species != b[i].species || a[i].variant != b[i].variant)
            return 0;
    return 1;
}

static void test_ground_cell(void) {
    const mtn_terrain *t = &g_terrain;
    mtn_flora f;
    mtn_flora_init(&f);
    add_species(&f);
    mtn_flora_populate(&f, t, 9);
    mtn_ground_item a[32], b[32];
    int total = 0, cells = 0, wrong = 0;
    /* around the trailhead in the meadows */
    int basex = (int)(t->trailhead.x / 8.0f), basez = (int)(t->trailhead.z / 8.0f);
    for (int cz = basez - 20; cz < basez + 20; cz += 3)
        for (int cx = basex - 20; cx < basex + 20; cx += 3) {
            int n = mtn_flora_ground_cell(&f, t, cx, cz, a, 32);
            int m = mtn_flora_ground_cell(&f, t, cx, cz, b, 32);
            CHECK(n == m);
            CHECK(n <= 28);
            CHECK(same_items(a, b, n));
            for (int i = 0; i < n; i++) {
                const mtn_plant_species *s = &f.species[a[i].species];
                if (s->kind != MTN_PLANT_GROUND) wrong++;
                if (!(s->biome_mask & (1 << mtn_terrain_biome(t, a[i].pos.x, a[i].pos.z)))) wrong++;
                if (a[i].pos.x < cx * 8.0f || a[i].pos.x >= cx * 8.0f + 8.0f) wrong++;
                if (a[i].pos.z < cz * 8.0f || a[i].pos.z >= cz * 8.0f + 8.0f) wrong++;
                if (a[i].size < s->height_min - 1e-4f || a[i].size > s->height_max + 1e-4f) wrong++;
                CHECK_NEAR(a[i].pos.y, mtn_terrain_height(t, a[i].pos.x, a[i].pos.z), 1e-3);
            }
            total += n;
            cells++;
        }
    CHECK(wrong == 0);
    CHECK(total > cells); /* the valley floor is grassy */

    /* max is respected */
    int best = 0, bx = 0, bz = 0;
    for (int cz = basez - 20; cz < basez + 20 && best < 5; cz++)
        for (int cx = basex - 20; cx < basex + 20 && best < 5; cx++) {
            int n = mtn_flora_ground_cell(&f, t, cx, cz, a, 32);
            if (n > best) {
                best = n;
                bx = cx;
                bz = cz;
            }
        }
    CHECK(best >= 5);
    CHECK(mtn_flora_ground_cell(&f, t, bx, bz, a, 3) == 3);
    CHECK(mtn_flora_ground_cell(&f, t, bx, bz, a, 0) == 0);

    /* a new seed changes the ground cover */
    mtn_flora_ground_cell(&f, t, bx, bz, a, 32);
    f.seed = 10;
    int n2 = mtn_flora_ground_cell(&f, t, bx, bz, b, 32);
    CHECK(n2 != best || !same_items(a, b, best));

    f.seed = 9;

    /* trees only: no ground cover */
    mtn_flora g;
    mtn_flora_init(&g);
    mtn_plant_species s;
    mtn_species_defaults(&s);
    mtn_flora_add_species(&g, &s);
    CHECK(mtn_flora_ground_cell(&g, t, bx, bz, a, 32) == 0);

    /* ground mesh: one upright billboard per item */
    mtn_mesh leaves;
    mtn_mesh_init(&leaves);
    int items = mtn_flora_ground_cell(&f, t, bx, bz, a, 32);
    CHECK(items == best);
    mtn_ground_build(&f, a, items, &leaves);
    CHECK(leaves.vcount == items * 4);
    CHECK(leaves.icount == items * 6);
    for (int i = 0; i < leaves.vcount; i++) {
        const mtn_vertex *v = &leaves.v[i];
        const mtn_ground_item *it = &a[i / 4];
        CHECK(v->extra == it->species);
        CHECK((v->flags & (MTN_VF_LEAF | MTN_VF_UPRIGHT | MTN_VF_WIND | MTN_VF_BILLBOARD)) ==
              (MTN_VF_LEAF | MTN_VF_UPRIGHT | MTN_VF_WIND | MTN_VF_BILLBOARD));
        CHECK(v->ny >= 0.0f); /* anchored on the ground */
        CHECK_NEAR(fabsf(v->nx), it->size * 0.65f, 1e-5);
        CHECK(v->r >= 200);
    }
    mtn_mesh_free(&leaves);
    mtn_flora_free(&f);
}

static float mean_y(const mtn_mesh *m) {
    float s = 0.0f;
    for (int i = 0; i < m->vcount; i++) s += m->v[i].y + (m->v[i].flags & MTN_VF_BILLBOARD ? m->v[i].ny : 0.0f);
    return m->vcount ? s / (float)m->vcount : 0.0f;
}

static void test_tree_meshes(void) {
    mtn_flora f;
    mtn_flora_init(&f);
    add_species(&f);
    mtn_plant_species s;
    mtn_species_defaults(&s);
    strcpy(s.name, "oak");
    s.crown = MTN_CROWN_ROUND;
    int oak = mtn_flora_add_species(&f, &s);
    strcpy(s.name, "poplar");
    s.crown = MTN_CROWN_OVAL;
    int poplar = mtn_flora_add_species(&f, &s);
    strcpy(s.name, "pine");
    s.crown = MTN_CROWN_UMBRELLA;
    int pine = mtn_flora_add_species(&f, &s);
    strcpy(s.name, "cherry");
    s.crown = MTN_CROWN_ROUND;
    s.blossom_tile = 21;
    int cherry = mtn_flora_add_species(&f, &s);
    int spruce = mtn_flora_find_species(&f, "spruce"), juniper = mtn_flora_find_species(&f, "juniper");
    int boulder = mtn_flora_find_species(&f, "boulder"), grass = mtn_flora_find_species(&f, "grass");

    mtn_mesh solid, leaves;
    mtn_mesh_init(&solid);
    mtn_mesh_init(&leaves);

    /* boulders: a rock in the solid mesh, nothing at billboard distance */
    mtn_plant p = plant_at(50, 10, 50, 2.0f, boulder);
    mtn_tree_build(&f, &p, 0, &solid, &leaves);
    CHECK(solid.vcount == 180);
    CHECK(leaves.vcount == 0);
    CHECK(solid.v[0].tile == 255);
    CHECK(solid.v[0].flags == MTN_VF_SNOW);
    vec3 mn, mx;
    mtn_mesh_bounds(&solid, &mn, &mx);
    CHECK(mn.x > 50.0f - 2.0f * 1.2f && mx.x < 50.0f + 2.0f * 1.2f);
    mtn_mesh_clear(&solid);
    mtn_tree_build(&f, &p, 2, &solid, &leaves);
    CHECK(solid.vcount == 0 && leaves.vcount == 0);

    /* bushes and ground plants: 5 / 2 / 1 sprites per LOD */
    const int bush_sprites[3] = {5, 2, 1};
    for (int lod = 0; lod < 3; lod++) {
        mtn_mesh_clear(&leaves);
        p = plant_at(50, 10, 50, 1.0f, lod == 1 ? grass : juniper);
        mtn_tree_build(&f, &p, lod, &solid, &leaves);
        CHECK(solid.vcount == 0);
        CHECK(leaves.vcount == bush_sprites[lod] * 4);
        for (int i = 0; i < leaves.vcount; i++) {
            CHECK(leaves.v[i].extra == p.species);
            CHECK(leaves.v[i].flags & MTN_VF_LEAF);
            CHECK(leaves.v[i].y >= 10.0f && leaves.v[i].y <= 11.0f);
        }
    }

    /* conifer */
    p = plant_at(20, 5, 30, 12.0f, spruce);
    mtn_mesh_clear(&solid);
    mtn_mesh_clear(&leaves);
    mtn_tree_build(&f, &p, 0, &solid, &leaves);
    CHECK(solid.vcount == 7 * 2); /* 6 sided trunk */
    int conifer_sprites = leaves.vcount / 4;
    CHECK(conifer_sprites >= 8 * 3 + 1);
    for (int i = 0; i < leaves.vcount; i++) {
        CHECK(leaves.v[i].tile == 2);
        CHECK((leaves.v[i].flags & (MTN_VF_LEAF | MTN_VF_WIND | MTN_VF_SNOW)) == (MTN_VF_LEAF | MTN_VF_WIND | MTN_VF_SNOW));
        CHECK(leaves.v[i].y > 5.0f && leaves.v[i].y < 5.0f + 12.0f);
    }
    mtn_mesh_bounds(&solid, &mn, &mx);
    CHECK_NEAR(mn.y, 4.6f, 1e-4);
    CHECK_NEAR(mx.y, 5.0f + 12.0f * 0.92f, 1e-4);
    mtn_mesh_clear(&solid);
    mtn_mesh_clear(&leaves);
    mtn_tree_build(&f, &p, 1, &solid, &leaves);
    CHECK(solid.vcount == 5 * 2); /* 4 sided trunk */
    CHECK(leaves.vcount == (3 * 2 + 1) * 4);
    mtn_mesh_clear(&solid);
    mtn_mesh_clear(&leaves);
    mtn_tree_build(&f, &p, 2, &solid, &leaves);
    CHECK(solid.vcount == 0);
    CHECK(leaves.vcount == 2 * 4);
    for (int i = 0; i < leaves.vcount; i++) CHECK(leaves.v[i].flags & MTN_VF_UPRIGHT);

    /* broadleaf crowns */
    float crown_y[3];
    int kinds[3] = {oak, poplar, pine};
    for (int k = 0; k < 3; k++) {
        p = plant_at(0, 0, 0, 10.0f, kinds[k]);
        mtn_mesh_clear(&solid);
        mtn_mesh_clear(&leaves);
        mtn_tree_build(&f, &p, 0, &solid, &leaves);
        int branches = (solid.vcount - 14) / 10;
        CHECK(branches >= 3 && branches <= 5);
        CHECK(solid.vcount == 14 + branches * 10);
        CHECK(leaves.vcount == (12 + 8) * 4);
        crown_y[k] = mean_y(&leaves);
        mtn_mesh_clear(&solid);
        mtn_mesh_clear(&leaves);
        mtn_tree_build(&f, &p, 1, &solid, &leaves);
        CHECK(solid.vcount == 10);
        CHECK(leaves.vcount == 5 * 4);
        mtn_mesh_clear(&solid);
        mtn_mesh_clear(&leaves);
        mtn_tree_build(&f, &p, 2, &solid, &leaves);
        CHECK(solid.vcount == 0);
        CHECK(leaves.vcount == 4);
    }
    CHECK(crown_y[2] > crown_y[0]); /* umbrella crowns sit on top */
    CHECK(crown_y[0] > 3.0f && crown_y[0] < 9.0f);

    /* flowering trees add blossom sprites */
    p = plant_at(0, 0, 0, 10.0f, cherry);
    mtn_mesh_clear(&solid);
    mtn_mesh_clear(&leaves);
    mtn_tree_build(&f, &p, 0, &solid, &leaves);
    CHECK(leaves.vcount == (20 + 10) * 4);
    int blossoms = 0;
    for (int i = 0; i < leaves.vcount; i++) blossoms += (leaves.v[i].flags & MTN_VF_BLOSSOM) && leaves.v[i].tile == 21;
    CHECK(blossoms == 40);

    /* deterministic */
    mtn_mesh again;
    mtn_mesh_init(&again);
    mtn_tree_build(&f, &p, 0, &again, &again);
    CHECK(again.vcount == solid.vcount + leaves.vcount);
    CHECK(memcmp(again.v, solid.v, sizeof(mtn_vertex) * (size_t)solid.vcount) == 0);
    mtn_mesh_free(&again);
    mtn_mesh_free(&solid);
    mtn_mesh_free(&leaves);
}

static void test_hide_and_chunks(void) {
    mtn_terrain t;
    flat_terrain(&t, 33, 8.0f, 10.0f, MTN_BIOME_ALPINE);
    mtn_flora f;
    mtn_flora_init(&f);
    add_species(&f);
    mtn_flora_populate(&f, &t, 3);
    int trees = 0, others = 0;
    for (int i = 0; i < f.count; i++) {
        if (f.species[f.plants[i].species].kind == MTN_PLANT_TREE) trees++;
        else others++;
    }
    CHECK(trees == 0); /* no tree species for alpine terrain */
    CHECK(others > 50);
    mtn_flora_free(&f);
    mtn_terrain_free(&t);

    flat_terrain(&t, 33, 8.0f, 10.0f, MTN_BIOME_FOREST);
    mtn_plant_species s;
    mtn_species_defaults(&s);
    strcpy(s.name, "rock");
    s.kind = MTN_PLANT_BOULDER;
    s.height_min = 0.5f;
    s.height_max = 1.5f;
    s.density = 0.5f;
    mtn_flora_add_species(&f, &s);
    mtn_flora_populate(&f, &t, 3);
    CHECK(f.count > 100);
    int spruce = mtn_flora_find_species(&f, "spruce");

    /* chunk building */
    mtn_mesh solid, leaves;
    mtn_mesh_init(&solid);
    mtn_mesh_init(&leaves);
    int c = mtn_flora_chunk_of(&f, t.center.x, t.center.z);
    CHECK(f.chunk_count[c] > 0);
    mtn_flora_chunk_build(&f, c, 1, &solid, &leaves);
    int full_solid = solid.vcount, full_leaves = leaves.vcount;
    CHECK(full_solid > 0 && full_leaves > 0);
    mtn_mesh_clear(&solid);
    mtn_mesh_clear(&leaves);
    mtn_flora_chunk_build(&f, -1, 1, &solid, &leaves);
    mtn_flora_chunk_build(&f, f.chunks * f.chunks, 1, &solid, &leaves);
    CHECK(solid.vcount == 0 && leaves.vcount == 0);

    /* hiding plants around a house plot */
    vec3 plot = t.center;
    int keep = mtn_flora_nearest_tree(&f, plot, 50.0f, 0.0f);
    CHECK(keep >= 0);
    float keep_d = v3_dist_xz(f.plants[keep].pos, plot);
    for (int i = 0; i < f.count; i++) {
        const mtn_plant *q = &f.plants[i];
        if (f.species[q->species].kind == MTN_PLANT_TREE) CHECK(v3_dist_xz(q->pos, plot) >= keep_d);
    }
    int version_before = f.chunk_version[c];
    int expected = 0;
    for (int i = 0; i < f.count; i++) {
        const mtn_plant *q = &f.plants[i];
        float pr = f.species[q->species].kind == MTN_PLANT_TREE ? 0.0f : q->scale * 0.5f;
        if (i != keep && v3_dist_xz(q->pos, plot) < 20.0f + pr) expected++;
    }
    int hidden = mtn_flora_hide_radius(&f, plot, 20.0f, keep);
    CHECK(hidden == expected);
    CHECK(hidden > 5);
    CHECK(f.chunk_version[c] > version_before);
    CHECK(!f.plants[keep].hidden);
    CHECK(mtn_flora_hide_radius(&f, plot, 20.0f, keep) == 0); /* already hidden */
    int now_hidden = 0;
    for (int i = 0; i < f.count; i++) now_hidden += f.plants[i].hidden;
    CHECK(now_hidden == hidden);

    /* hidden plants are skipped by chunk building, the nearest tree search and collision */
    mtn_flora_chunk_build(&f, c, 1, &solid, &leaves);
    CHECK(solid.vcount + leaves.vcount < full_solid + full_leaves);
    /* a hidden spruce near the plot centre, away from the kept tree */
    int hidden_tree = -1;
    float best_d = 1e9f;
    for (int i = 0; i < f.count; i++) {
        float d = v3_dist_xz(f.plants[i].pos, plot);
        if (f.plants[i].hidden && f.plants[i].species == spruce && d < best_d &&
            v3_dist_xz(f.plants[i].pos, f.plants[keep].pos) > 3.0f) {
            best_d = d;
            hidden_tree = i;
        }
    }
    CHECK(hidden_tree >= 0 && best_d < 12.0f);
    vec3 hp = f.plants[hidden_tree].pos;
    int near = mtn_flora_nearest_tree(&f, hp, 50.0f, 0.0f);
    CHECK(near == keep); /* every other tree around is hidden */
    vec3 probe = v3(hp.x + 0.05f, hp.y, hp.z);
    CHECK(mtn_flora_collide(&f, &probe, 0.35f) == 0);
    CHECK_NEAR(probe.x, hp.x + 0.05f, 0);
    vec3 at_keep = v3(f.plants[keep].pos.x + 0.05f, f.plants[keep].pos.y, f.plants[keep].pos.z);
    CHECK(mtn_flora_collide(&f, &at_keep, 0.35f) == 1); /* the kept tree still blocks */

    /* nearest tree filters */
    CHECK(mtn_flora_nearest_tree(&f, plot, 0.001f, 0.0f) == -1);
    CHECK(mtn_flora_nearest_tree(&f, plot, 500.0f, 1000.0f) == -1);

    int old_version = f.chunk_version[c];
    mtn_flora_unhide_all(&f);
    for (int i = 0; i < f.count; i++) CHECK(!f.plants[i].hidden);
    CHECK(f.chunk_version[c] > old_version);
    mtn_mesh_clear(&solid);
    mtn_mesh_clear(&leaves);
    mtn_flora_chunk_build(&f, c, 1, &solid, &leaves);
    CHECK(solid.vcount == full_solid && leaves.vcount == full_leaves);

    mtn_mesh_free(&solid);
    mtn_mesh_free(&leaves);
    mtn_flora_free(&f);
    mtn_terrain_free(&t);
}

static void test_collide(void) {
    mtn_flora f;
    mtn_flora_init(&f);
    vec3 pos = v3(10, 0, 10);
    CHECK(mtn_flora_collide(&f, &pos, 0.35f) == 0); /* not populated */

    mtn_plant_species s;
    mtn_species_defaults(&s);
    s.trunk_radius = 0.3f;
    int tree = mtn_flora_add_species(&f, &s);
    s.kind = MTN_PLANT_BOULDER;
    int rock = mtn_flora_add_species(&f, &s);
    s.kind = MTN_PLANT_BUSH;
    int bush = mtn_flora_add_species(&f, &s);
    mtn_plant pl[6] = {
        plant_at(20, 0, 20, 10.0f, tree),   /* trunk radius 0.3 + 0.1 */
        plant_at(40, 0, 20, 2.0f, rock),    /* radius 1.7 */
        plant_at(60, 0, 20, 2.0f, bush),    /* no collision */
        plant_at(96.3f, 0, 20, 10.0f, tree), /* just across the chunk border */
        plant_at(20, 0, 60, 10.0f, tree),   /* hidden below */
        plant_at(150, 0, 150, 10.0f, tree), /* far away chunk */
    };
    pl[4].hidden = 1;
    flora_from_plants(&f, pl, 6, 2);
    CHECK(f.chunk_count[0] == 4 && f.chunk_count[1] == 1 && f.chunk_count[3] == 1);

    /* pushed out of a trunk to exactly the contact distance */
    pos = v3(20.2f, 1.0f, 20.0f);
    CHECK(mtn_flora_collide(&f, &pos, 0.35f) == 1);
    CHECK_NEAR(v3_dist_xz(pos, v3(20, 0, 20)), 0.4f + 0.35f, 1e-5);
    CHECK_NEAR(pos.z, 20.0f, 1e-6); /* along the contact direction */
    CHECK(pos.x > 20.2f);
    CHECK_NEAR(pos.y, 1.0f, 0);
    /* dead centre: pushed along +x */
    pos = v3(20.0f, 0.0f, 20.0f);
    CHECK(mtn_flora_collide(&f, &pos, 0.35f) == 1);
    CHECK_NEAR(pos.x, 20.75f, 1e-5);
    CHECK_NEAR(pos.z, 20.0f, 1e-6);
    /* no contact once clear, or when standing above the tree top */
    pos = v3(21.0f, 0.0f, 20.0f);
    CHECK(mtn_flora_collide(&f, &pos, 0.35f) == 0);
    CHECK_NEAR(pos.x, 21.0f, 0);
    pos = v3(20.1f, 10.5f, 20.0f);
    CHECK(mtn_flora_collide(&f, &pos, 0.35f) == 0);

    /* boulders are wide but low */
    pos = v3(40.0f, 0.5f, 21.0f);
    CHECK(mtn_flora_collide(&f, &pos, 0.35f) == 1);
    CHECK_NEAR(v3_dist_xz(pos, v3(40, 0, 20)), 1.7f + 0.35f, 1e-5);
    pos = v3(40.0f, 1.9f, 21.0f); /* climbed on top */
    CHECK(mtn_flora_collide(&f, &pos, 0.35f) == 0);

    /* bushes can be walked through */
    pos = v3(60.1f, 0.0f, 20.0f);
    CHECK(mtn_flora_collide(&f, &pos, 0.35f) == 0);

    /* trunks in the neighbouring chunk still block */
    pos = v3(95.9f, 0.0f, 20.0f);
    CHECK(mtn_flora_chunk_of(&f, pos.x, pos.z) == 0);
    CHECK(mtn_flora_collide(&f, &pos, 0.35f) == 1);
    CHECK_NEAR(pos.x, 96.3f - 0.75f, 1e-4);
    pos = v3(96.6f, 0.0f, 20.0f);
    CHECK(mtn_flora_chunk_of(&f, pos.x, pos.z) == 1);
    CHECK(mtn_flora_collide(&f, &pos, 0.35f) == 1);
    CHECK_NEAR(pos.x, 96.3f + 0.75f, 1e-4);
    pos = v3(95.0f, 0.0f, 20.0f);
    CHECK(mtn_flora_collide(&f, &pos, 0.35f) == 0);

    /* hidden trees do not block */
    pos = v3(20.1f, 0.0f, 60.0f);
    CHECK(mtn_flora_collide(&f, &pos, 0.35f) == 0);
    /* far corner chunk */
    pos = v3(150.2f, 0.0f, 150.0f);
    CHECK(mtn_flora_collide(&f, &pos, 0.35f) == 1);

    /* hiding by radius: boulders count their size, trees only their trunk position */
    CHECK(mtn_flora_hide_radius(&f, v3(38.5f, 0, 20.0f), 0.6f, -1) == 1); /* boulder edge within reach */
    CHECK(mtn_flora_hide_radius(&f, v3(21.0f, 0, 20.0f), 0.6f, -1) == 0); /* trunk 1 m away */
    CHECK(f.chunk_version[0] == 1);
    pos = v3(40.0f, 0.5f, 21.0f);
    CHECK(mtn_flora_collide(&f, &pos, 0.35f) == 0);
    mtn_flora_free(&f);
}

MTN_SUITE(flora) {
    mtn_terrain_params p;
    mtn_terrain_default_params(&p);
    p.seed = 11;
    p.size = 129;
    p.cell = 16.0f;
    p.erosion_drops = 3000;
    mtn_terrain_generate(&g_terrain, &p);
    TEST_RUN(test_species);
    TEST_RUN(test_habitat);
    TEST_RUN(test_chunk_of);
    TEST_RUN(test_populate);
    TEST_RUN(test_populate_flat);
    TEST_RUN(test_season);
    TEST_RUN(test_ground_cell);
    TEST_RUN(test_tree_meshes);
    TEST_RUN(test_hide_and_chunks);
    TEST_RUN(test_collide);
    mtn_terrain_free(&g_terrain);
}
