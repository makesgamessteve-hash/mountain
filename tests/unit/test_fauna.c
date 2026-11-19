#include "test.h"
#include "mtn/fauna.h"

#include <stdlib.h>

/* ------------------------------------------------------------------ fixtures */

#define N 129
#define CELL 4.0f
#define GROUND 10.0f
/* lake: grid x 80..90, z 60..70 (world x 320..360, z 240..280) */
#define LAKE_X0 80
#define LAKE_X1 90
#define LAKE_Z0 60
#define LAKE_Z1 70
/* steep wall: grid x 20..30, z 60..70 (world x 80..120) */
#define WALL_X0 20
#define WALL_X1 30

/* 512 m flat meadow at altitude 0.1, playable radius ~210 m around (256, 256). */
static void make_terrain(mtn_terrain *t, int lake, int wall) {
    memset(t, 0, sizeof *t);
    mtn_terrain_default_params(&t->p);
    t->n = N;
    t->cell = CELL;
    t->world_size = (float)(N - 1) * CELL;
    t->center = v3(t->world_size * 0.5f, GROUND, t->world_size * 0.5f);
    size_t nn = (size_t)N * N;
    t->h = malloc(sizeof(float) * nn);
    t->water = malloc(sizeof(float) * nn);
    t->moist = malloc(sizeof(float) * nn);
    t->trail_dist = malloc(sizeof(float) * nn);
    t->biome = malloc(nn);
    for (int z = 0; z < N; z++)
        for (int x = 0; x < N; x++) {
            int i = z * N + x;
            t->h[i] = GROUND;
            t->water[i] = MTN_NO_WATER;
            t->moist[i] = 0.5f;
            t->trail_dist[i] = 1000.0f;
            t->biome[i] = MTN_BIOME_MEADOW;
            if (lake && x >= LAKE_X0 && x <= LAKE_X1 && z >= LAKE_Z0 && z <= LAKE_Z1) {
                t->water[i] = GROUND + 2.0f;
                t->biome[i] = MTN_BIOME_WATER;
            }
            if (wall && x >= WALL_X0 && x <= WALL_X1 && z >= LAKE_Z0 && z <= LAKE_Z1) {
                t->h[i] = GROUND + (float)(x - WALL_X0) * CELL * 3.0f;
                t->biome[i] = MTN_BIOME_ROCK;
            }
        }
    t->min_h = 0.0f;
    t->max_h = 100.0f;
}

static mtn_season_blend pure(mtn_season s) {
    mtn_season_blend b;
    memset(&b, 0, sizeof b);
    b.w[s] = 1.0f;
    b.dominant = s;
    return b;
}

static void new_fauna(mtn_fauna *f, uint32_t seed) {
    memset(f, 0, sizeof *f);
    mtn_fauna_init(f, seed);
}

static mtn_animal_species deer(void) {
    mtn_animal_species s;
    mtn_animal_defaults(&s);
    strcpy(s.name, "deer");
    s.horns = 1;
    strcpy(s.sound, "deer_bark");
    s.group_min = s.group_max = 1;
    return s;
}

static mtn_animal_species flyer(const char *name, int kind) {
    mtn_animal_species s;
    mtn_animal_defaults(&s);
    strcpy(s.name, name);
    s.kind = kind;
    s.size = kind == MTN_ANIMAL_SOARING ? 2.0f : 0.3f;
    s.group_min = s.group_max = 1;
    return s;
}

static int alive_index(const mtn_fauna *f, int nth) {
    for (int i = 0; i < MTN_MAX_ANIMALS; i++)
        if (f->animals[i].alive && nth-- == 0) return i;
    return -1;
}

static int group_count(const mtn_fauna *f, int species) {
    int seen[MTN_MAX_ANIMALS], n = 0;
    for (int i = 0; i < MTN_MAX_ANIMALS; i++) {
        const mtn_animal *a = &f->animals[i];
        if (!a->alive || a->species != species) continue;
        int dup = 0;
        for (int k = 0; k < n; k++) dup |= seen[k] == a->group;
        if (!dup) seen[n++] = a->group;
    }
    return n;
}

/* ------------------------------------------------------------------ tests */

static void test_species(void) {
    mtn_fauna f;
    new_fauna(&f, 1);
    CHECK(f.species_count == 0);
    mtn_animal_species s;
    mtn_animal_defaults(&s);
    CHECK_STR(s.name, "animal");
    CHECK(s.kind == MTN_ANIMAL_QUADRUPED);
    CHECK(s.season_mask == 15);
    CHECK(s.biome_mask == 0xFE);
    CHECK(s.group_min <= s.group_max);
    CHECK(s.sound[0] == 0);
    for (int i = 0; i < MTN_MAX_ANIMAL_SPECIES; i++) CHECK(mtn_fauna_add_species(&f, &s) == i);
    CHECK(mtn_fauna_add_species(&f, &s) == -1);
    CHECK(f.species_count == MTN_MAX_ANIMAL_SPECIES);

    /* re-initialising keeps the species but forgets the animals */
    f.animals[3].alive = 1;
    f.next_group = 7;
    f.call_count = 2;
    mtn_fauna_init(&f, 2);
    CHECK(f.species_count == MTN_MAX_ANIMAL_SPECIES);
    CHECK_STR(f.species[5].name, "animal");
    CHECK(!f.animals[3].alive);
    CHECK(f.next_group == 0 && f.call_count == 0);
    f.animals[0].alive = 1;
    f.call_count = 1;
    mtn_fauna_clear(&f);
    CHECK(mtn_fauna_count(&f, -1) == 0);
    CHECK(f.call_count == 0);
    CHECK(f.species_count == MTN_MAX_ANIMAL_SPECIES);
}

static void test_active(void) {
    mtn_animal_species s;
    mtn_animal_defaults(&s);
    s.hour_start = 6.0f;
    s.hour_end = 20.0f;
    CHECK(mtn_fauna_active(&s, 12.0f, pure(MTN_SUMMER)));
    CHECK(mtn_fauna_active(&s, 6.0f, pure(MTN_SUMMER)));
    CHECK(!mtn_fauna_active(&s, 22.0f, pure(MTN_SUMMER)));
    CHECK(!mtn_fauna_active(&s, 3.0f, pure(MTN_SUMMER)));
    /* nocturnal: wraps past midnight */
    s.hour_start = 20.0f;
    s.hour_end = 5.0f;
    CHECK(mtn_fauna_active(&s, 23.0f, pure(MTN_SUMMER)));
    CHECK(mtn_fauna_active(&s, 2.0f, pure(MTN_SUMMER)));
    CHECK(!mtn_fauna_active(&s, 12.0f, pure(MTN_SUMMER)));
    /* seasons */
    s.hour_start = 0.0f;
    s.hour_end = 24.0f;
    s.season_mask = (1 << MTN_SPRING) | (1 << MTN_SUMMER);
    CHECK(mtn_fauna_active(&s, 12.0f, pure(MTN_SPRING)));
    CHECK(!mtn_fauna_active(&s, 12.0f, pure(MTN_WINTER)));
    mtn_season_blend b = pure(MTN_SUMMER);
    b.w[MTN_SUMMER] = 0.5f;
    b.w[MTN_AUTUMN] = 0.5f;
    CHECK(mtn_fauna_active(&s, 12.0f, b));
    b.w[MTN_SUMMER] = 0.4f;
    b.w[MTN_AUTUMN] = 0.6f;
    CHECK(!mtn_fauna_active(&s, 12.0f, b));
}

static void test_spawn_group(void) {
    mtn_terrain t;
    make_terrain(&t, 1, 0);
    mtn_fauna f;
    new_fauna(&f, 3);
    mtn_animal_species s = deer();
    s.group_min = 2;
    s.group_max = 4;
    s.alt_max = 0.5f;
    int d = mtn_fauna_add_species(&f, &s);
    vec3 at = t.center;

    CHECK(mtn_fauna_spawn_group(&f, &t, -1, at) == 0);
    CHECK(mtn_fauna_spawn_group(&f, &t, 1, at) == 0);
    CHECK(mtn_fauna_spawn_group(&f, &t, d, v3(5, 0, 5)) == 0);       /* outside the playable disc */
    CHECK(mtn_fauna_spawn_group(&f, &t, d, v3(340, 0, 260)) == 0);   /* in the lake */
    CHECK(mtn_fauna_count(&f, -1) == 0);

    int n = mtn_fauna_spawn_group(&f, &t, d, at);
    CHECK(n >= 2 && n <= 4);
    CHECK(mtn_fauna_count(&f, d) == n);
    CHECK(mtn_fauna_count(&f, -1) == n);
    CHECK(mtn_fauna_count(&f, 5) == 0);
    CHECK(group_count(&f, d) == 1);
    for (int k = 0; k < n; k++) {
        const mtn_animal *a = &f.animals[alive_index(&f, k)];
        CHECK(a->species == d);
        CHECK(a->group == 1);
        CHECK(a->state == MTN_AS_GRAZE);
        CHECK_NEAR(a->fade, 0.0f, 0);
        CHECK(fabsf(a->pos.x - at.x) <= 6.0f && fabsf(a->pos.z - at.z) <= 6.0f);
        CHECK_NEAR(a->pos.y, GROUND, 1e-4);
        CHECK_NEAR(a->home.x, at.x, 0);
        CHECK_NEAR(a->home.y, 0.0f, 0);
        CHECK(a->timer >= 2.0f && a->timer <= 8.0f);
    }
    CHECK(mtn_fauna_spawn_group(&f, &t, d, at) > 0);
    CHECK(group_count(&f, d) == 2);

    /* biome and altitude limits */
    mtn_animal_species picky = s;
    picky.biome_mask = 1 << MTN_BIOME_FOREST;
    int pk = mtn_fauna_add_species(&f, &picky);
    CHECK(mtn_fauna_spawn_group(&f, &t, pk, at) == 0);
    mtn_animal_species high = s;
    high.alt_min = 0.5f;
    int hi = mtn_fauna_add_species(&f, &high);
    CHECK(mtn_fauna_spawn_group(&f, &t, hi, at) == 0);

    /* flying kinds start in the air at their typical height */
    struct { int kind; float lo, hi; } fl[4] = {
        {MTN_ANIMAL_BIRD, 14, 32}, {MTN_ANIMAL_SOARING, 90, 140}, {MTN_ANIMAL_BUTTERFLY, 0.5f, 1.6f},
        {MTN_ANIMAL_FIREFLY, 0.4f, 2.5f}};
    for (int k = 0; k < 4; k++) {
        int si = mtn_fauna_add_species(&f, &(mtn_animal_species){0});
        f.species[si] = flyer("flyer", fl[k].kind);
        CHECK(mtn_fauna_spawn_group(&f, &t, si, at) == 1);
        const mtn_animal *a = NULL;
        for (int i = 0; i < MTN_MAX_ANIMALS; i++)
            if (f.animals[i].alive && f.animals[i].species == si) a = &f.animals[i];
        CHECK(a != NULL);
        if (!a) continue;
        CHECK(a->state == MTN_AS_FLY);
        CHECK(a->home.y >= fl[k].lo && a->home.y <= fl[k].hi);
        CHECK_NEAR(a->pos.y, GROUND + a->home.y, 1e-4);
        CHECK(fabsf(a->pos.x - at.x) <= 3.0f);
    }

    /* the animal table fills up and then refuses more */
    mtn_fauna_clear(&f);
    int total = 0, made;
    while ((made = mtn_fauna_spawn_group(&f, &t, d, at)) > 0) total += made;
    CHECK(total == MTN_MAX_ANIMALS);
    CHECK(mtn_fauna_count(&f, -1) == MTN_MAX_ANIMALS);
    mtn_terrain_free(&t);
}

static void test_spawning(void) {
    mtn_terrain t;
    make_terrain(&t, 0, 0);
    vec3 player = t.center;
    mtn_season_blend su = pure(MTN_SUMMER);

    /* nothing to spawn without species */
    mtn_fauna f;
    new_fauna(&f, 4);
    mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.1f);
    CHECK(mtn_fauna_count(&f, -1) == 0);
    CHECK_NEAR(f.spawn_timer, 1.5f, 0);

    /* a sure spawn happens at the species' spawn distance */
    mtn_animal_species s = deer();
    s.spawn_chance = 1.0f;
    s.max_groups = 2;
    s.hour_start = 6.0f;
    s.hour_end = 20.0f;
    s.comfort_dist = 1.0f; /* tame, so they stay where they are */
    int d = mtn_fauna_add_species(&f, &s);
    f.spawn_timer = 0.0f;
    mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.1f);
    CHECK(mtn_fauna_count(&f, d) == 1);
    const mtn_animal *a = &f.animals[alive_index(&f, 0)];
    float dist = v3_dist_xz(a->home, player);
    CHECK(dist >= 45.0f && dist <= 140.0f);
    /* the timer gates further attempts */
    mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.1f);
    CHECK(mtn_fauna_count(&f, d) == 1);
    /* never more than max_groups */
    for (int i = 0; i < 40; i++) mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 1.5f);
    CHECK(group_count(&f, d) == 2);

    /* not at night */
    mtn_fauna_clear(&f);
    f.spawn_timer = 0.0f;
    for (int i = 0; i < 20; i++) mtn_fauna_update(&f, &t, player, 23.0f, su, 0.0f, 1.5f);
    CHECK(mtn_fauna_count(&f, -1) == 0);

    /* shy species rarely show up */
    f.species[d].spawn_chance = 0.0f;
    for (int i = 0; i < 20; i++) mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 1.5f);
    CHECK(mtn_fauna_count(&f, -1) == 0);

    /* butterflies and fireflies hide from the rain */
    mtn_fauna g;
    new_fauna(&g, 5);
    mtn_animal_species b = flyer("butterfly", MTN_ANIMAL_BUTTERFLY);
    b.spawn_chance = 1.0f;
    int bi = mtn_fauna_add_species(&g, &b);
    for (int i = 0; i < 10; i++) mtn_fauna_update(&g, &t, player, 12.0f, su, 0.6f, 1.5f);
    CHECK(mtn_fauna_count(&g, -1) == 0);
    mtn_fauna_update(&g, &t, player, 12.0f, su, 0.0f, 1.5f);
    CHECK(mtn_fauna_count(&g, bi) == 1);
    a = &g.animals[alive_index(&g, 0)];
    dist = v3_dist_xz(a->home, player);
    CHECK(dist >= 6.0f && dist <= 30.0f);
    mtn_terrain_free(&t);
}

/* Each kind appears at its own distance from the player. */
static void check_spawn_distance(const mtn_terrain *t, int kind, float lo, float hi) {
    mtn_fauna f;
    new_fauna(&f, 21u + (uint32_t)kind);
    mtn_animal_species s = flyer("x", kind);
    s.spawn_chance = 1.0f;
    s.max_groups = 1;
    mtn_fauna_add_species(&f, &s);
    vec3 player = t->center;
    for (int i = 0; i < 30 && mtn_fauna_count(&f, -1) == 0; i++)
        mtn_fauna_update(&f, t, player, 12.0f, pure(MTN_SUMMER), 0.0f, 1.5f);
    CHECK(mtn_fauna_count(&f, -1) == 1);
    int i = alive_index(&f, 0);
    if (i < 0) return;
    float d = v3_dist_xz(f.animals[i].home, player);
    CHECK(d >= lo - 1e-3f && d <= hi + 1e-3f);
    CHECK(mtn_terrain_in_playable(t, f.animals[i].home.x, f.animals[i].home.z));
}

static void test_spawn_distances(void) {
    mtn_terrain t;
    make_terrain(&t, 0, 0);
    check_spawn_distance(&t, MTN_ANIMAL_QUADRUPED, 45.0f, 140.0f);
    check_spawn_distance(&t, MTN_ANIMAL_BIRD, 60.0f, 180.0f);
    check_spawn_distance(&t, MTN_ANIMAL_SOARING, 120.0f, 250.0f);
    check_spawn_distance(&t, MTN_ANIMAL_BUTTERFLY, 6.0f, 30.0f);
    check_spawn_distance(&t, MTN_ANIMAL_FIREFLY, 4.0f, 25.0f);
    mtn_terrain_free(&t);
}

static void test_fade_and_despawn(void) {
    mtn_terrain t;
    make_terrain(&t, 0, 0);
    mtn_season_blend su = pure(MTN_SUMMER);
    mtn_fauna f;
    new_fauna(&f, 6);
    mtn_animal_species s = deer();
    s.comfort_dist = 1.0f;
    s.hour_start = 6.0f;
    s.hour_end = 20.0f;
    int d = mtn_fauna_add_species(&f, &s);
    vec3 player = v3(t.center.x + 100.0f, 0, t.center.z);
    CHECK(mtn_fauna_spawn_group(&f, &t, d, t.center) == 1);
    int i = alive_index(&f, 0);
    f.spawn_timer = 1e9f;

    /* fades in over ~1.25 s */
    mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.5f);
    CHECK_NEAR(f.animals[i].fade, 0.4f, 1e-5);
    for (int k = 0; k < 4; k++) mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.5f);
    CHECK_NEAR(f.animals[i].fade, 1.0f, 0);
    CHECK(!f.animals[i].leaving);

    /* walking away: it fades out and is gone */
    player = v3(-2000.0f, 0, -2000.0f);
    mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.5f);
    CHECK(f.animals[i].leaving);
    CHECK(f.animals[i].alive);
    CHECK_NEAR(f.animals[i].fade, 0.6f, 1e-5);
    for (int k = 0; k < 3; k++) mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.5f);
    CHECK(!f.animals[i].alive);
    CHECK(mtn_fauna_count(&f, -1) == 0);

    /* evening: diurnal animals leave even when close */
    player = t.center;
    CHECK(mtn_fauna_spawn_group(&f, &t, d, t.center) == 1);
    i = alive_index(&f, 0);
    mtn_fauna_update(&f, &t, player, 22.0f, su, 0.0f, 0.1f);
    CHECK(f.animals[i].leaving);
    CHECK(!f.animals[i].alive); /* never faded in */

    /* rain sends butterflies away */
    mtn_animal_species b = flyer("butterfly", MTN_ANIMAL_BUTTERFLY);
    int bi = mtn_fauna_add_species(&f, &b);
    CHECK(mtn_fauna_spawn_group(&f, &t, bi, t.center) == 1);
    i = alive_index(&f, 0);
    mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 1.0f);
    CHECK(!f.animals[i].leaving);
    mtn_fauna_update(&f, &t, player, 12.0f, su, 0.8f, 0.1f);
    CHECK(f.animals[i].leaving);
    mtn_terrain_free(&t);
}

static void test_flee(void) {
    mtn_terrain t;
    make_terrain(&t, 0, 0);
    mtn_season_blend su = pure(MTN_SUMMER);
    mtn_fauna f;
    new_fauna(&f, 7);
    mtn_animal_species s = deer();
    int d = mtn_fauna_add_species(&f, &s);
    CHECK(mtn_fauna_spawn_group(&f, &t, d, t.center) == 1);
    mtn_animal *a = &f.animals[alive_index(&f, 0)];
    f.spawn_timer = 1e9f;
    vec3 player = v3(a->pos.x - 5.0f, 0, a->pos.z + 2.0f);

    mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.1f);
    CHECK(a->state == MTN_AS_FLEE);
    CHECK_STR(mtn_fauna_pop_call(&f), "deer_bark");
    CHECK(v3_dist_xz(f.last_call_pos, f.animals[alive_index(&f, 0)].pos) < 10.0f); /* the call comes from the animal */
    CHECK_STR(mtn_fauna_pop_call(&f), "");
    CHECK_NEAR(v3_len(a->vel), s.run_speed * 0.6f, 1e-4);

    float prev = v3_dist_xz(a->pos, player);
    int steps = 0;
    while (a->state == MTN_AS_FLEE && steps < 400) {
        mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.1f);
        float dnow = v3_dist_xz(a->pos, player);
        if (a->state == MTN_AS_FLEE) CHECK(dnow > prev); /* always moving away */
        else CHECK_NEAR(dnow, prev, 1e-6);                /* stops where it settles */
        prev = dnow;
        CHECK_NEAR(a->pos.y, GROUND, 1e-4);
        steps++;
    }
    CHECK(a->state == MTN_AS_IDLE);
    CHECK(steps > 20);
    /* it keeps (more than) its comfort distance and settles there */
    CHECK(v3_dist_xz(a->pos, player) > s.comfort_dist * 1.6f);
    CHECK_NEAR(a->home.x, a->pos.x, 1e-3);
    CHECK_NEAR(a->home.z, a->pos.z, 1e-3);
    /* and faces away from the player */
    vec3 fwd = v3(sinf(a->yaw), 0, -cosf(a->yaw));
    vec3 away = v3_norm(v3(a->pos.x - player.x, 0, a->pos.z - player.z));
    CHECK(v3_dot(fwd, away) > 0.95f);
    /* once settled it stands still until it decides to do something else */
    vec3 settled = a->pos;
    for (int i = 0; i < 15; i++) mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.1f);
    CHECK(a->state == MTN_AS_IDLE);
    CHECK_NEAR(v3_dist_xz(a->pos, settled), 0.0f, 1e-6);
    mtn_terrain_free(&t);
}

static void test_wander(void) {
    mtn_terrain t;
    make_terrain(&t, 0, 0);
    mtn_season_blend su = pure(MTN_SUMMER);
    mtn_fauna f;
    new_fauna(&f, 8);
    mtn_animal_species s = deer();
    s.comfort_dist = 5.0f;
    int d = mtn_fauna_add_species(&f, &s);
    CHECK(mtn_fauna_spawn_group(&f, &t, d, t.center) == 1);
    mtn_animal *a = &f.animals[alive_index(&f, 0)];
    f.spawn_timer = 1e9f;
    vec3 player = v3(t.center.x, 0, t.center.z + 150.0f);
    int seen[5] = {0}, calls = 0, approach_ok = 1;
    for (int i = 0; i < 1500; i++) {
        int was = a->state;
        float before = v3_dist_xz(a->pos, a->home);
        mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.25f);
        seen[a->state]++;
        if (was == MTN_AS_WALK && a->state == MTN_AS_WALK && v3_dist_xz(a->pos, a->home) >= before) approach_ok = 0;
        if (a->state == MTN_AS_WALK) CHECK(mtn_terrain_in_playable(&t, a->home.x, a->home.z));
        if (a->state == MTN_AS_GRAZE || a->state == MTN_AS_IDLE) CHECK_NEAR(v3_len(a->vel), 0.0f, 0);
        CHECK(f.call_count <= 4);
        while (mtn_fauna_pop_call(&f)[0]) calls++;
    }
    CHECK(seen[MTN_AS_GRAZE] > 0);
    CHECK(seen[MTN_AS_IDLE] > 0);
    CHECK(seen[MTN_AS_WALK] > 0);
    CHECK(seen[MTN_AS_FLEE] == 0);
    CHECK(approach_ok);
    CHECK(calls > 0);
    CHECK(a->alive);
    CHECK(a->anim > 0.0f);

    /* walk targets outside the habitat are refused */
    f.species[d].biome_mask = 1 << MTN_BIOME_FOREST;
    a->state = MTN_AS_GRAZE;
    a->home = a->pos;
    for (int i = 0; i < 400; i++) {
        mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.25f);
        CHECK(a->state != MTN_AS_WALK);
        while (mtn_fauna_pop_call(&f)[0]) {}
    }
    /* a silent species never calls */
    f.species[d].biome_mask = 0xFE;
    f.species[d].sound[0] = 0;
    for (int i = 0; i < 400; i++) {
        mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.25f);
        CHECK(f.call_count == 0);
    }
    mtn_terrain_free(&t);
}

static void test_obstacles(void) {
    mtn_terrain t;
    make_terrain(&t, 1, 1);
    mtn_season_blend su = pure(MTN_SUMMER);
    mtn_fauna f;
    new_fauna(&f, 9);
    mtn_animal_species s = deer();
    int d = mtn_fauna_add_species(&f, &s);
    f.spawn_timer = 1e9f;
    vec3 player = v3(t.center.x, 0, t.center.z - 150.0f);
    float lake_w = (float)(LAKE_X0 - 1) * CELL; /* any point east of here touches the lake */
    float zmid = (float)(LAKE_Z0 + LAKE_Z1) * 0.5f * CELL;

    /* walking into the lake: stops and idles */
    CHECK(mtn_fauna_spawn_group(&f, &t, d, v3(lake_w - 20.0f, 0, zmid)) == 1);
    mtn_animal *a = &f.animals[alive_index(&f, 0)];
    a->pos = v3(lake_w - 0.5f, GROUND, zmid);
    a->state = MTN_AS_WALK;
    a->home = v3(lake_w + 60.0f, 0, zmid);
    a->timer = 100.0f;
    mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 1.0f);
    CHECK(a->state == MTN_AS_IDLE);
    CHECK_NEAR(a->timer, 1.0f, 0);
    CHECK_NEAR(a->pos.x, lake_w - 0.5f, 0);
    /* ... and later carries on with its life instead of staring at the water */
    int resumed = 0;
    for (int i = 0; i < 120 && !resumed; i++) {
        mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.25f);
        resumed = a->state != MTN_AS_IDLE;
    }
    CHECK(resumed);
    CHECK(mtn_terrain_water(&t, a->pos.x, a->pos.z) == MTN_NO_WATER);
    a->pos = v3(lake_w - 0.5f, GROUND, zmid);

    /* fleeing into the lake: detours along the shore instead of swimming */
    a->state = MTN_AS_FLEE;
    a->timer = 5.0f;
    a->phase = 0.0f;
    vec3 near_player = v3(a->pos.x - 4.0f, 0, zmid);
    mtn_fauna_update(&f, &t, near_player, 12.0f, su, 0.0f, 0.5f);
    CHECK(a->state == MTN_AS_FLEE);
    CHECK_NEAR(a->pos.x, lake_w - 0.5f, 0); /* blocked this frame */
    CHECK_NEAR(a->phase, 0.8f, 1e-6);       /* next try turns further around */
    CHECK(mtn_terrain_water(&t, a->pos.x, a->pos.z) == MTN_NO_WATER);
    vec3 before = a->pos;
    for (int i = 0; i < 20; i++) {
        mtn_fauna_update(&f, &t, near_player, 12.0f, su, 0.0f, 0.25f);
        CHECK(mtn_terrain_water(&t, a->pos.x, a->pos.z) == MTN_NO_WATER);
    }
    CHECK(v3_dist_xz(a->pos, before) > 2.0f); /* it found a way along the shore */
    a->pos = v3(lake_w - 0.5f, GROUND, zmid);

    /* walking into a cliff */
    float wall_w = (float)(WALL_X1 + 1) * CELL + 2.0f; /* east foot of the wall, walking west */
    a->pos = v3(wall_w, GROUND, zmid);
    a->state = MTN_AS_WALK;
    a->home = v3(wall_w - 60.0f, 0, zmid);
    a->timer = 100.0f;
    CHECK(mtn_terrain_slope(&t, wall_w - 1.2f, zmid) >= 0.6f);
    mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 1.0f);
    CHECK(a->state == MTN_AS_IDLE);
    CHECK_NEAR(a->pos.x, wall_w, 0);

    /* a walk that runs out of time ends in grazing */
    a->pos = v3(t.center.x, GROUND, t.center.z);
    a->state = MTN_AS_WALK;
    a->home = v3(t.center.x + 30.0f, 0, t.center.z);
    a->timer = 0.5f;
    mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.25f);
    CHECK(a->state == MTN_AS_WALK);
    CHECK(a->pos.x > t.center.x);
    mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.5f);
    CHECK(a->state == MTN_AS_GRAZE);
    CHECK_NEAR(v3_len(a->vel), 0.0f, 0);
    mtn_terrain_free(&t);
}

static void test_flyers(void) {
    mtn_terrain t;
    make_terrain(&t, 0, 0);
    mtn_season_blend su = pure(MTN_SUMMER);
    mtn_fauna f;
    new_fauna(&f, 10);
    int bird = mtn_fauna_add_species(&f, &(mtn_animal_species){0});
    f.species[bird] = flyer("swallow", MTN_ANIMAL_BIRD);
    int eagle = mtn_fauna_add_species(&f, &(mtn_animal_species){0});
    f.species[eagle] = flyer("eagle", MTN_ANIMAL_SOARING);
    int bfly = mtn_fauna_add_species(&f, &(mtn_animal_species){0});
    f.species[bfly] = flyer("butterfly", MTN_ANIMAL_BUTTERFLY);
    int ffly = mtn_fauna_add_species(&f, &(mtn_animal_species){0});
    f.species[ffly] = flyer("firefly", MTN_ANIMAL_FIREFLY);
    vec3 at = t.center;
    for (int k = 0; k < 4; k++) CHECK(mtn_fauna_spawn_group(&f, &t, k, at) == 1);
    f.spawn_timer = 1e9f;
    mtn_animal *b = &f.animals[0], *e = &f.animals[1], *bu = &f.animals[2], *fi = &f.animals[3];
    CHECK(b->species == bird && e->species == eagle && bu->species == bfly && fi->species == ffly);
    vec3 player = at;

    float max_b = 0.0f, max_f = 0.0f, far_b = 0.0f, far_f = 0.0f;
    for (int i = 0; i < 600; i++) {
        float anim_b = b->anim;
        mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.1f);
        /* birds circle their home */
        CHECK_NEAR(v3_dist_xz(b->pos, b->home), 18.0f, 1e-3);
        CHECK_NEAR(v3_dist_xz(e->pos, e->home), 70.0f, 1e-3);
        CHECK(b->pos.y >= GROUND + 5.0f);
        CHECK(e->pos.y > GROUND + 80.0f);
        CHECK_NEAR(b->anim - anim_b, 1.4f, 1e-4);
        vec3 fwd = v3(sinf(b->yaw), 0, -cosf(b->yaw));
        CHECK(v3_dot(fwd, v3_norm(v3(b->vel.x, 0, b->vel.z))) > 0.999f);
        /* insects flutter slowly near home */
        float sb = v3_len(bu->vel), sf = v3_len(fi->vel);
        max_b = mtn_maxf(max_b, sb);
        max_f = mtn_maxf(max_f, sf);
        far_b = mtn_maxf(far_b, v3_dist_xz(bu->pos, bu->home));
        far_f = mtn_maxf(far_f, v3_dist_xz(fi->pos, fi->home));
        CHECK(fabsf(bu->pos.y - (GROUND + bu->home.y)) <= 0.3f + 1e-4f);
        CHECK(fabsf(fi->pos.y - (GROUND + fi->home.y)) <= 0.3f + 1e-4f);
    }
    CHECK(max_b <= 1.6f + 1e-4f && max_b > 0.5f);
    CHECK(max_f <= 0.4f + 1e-4f && max_f > 0.1f);
    CHECK(far_b < 30.0f && far_b > 1.0f);
    CHECK(far_f < 15.0f);
    CHECK(mtn_fauna_count(&f, -1) == 4);

    /* birds never dive into the ground */
    b->home.y = -50.0f;
    mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.1f);
    CHECK_NEAR(b->pos.y, GROUND + 5.0f, 1e-4);
    /* a zero time step leaves the velocity at rest */
    mtn_fauna_update(&f, &t, player, 12.0f, su, 0.0f, 0.0f);
    CHECK_NEAR(v3_len(b->vel), 0.0f, 0);
    mtn_terrain_free(&t);
}

static void test_calls(void) {
    mtn_terrain t;
    make_terrain(&t, 0, 0);
    mtn_fauna f;
    new_fauna(&f, 11);
    CHECK_STR(mtn_fauna_pop_call(&f), "");
    mtn_animal_species s = deer();
    memcpy(s.sound, "abcdefghijklmnopqrstuvwx", 24); /* unterminated: truncated to 23 characters */
    int d = mtn_fauna_add_species(&f, &s);
    s = deer();
    strcpy(s.sound, "ibex");
    int ibex = mtn_fauna_add_species(&f, &s);
    for (int k = 0; k < 3; k++) mtn_fauna_spawn_group(&f, &t, d, t.center);
    for (int k = 0; k < 3; k++) mtn_fauna_spawn_group(&f, &t, ibex, t.center);
    f.spawn_timer = 1e9f;
    /* everyone flees at once: only 4 calls are queued */
    mtn_fauna_update(&f, &t, t.center, 12.0f, pure(MTN_SPRING), 0.0f, 0.1f);
    CHECK(f.call_count == 4);
    CHECK_STR(mtn_fauna_pop_call(&f), "abcdefghijklmnopqrstuvw");
    CHECK_STR(mtn_fauna_pop_call(&f), "abcdefghijklmnopqrstuvw");
    CHECK_STR(mtn_fauna_pop_call(&f), "abcdefghijklmnopqrstuvw");
    CHECK_STR(mtn_fauna_pop_call(&f), "ibex");
    CHECK_STR(mtn_fauna_pop_call(&f), "");
    CHECK(f.call_count == 0);
    mtn_terrain_free(&t);
}

static void test_build(void) {
    mtn_fauna f;
    new_fauna(&f, 12);
    mtn_mesh solid, sprites;
    mtn_mesh_init(&solid);
    mtn_mesh_init(&sprites);
    vec3 cam = v3(0, 0, 0);
    mtn_fauna_build(&f, cam, 100.0f, &solid, &sprites);
    CHECK(solid.vcount == 0 && sprites.vcount == 0);

    /* quadrupeds: count the parts */
    int counts[4];
    for (int horns = 0; horns < 4; horns++) {
        mtn_fauna_clear(&f);
        f.species_count = 0;
        mtn_animal_species s = deer();
        s.horns = horns;
        s.tail = 0.0f;
        mtn_fauna_add_species(&f, &s);
        f.animals[0].alive = 1;
        f.animals[0].pos = v3(5, 1, 5);
        f.animals[0].state = MTN_AS_IDLE;
        mtn_mesh_clear(&solid);
        mtn_fauna_build(&f, cam, 100.0f, &solid, &sprites);
        counts[horns] = solid.vcount;
    }
    CHECK(counts[0] == 188);
    CHECK(counts[1] - counts[0] == 32); /* antlers */
    CHECK(counts[2] - counts[0] == 40); /* curved horns */
    CHECK(counts[3] - counts[0] == 20); /* long ears */
    CHECK(sprites.vcount == 0);
    f.species[0].tail = 0.2f;
    mtn_mesh_clear(&solid);
    mtn_fauna_build(&f, cam, 100.0f, &solid, &sprites);
    CHECK(solid.vcount == counts[3] + 10);
    /* placed at the animal, standing on the ground */
    vec3 mn, mx;
    mtn_mesh_bounds(&solid, &mn, &mx);
    CHECK(mn.y >= 1.0f - 0.05f && mn.y < 1.2f);
    CHECK(mx.y > 1.0f + f.species[0].leg_len);
    CHECK(fabsf((mn.x + mx.x) * 0.5f - 5.0f) < 1.0f);
    CHECK(fabsf((mn.z + mx.z) * 0.5f - 5.0f) < 1.0f);
    float idle_top = mx.y;

    /* grazing lowers the head */
    f.animals[0].state = MTN_AS_GRAZE;
    f.animals[0].anim = MTN_PI * 0.5f / 0.7f;
    mtn_mesh_clear(&solid);
    mtn_fauna_build(&f, cam, 100.0f, &solid, &sprites);
    mtn_mesh_bounds(&solid, &mn, &mx);
    CHECK(mx.y < idle_top - 0.2f);
    /* walking moves the legs */
    mtn_mesh idle;
    mtn_mesh_init(&idle);
    f.animals[0].state = MTN_AS_IDLE;
    f.animals[0].anim = 1.0f;
    mtn_fauna_build(&f, cam, 100.0f, &idle, &sprites);
    f.animals[0].state = MTN_AS_WALK;
    mtn_mesh_clear(&solid);
    mtn_fauna_build(&f, cam, 100.0f, &solid, &sprites);
    CHECK(solid.vcount == idle.vcount);
    CHECK(memcmp(solid.v, idle.v, sizeof(mtn_vertex) * (size_t)solid.vcount) != 0);
    mtn_mesh_free(&idle);
    /* too far from the camera: skipped */
    mtn_mesh_clear(&solid);
    mtn_fauna_build(&f, v3(500, 0, 0), 100.0f, &solid, &sprites);
    CHECK(solid.vcount == 0);

    /* birds */
    mtn_fauna_clear(&f);
    f.species_count = 0;
    mtn_fauna_add_species(&f, &(mtn_animal_species){0});
    f.species[0] = flyer("swallow", MTN_ANIMAL_BIRD);
    mtn_fauna_add_species(&f, &(mtn_animal_species){0});
    f.species[1] = flyer("eagle", MTN_ANIMAL_SOARING);
    f.animals[0].alive = 1;
    f.animals[0].species = 0;
    f.animals[0].pos = v3(0, 20, 0);
    f.animals[0].anim = 1.0f;
    mtn_mesh_clear(&solid);
    mtn_fauna_build(&f, cam, 100.0f, &solid, &sprites);
    CHECK(solid.vcount == 38);
    mtn_mesh_bounds(&solid, &mn, &mx);
    float bird_flap = (mx.y - 20.0f) / f.species[0].size;
    CHECK(bird_flap > 0.2f);
    f.animals[0].species = 1;
    mtn_mesh_clear(&solid);
    mtn_fauna_build(&f, cam, 100.0f, &solid, &sprites);
    CHECK(solid.vcount == 38);
    mtn_mesh_bounds(&solid, &mn, &mx);
    CHECK(mx.x - mn.x > 1.9f); /* 2 m wingspan */
    CHECK((mx.y - 20.0f) / f.species[1].size < bird_flap * 0.5f); /* soaring birds barely flap */

    /* butterflies and fireflies are sprites */
    mtn_fauna_clear(&f);
    f.species_count = 0;
    mtn_fauna_add_species(&f, &(mtn_animal_species){0});
    f.species[0] = flyer("butterfly", MTN_ANIMAL_BUTTERFLY);
    f.species[0].sprite_tile = 40;
    mtn_fauna_add_species(&f, &(mtn_animal_species){0});
    f.species[1] = flyer("firefly", MTN_ANIMAL_FIREFLY);
    f.species[1].sprite_tile = 44;
    f.animals[0] = (mtn_animal){.species = 0, .alive = 1, .pos = v3(1, 1, 1), .fade = 0.5f, .anim = 0.15f};
    f.animals[1] = (mtn_animal){.species = 1, .alive = 1, .pos = v3(2, 1, 1), .fade = 1.0f, .anim = 0.5f};
    mtn_mesh_clear(&solid);
    mtn_mesh_clear(&sprites);
    mtn_fauna_build(&f, cam, 100.0f, &solid, &sprites);
    CHECK(solid.vcount == 0);
    CHECK(sprites.vcount == 8);
    CHECK(sprites.v[0].tile == 41); /* second animation frame */
    CHECK(sprites.v[0].a == 127);   /* half faded */
    CHECK(!(sprites.v[0].flags & MTN_VF_EMISSIVE));
    CHECK(sprites.v[4].tile == 44);
    CHECK(sprites.v[4].flags & MTN_VF_EMISSIVE);
    CHECK(sprites.v[4].r == 255); /* sin(1.5) * 1.5 > 1: fully lit */
    f.animals[1].anim = MTN_PI;    /* sin(3 pi) = 0: dark */
    mtn_mesh_clear(&sprites);
    mtn_fauna_build(&f, cam, 100.0f, &solid, &sprites);
    CHECK(sprites.v[4].r < 5);
    mtn_mesh_free(&solid);
    mtn_mesh_free(&sprites);
}

MTN_SUITE(fauna) {
    TEST_RUN(test_species);
    TEST_RUN(test_active);
    TEST_RUN(test_spawn_group);
    TEST_RUN(test_spawning);
    TEST_RUN(test_spawn_distances);
    TEST_RUN(test_fade_and_despawn);
    TEST_RUN(test_flee);
    TEST_RUN(test_wander);
    TEST_RUN(test_obstacles);
    TEST_RUN(test_flyers);
    TEST_RUN(test_calls);
    TEST_RUN(test_build);
}
