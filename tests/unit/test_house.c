#include "test.h"
#include "mtn/house.h"

#include <stdlib.h>

/* ---------------------------------------------------------------- fixtures */

/* A tiny hand-made terrain: 65x65 points, 2 m cells (128 m square), centre (64, 64),
 * playable radius 52.5 m. Height = base + slope_x * x. No water, forest, moist 0.5. */
#define TN 65
#define TCELL 2.0f

static void make_terrain(mtn_terrain *t, float base, float slope_x) {
    memset(t, 0, sizeof *t);
    mtn_terrain_default_params(&t->p);
    t->p.size = TN;
    t->p.cell = TCELL;
    t->n = TN;
    t->cell = TCELL;
    t->world_size = TCELL * (float)(TN - 1);
    size_t nn = (size_t)TN * TN;
    t->h = malloc(sizeof(float) * nn);
    t->water = malloc(sizeof(float) * nn);
    t->moist = malloc(sizeof(float) * nn);
    t->trail_dist = malloc(sizeof(float) * nn);
    t->biome = malloc(nn);
    for (int z = 0; z < TN; z++)
        for (int x = 0; x < TN; x++) {
            int i = z * TN + x;
            t->h[i] = base + slope_x * (float)x * TCELL;
            t->water[i] = MTN_NO_WATER;
            t->moist[i] = 0.5f;
            t->trail_dist[i] = 50.0f;
            t->biome[i] = MTN_BIOME_FOREST;
        }
    t->min_h = mtn_minf(t->h[0], t->h[TN - 1]);
    t->max_h = mtn_maxf(t->h[0], t->h[TN - 1]);
    t->center = v3(64, mtn_terrain_height(t, 64, 64), 64);
}

static void set_water(mtn_terrain *t, int gx0, int gx1, int gz0, int gz1, float level) {
    for (int z = gz0; z <= gz1; z++)
        for (int x = gx0; x <= gx1; x++) t->water[z * TN + x] = level;
}

static void fill_terrain(mtn_terrain *t, float moist, int biome) {
    for (int i = 0; i < TN * TN; i++) {
        t->moist[i] = moist;
        t->biome[i] = (uint8_t)biome;
    }
}

/* flora with a single plant */
static void make_flora(mtn_flora *f, vec3 pos, float height, int kind) {
    mtn_flora_init(f);
    mtn_plant_species s;
    mtn_species_defaults(&s);
    s.kind = kind;
    snprintf(s.name, sizeof s.name, "%s", kind == MTN_PLANT_TREE ? "spruce" : "shrub");
    CHECK(mtn_flora_add_species(f, &s) == 0);
    f->plants = calloc(1, sizeof(mtn_plant));
    f->count = f->cap = 1;
    f->plants[0].pos = pos;
    f->plants[0].scale = height;
    f->plants[0].species = 0;
    f->chunks = 1;
    f->chunk_start = calloc(1, sizeof(int));
    f->chunk_count = calloc(1, sizeof(int));
    f->chunk_version = calloc(1, sizeof(int));
    f->chunk_count[0] = 1;
}

/* player in the middle of the map looking north (-z): the target is (64, 56) */
static vec3 player_at(const mtn_terrain *t) { return v3(64, mtn_terrain_height(t, 64, 64), 64); }

static int mesh_ok(const mtn_mesh *m) {
    if (m->icount % 3 != 0) return 0;
    for (int i = 0; i < m->vcount; i++) {
        const mtn_vertex *v = &m->v[i];
        if (!isfinite(v->x) || !isfinite(v->y) || !isfinite(v->z) || !isfinite(v->nx) || !isfinite(v->u) || !isfinite(v->v))
            return 0;
    }
    for (int i = 0; i < m->icount; i++)
        if (m->idx[i] >= (uint32_t)m->vcount) return 0;
    return 1;
}

static int count_tile(const mtn_mesh *m, int tile) {
    int n = 0;
    for (int i = 0; i < m->vcount; i++) n += m->v[i].tile == tile;
    return n;
}

static int has_color(const mtn_mesh *m, int tile, int r, int g, int b) {
    for (int i = 0; i < m->vcount; i++)
        if (m->v[i].tile == tile && m->v[i].r == r && m->v[i].g == g && m->v[i].b == b) return 1;
    return 0;
}

static int mat_near(mat4 a, mat4 b, float eps) {
    for (int i = 0; i < 16; i++)
        if (fabsf(a.m[i] - b.m[i]) > eps) return 0;
    return 1;
}

static vec3 to_world(const mtn_house *h, float lx, float ly, float lz) { return m4_mul_point(h->frame, v3(lx, ly, lz)); }
static vec3 to_local(const mtn_house *h, vec3 w) { return m4_mul_point(h->inv_frame, w); }

static void make_present(mtn_house *h, const mtn_house_site *s, uint32_t seed) {
    mtn_house_init(h);
    mtn_house_summon(h, s, seed);
    mtn_house_update(h, 2.0f);
}

/* ---------------------------------------------------------------- basics */

static void test_defaults(void) {
    static mtn_house h;
    mtn_house_init(&h);
    CHECK(h.state == MTN_HOUSE_NONE);
    CHECK(!mtn_house_exists(&h));
    CHECK_NEAR(h.width, 6, 0);
    CHECK_NEAR(h.depth, 5, 0);
    CHECK_NEAR(h.wall_h, 2.7f, 1e-6);
    CHECK(h.type_count == 0);
    CHECK_STR(mtn_house_variant_name(MTN_HV_COTTAGE), "cottage");
    CHECK_STR(mtn_house_variant_name(MTN_HV_HILLSIDE), "hillside");
    CHECK_STR(mtn_house_variant_name(MTN_HV_TREE), "tree");
    CHECK_STR(mtn_house_variant_name(MTN_HV_STILTS), "stilts");
    CHECK_STR(mtn_house_variant_name(-1), "unknown");
    CHECK_STR(mtn_house_variant_name(MTN_HV_COUNT), "unknown");

    /* a house that does not exist has no surfaces, collisions or meshes */
    float hh = -7;
    vec3 p = v3(0, 0, 0);
    CHECK(mtn_house_door_distance(&h, p) > 1e29f);
    CHECK(!mtn_house_surface(&h, 0, 0, &hh));
    CHECK(hh == -7);
    CHECK(!mtn_house_collide(&h, &p, 0.3f));
    CHECK(!mtn_house_crossed_in(&h, v3(0, 0, 3), v3(0, 0, 0)));
    CHECK(!mtn_house_can_unsummon(&h, p));
    mtn_mesh a, b, c;
    mtn_mesh_init(&a);
    mtn_mesh_init(&b);
    mtn_mesh_init(&c);
    mtn_house_build_exterior(&h, NULL, NULL, &a, &b, &c);
    CHECK(a.vcount == 0 && b.vcount == 0 && c.vcount == 0);
    mtn_house_toggle_door(&h);
    CHECK(!h.door_open);
    mtn_house_unsummon(&h);
    CHECK(h.state == MTN_HOUSE_NONE);
    mtn_house_update(&h, 1.0f);
    CHECK(h.state == MTN_HOUSE_NONE && h.version == 0);

    /* room types and items */
    mtn_room_type t;
    mtn_room_type_defaults(&t);
    CHECK_STR(t.name, "room");
    CHECK_NEAR(t.width, 8, 0);
    CHECK_NEAR(t.height, 3.6f, 1e-6);
    CHECK(t.wall_color == 0xFFFFFFFFu && t.item_count == 0 && !t.is_cabin);
    CHECK_NEAR(t.weight, 1, 0);
    CHECK(mtn_room_add_item(&t, "bed", 1, 2, 90, 0, 3) == 0);
    CHECK_STR(t.items[0].prefab, "bed");
    CHECK_NEAR(t.items[0].scale, 1, 0); /* non-positive scale -> 1 */
    CHECK(t.items[0].arg == 3 && t.items[0].x == 1 && t.items[0].z == 2 && t.items[0].rot == 90);
    CHECK(mtn_room_add_item(&t, "a_prefab_name_that_is_much_too_long", 0, 0, 0, 2.5f, 0) == 1);
    CHECK(strlen(t.items[1].prefab) == sizeof t.items[1].prefab - 1);
    CHECK_NEAR(t.items[1].scale, 2.5f, 0);
    while (t.item_count < MTN_MAX_ROOM_ITEMS) mtn_room_add_item(&t, "rug", 0, 0, 0, 1, 0);
    CHECK(mtn_room_add_item(&t, "rug", 0, 0, 0, 1, 0) == -1);
    CHECK(t.item_count == MTN_MAX_ROOM_ITEMS);
    for (int i = 0; i < MTN_MAX_ROOM_TYPES; i++) CHECK(mtn_house_add_room_type(&h, &t) == i);
    CHECK(mtn_house_add_room_type(&h, &t) == -1);
    CHECK(h.type_count == MTN_MAX_ROOM_TYPES);
    mtn_mesh_free(&a);
    mtn_mesh_free(&b);
    mtn_mesh_free(&c);
}

/* ---------------------------------------------------------------- site analysis */

static void test_site_cottage(void) {
    mtn_terrain t;
    make_terrain(&t, 100, 0);
    mtn_house_site s;
    CHECK(mtn_house_find_site(&t, NULL, player_at(&t), 0.0f, &s));
    CHECK(s.ok);
    CHECK(s.variant == MTN_HV_COTTAGE);
    CHECK_NEAR(s.pos.x, 64, 1e-3);
    CHECK_NEAR(s.pos.z, 56, 1e-3); /* 8 m in front of the player */
    CHECK_NEAR(s.pos.y, 100.25f, 1e-4);
    CHECK_NEAR(s.front.x, 0, 1e-4);
    CHECK_NEAR(s.front.z, 1, 1e-4); /* the door faces the player */
    CHECK_NEAR(s.ground_min, 100, 1e-4);
    CHECK_NEAR(s.ground_max, 100, 1e-4);
    CHECK(s.tree == -1);
    CHECK(s.reason[0] == 0);
    /* yaw rotates the target around the player */
    CHECK(mtn_house_find_site(&t, NULL, player_at(&t), MTN_PI * 0.5f, &s));
    CHECK_NEAR(s.pos.x, 72, 1e-3);
    CHECK_NEAR(s.pos.z, 64, 1e-3);
    CHECK_NEAR(s.front.x, -1, 1e-4);
    mtn_terrain_free(&t);
}

static void test_site_hillside(void) {
    mtn_terrain t;
    make_terrain(&t, 100, 0.5f); /* 3.6 m across the 7.2 m wide footprint */
    mtn_house_site s;
    CHECK(mtn_house_find_site(&t, NULL, player_at(&t), 0.0f, &s));
    CHECK(s.variant == MTN_HV_HILLSIDE);
    CHECK_NEAR(s.pos.x, 64, 1e-3);
    CHECK_NEAR(s.pos.z, 56, 1e-3);
    CHECK_NEAR(s.ground_max - s.ground_min, 3.6f, 1e-3);
    /* the door faces downhill and the floor sits on the ground at the door */
    CHECK_NEAR(s.front.x, -1, 1e-4);
    CHECK_NEAR(s.front.z, 0, 1e-4);
    CHECK_NEAR(s.pos.y, mtn_terrain_height(&t, 64 - 2.5f, 56) + 0.2f, 1e-3);
    mtn_terrain_free(&t);
}

static void test_site_rejected(void) {
    mtn_terrain t;
    mtn_house_site s;
    /* too steep: >9 m over the footprint in every orientation */
    make_terrain(&t, 100, 1.5f);
    CHECK(!mtn_house_find_site(&t, NULL, player_at(&t), 0.0f, &s));
    CHECK(!s.ok);
    CHECK_STR(s.reason, "the slope is too steep");
    mtn_terrain_free(&t);
    /* flooded */
    make_terrain(&t, 100, 0);
    set_water(&t, 0, TN - 1, 0, TN - 1, 101);
    CHECK(!mtn_house_find_site(&t, NULL, player_at(&t), 0.0f, &s));
    CHECK_STR(s.reason, "the ground here is under water");
    /* water below the ground does not count */
    set_water(&t, 0, TN - 1, 0, TN - 1, 99);
    CHECK(mtn_house_find_site(&t, NULL, player_at(&t), 0.0f, &s));
    CHECK(s.variant == MTN_HV_COTTAGE);
    /* the edge of the world */
    set_water(&t, 0, TN - 1, 0, TN - 1, MTN_NO_WATER);
    CHECK(!mtn_house_find_site(&t, NULL, v3(6, 100, 6), MTN_PI * 1.25f, &s));
    CHECK_STR(s.reason, "the edge of the world is too close");
    mtn_terrain_free(&t);
}

static void test_site_stilts(void) {
    mtn_terrain t;
    make_terrain(&t, 100, 0);
    set_water(&t, 37, TN - 1, 0, TN - 1, 100.5f); /* a lake east of x = 74 */
    mtn_house_site s;
    CHECK(mtn_house_find_site(&t, NULL, player_at(&t), 0.0f, &s));
    CHECK(s.variant == MTN_HV_STILTS);
    CHECK_NEAR(s.pos.x, 64, 1e-3);
    CHECK_NEAR(s.pos.z, 56, 1e-3);
    CHECK_NEAR(s.water_level, 100.5f, 1e-4);
    CHECK_NEAR(s.pos.y, 101.4f, 1e-4); /* above the water */
    CHECK_NEAR(s.front.x, 1, 1e-4);   /* looking out over the lake */
    CHECK_NEAR(s.ground_min, 100, 1e-4);
    mtn_terrain_free(&t);
}

static void test_site_tree(void) {
    mtn_terrain t;
    make_terrain(&t, 100, 0);
    mtn_flora f;
    mtn_house_site s;
    make_flora(&f, v3(66, 100, 57), 14.0f, MTN_PLANT_TREE);
    CHECK(mtn_house_find_site(&t, &f, player_at(&t), 0.0f, &s));
    CHECK(s.variant == MTN_HV_TREE);
    CHECK(s.tree == 0);
    CHECK_NEAR(s.pos.x, 66, 1e-4);
    CHECK_NEAR(s.pos.z, 57, 1e-4);
    CHECK_NEAR(s.pos.y, 103.6f, 1e-4);
    CHECK_NEAR(s.ground_min, 100, 1e-4);
    CHECK_NEAR(s.front.z, 1, 0.1);
    /* a hidden tree, a short tree or a bush give an ordinary cottage */
    f.plants[0].hidden = 1;
    CHECK(mtn_house_find_site(&t, &f, player_at(&t), 0.0f, &s));
    CHECK(s.variant == MTN_HV_COTTAGE && s.tree == -1);
    f.plants[0].hidden = 0;
    f.plants[0].scale = 8.0f;
    CHECK(mtn_house_find_site(&t, &f, player_at(&t), 0.0f, &s));
    CHECK(s.variant == MTN_HV_COTTAGE);
    mtn_flora_free(&f);
    make_flora(&f, v3(64, 100, 56), 14.0f, MTN_PLANT_BUSH);
    CHECK(mtn_house_find_site(&t, &f, player_at(&t), 0.0f, &s));
    CHECK(s.variant == MTN_HV_COTTAGE);
    mtn_flora_free(&f);
    /* a tree too far from every candidate */
    make_flora(&f, v3(64, 100, 30), 14.0f, MTN_PLANT_TREE);
    CHECK(mtn_house_find_site(&t, &f, player_at(&t), 0.0f, &s));
    CHECK(s.variant == MTN_HV_COTTAGE);
    mtn_flora_free(&f);
    mtn_terrain_free(&t);
}

/* ---------------------------------------------------------------- lifecycle */

static void test_lifecycle(void) {
    mtn_terrain t;
    make_terrain(&t, 100, 0);
    mtn_house_site s;
    CHECK(mtn_house_find_site(&t, NULL, player_at(&t), 0.0f, &s));
    static mtn_house h;
    mtn_house_init(&h);
    mtn_house_summon(&h, &s, 7);
    CHECK(h.state == MTN_HOUSE_APPEARING);
    CHECK(mtn_house_exists(&h));
    CHECK(h.anim == 0 && h.version == 1 && h.seed == 7);
    CHECK(h.variant == MTN_HV_COTTAGE && h.tree == -1);
    CHECK_NEAR(h.floor_y, 100.25f, 1e-4);
    CHECK_NEAR(h.right.x, 1, 1e-5); /* right = up x front */
    CHECK_NEAR(h.right.z, 0, 1e-5);
    vec3 o = to_world(&h, 0, 0, 0);
    CHECK_NEAR(o.x, 64, 1e-4);
    CHECK_NEAR(o.z, 56, 1e-4);
    vec3 back = to_local(&h, v3(65, 101, 59));
    CHECK_NEAR(back.x, 1, 1e-4);
    CHECK_NEAR(back.y, 0.75f, 1e-4);
    CHECK_NEAR(back.z, 3, 1e-4);
    vec3 ct = h.chimney_top;
    CHECK_NEAR(ct.x, 64 + 1.8f, 1e-4);
    CHECK_NEAR(ct.y, 100.25f + 2.7f + 2.9f, 1e-4);
    CHECK_NEAR(ct.z, 56 - 1.25f, 1e-4);
    /* the door only works once the house is fully there */
    mtn_house_toggle_door(&h);
    CHECK(!h.door_open);
    CHECK(!mtn_house_can_unsummon(&h, o));
    mtn_house_update(&h, 0.8f);
    CHECK(h.state == MTN_HOUSE_APPEARING);
    CHECK_NEAR(h.anim, 0.5f, 1e-5);
    mtn_house_update(&h, 0.9f);
    CHECK(h.state == MTN_HOUSE_PRESENT);
    CHECK_NEAR(h.anim, 1, 0);
    mtn_house_update(&h, 0.5f);
    CHECK(h.state == MTN_HOUSE_PRESENT && h.anim == 1);

    /* door animation */
    mtn_house_toggle_door(&h);
    CHECK(h.door_open);
    mtn_house_update(&h, 0.2f);
    CHECK_NEAR(h.door_angle, 0.44f, 1e-5);
    mtn_house_update(&h, 1.0f);
    CHECK_NEAR(h.door_angle, 1.75f, 1e-6);
    mtn_house_update(&h, 1.0f);
    CHECK_NEAR(h.door_angle, 1.75f, 1e-6);
    mtn_house_toggle_door(&h);
    CHECK(!h.door_open);
    mtn_house_update(&h, 0.5f);
    CHECK_NEAR(h.door_angle, 0.65f, 1e-5);
    mtn_house_update(&h, 1.0f);
    CHECK_NEAR(h.door_angle, 0, 0);

    /* door position and distance (vertical offsets count a quarter) */
    vec3 d = mtn_house_door_world(&h);
    CHECK_NEAR(d.x, 64, 1e-4);
    CHECK_NEAR(d.y, 100.25f, 1e-4);
    CHECK_NEAR(d.z, 58.5f, 1e-4);
    CHECK_NEAR(mtn_house_door_distance(&h, v3(d.x + 3, d.y + 4, d.z)), sqrtf(13), 1e-4);
    CHECK_NEAR(mtn_house_door_distance(&h, d), 0, 1e-5);

    /* unsummoning: only from outside and within 25 m of the house */
    CHECK(mtn_house_can_unsummon(&h, v3(64 + 25, 0, 56)));
    CHECK(mtn_house_can_unsummon(&h, v3(64, 500, 56 - 24.9f)));
    CHECK(!mtn_house_can_unsummon(&h, v3(64 + 25.2f, 100, 56)));
    h.inside = 1;
    CHECK(!mtn_house_can_unsummon(&h, o));
    h.inside = 0;
    mtn_house_toggle_door(&h);
    mtn_house_update(&h, 1.0f);
    mtn_house_unsummon(&h);
    CHECK(h.state == MTN_HOUSE_VANISHING);
    CHECK(!h.door_open);
    CHECK(mtn_house_exists(&h));
    CHECK(!mtn_house_can_unsummon(&h, o));
    mtn_house_toggle_door(&h);
    CHECK(!h.door_open);
    float hh = 0;
    CHECK(!mtn_house_surface(&h, 64, 59, &hh)); /* no porch while vanishing */
    vec3 p = to_world(&h, 2.8f, 0.5f, 0);
    CHECK(!mtn_house_collide(&h, &p, 0.3f));
    mtn_house_update(&h, 0.6f);
    CHECK(h.state == MTN_HOUSE_VANISHING);
    CHECK_NEAR(h.anim, 0.5f, 1e-5);
    CHECK(h.door_angle < 1.75f); /* the door swings shut while vanishing */
    mtn_house_update(&h, 0.7f);
    CHECK(h.state == MTN_HOUSE_NONE);
    CHECK(h.anim == 0);
    CHECK(h.version == 2);
    CHECK(!mtn_house_exists(&h));
    mtn_house_unsummon(&h);
    CHECK(h.state == MTN_HOUSE_NONE);

    /* unsummon while still appearing; re-summon bumps the version */
    mtn_house_summon(&h, &s, 8);
    CHECK(h.version == 3);
    mtn_house_unsummon(&h);
    CHECK(h.state == MTN_HOUSE_VANISHING);

    /* a degenerate front falls back to +z */
    mtn_house_site odd = s;
    odd.front = v3(0, 1, 0);
    mtn_house_summon(&h, &odd, 9);
    CHECK_NEAR(h.front.z, 1, 0);
    CHECK_NEAR(h.right.x, 1, 0);
    mtn_terrain_free(&t);
}

/* ---------------------------------------------------------------- walking */

static void test_surfaces(void) {
    mtn_terrain t;
    mtn_house_site s;
    static mtn_house h;
    float y = -1;

    /* cottage: porch only (0.25 m step, no stairs) */
    make_terrain(&t, 100, 0);
    CHECK(mtn_house_find_site(&t, NULL, player_at(&t), 0.0f, &s));
    mtn_house_init(&h);
    mtn_house_summon(&h, &s, 1);
    vec3 w = to_world(&h, 0.5f, 0, 3.0f);
    CHECK(mtn_house_surface(&h, w.x, w.z, &y)); /* usable while appearing */
    CHECK_NEAR(y, h.floor_y, 1e-5);
    mtn_house_update(&h, 2);
    w = to_world(&h, -1.5f, 0, 4.0f);
    CHECK(mtn_house_surface(&h, w.x, w.z, &y));
    y = -1;
    w = to_world(&h, 0, 0, 4.5f); /* beyond the porch: no stairs for a small step */
    CHECK(!mtn_house_surface(&h, w.x, w.z, &y));
    CHECK(y == -1);
    w = to_world(&h, 2.5f, 0, 3.0f); /* beside the porch */
    CHECK(!mtn_house_surface(&h, w.x, w.z, &y));
    mtn_terrain_free(&t);

    /* hillside: porch and a short stair down to the lowest ground */
    make_terrain(&t, 100, 0.5f);
    CHECK(mtn_house_find_site(&t, NULL, player_at(&t), 0.0f, &s));
    make_present(&h, &s, 2);
    float drop = h.floor_y - h.ground_min;
    CHECK(drop > 0.3f);
    w = to_world(&h, 0, 0, 3.0f);
    CHECK(mtn_house_surface(&h, w.x, w.z, &y));
    CHECK_NEAR(y, h.floor_y, 1e-5);
    w = to_world(&h, 0.5f, 0, 4.1f + 0.5f);
    CHECK(mtn_house_surface(&h, w.x, w.z, &y));
    CHECK_NEAR(y, h.floor_y - 0.5f * 0.7f, 1e-3);
    w = to_world(&h, 0, 0, 4.1f + drop / 0.7f + 0.2f);
    CHECK(!mtn_house_surface(&h, w.x, w.z, &y));
    w = to_world(&h, 1.0f, 0, 4.6f); /* beside the stairs */
    CHECK(!mtn_house_surface(&h, w.x, w.z, &y));
    mtn_terrain_free(&t);

    /* stilts: a platform around the whole house and stairs from its edge */
    make_terrain(&t, 100, 0);
    set_water(&t, 37, TN - 1, 0, TN - 1, 100.5f);
    CHECK(mtn_house_find_site(&t, NULL, player_at(&t), 0.0f, &s));
    make_present(&h, &s, 3);
    CHECK(h.variant == MTN_HV_STILTS);
    w = to_world(&h, -4.0f, 0, -3.5f); /* back corner of the platform */
    CHECK(mtn_house_surface(&h, w.x, w.z, &y));
    CHECK_NEAR(y, h.floor_y, 1e-5);
    w = to_world(&h, 0, 0, 3.7f + 1.0f);
    CHECK(mtn_house_surface(&h, w.x, w.z, &y));
    CHECK_NEAR(y, h.floor_y - 0.7f, 1e-3);
    w = to_world(&h, 0, 0, 3.7f + 0.05f); /* platform edge meets the top step */
    CHECK(mtn_house_surface(&h, w.x, w.z, &y));
    CHECK_NEAR(y, h.floor_y - 0.035f, 1e-3);
    w = to_world(&h, 4.5f, 0, 0);
    CHECK(!mtn_house_surface(&h, w.x, w.z, &y));
    mtn_terrain_free(&t);
}

static void test_collision(void) {
    mtn_terrain t;
    mtn_house_site s;
    static mtn_house h;
    make_terrain(&t, 100, 0);
    CHECK(mtn_house_find_site(&t, NULL, player_at(&t), 0.0f, &s));
    make_present(&h, &s, 4);
    const float r = 0.3f;
    vec3 p, l;
    /* right wall: pushed out along x */
    p = to_world(&h, 2.8f, 0.5f, 0.4f);
    CHECK(mtn_house_collide(&h, &p, r));
    l = to_local(&h, p);
    CHECK_NEAR(l.x, 3.3f, 1e-4);
    CHECK_NEAR(l.z, 0.4f, 1e-4);
    /* left wall */
    p = to_world(&h, -3.0f, 0.5f, -0.5f);
    CHECK(mtn_house_collide(&h, &p, r));
    l = to_local(&h, p);
    CHECK_NEAR(l.x, -3.3f, 1e-4);
    /* front wall (closed door) and back wall: pushed out along z */
    p = to_world(&h, 0.1f, 0.5f, 2.6f);
    CHECK(mtn_house_collide(&h, &p, r));
    l = to_local(&h, p);
    CHECK_NEAR(l.z, 2.8f, 1e-4);
    CHECK_NEAR(l.x, 0.1f, 1e-4);
    p = to_world(&h, -1.0f, 0.5f, -2.6f);
    CHECK(mtn_house_collide(&h, &p, r));
    l = to_local(&h, p);
    CHECK_NEAR(l.z, -2.8f, 1e-4);
    /* y is left alone */
    CHECK_NEAR(p.y, h.floor_y + 0.5f, 1e-4);
    /* outside, too high or too low: no contact */
    p = to_world(&h, 3.4f, 0.5f, 0);
    vec3 before = p;
    CHECK(!mtn_house_collide(&h, &p, r));
    CHECK(p.x == before.x && p.z == before.z);
    p = to_world(&h, 2.8f, 2.7f + 2.6f, 0);
    CHECK(!mtn_house_collide(&h, &p, r));
    p = to_world(&h, 2.8f, -1.7f, 0);
    CHECK(!mtn_house_collide(&h, &p, r));
    p = to_world(&h, 2.8f, 2.7f + 2.4f, 0); /* on the roof edge still collides */
    CHECK(mtn_house_collide(&h, &p, r));

    /* the open door lets the player through the doorway, but not beside it */
    mtn_house_toggle_door(&h);
    mtn_house_update(&h, 0.2f); /* opening, not wide enough yet */
    p = to_world(&h, 0.1f, 0.5f, 2.6f);
    CHECK(mtn_house_collide(&h, &p, r));
    mtn_house_update(&h, 1.0f);
    p = to_world(&h, 0.1f, 0.5f, 2.6f);
    before = p;
    CHECK(!mtn_house_collide(&h, &p, r));
    CHECK(p.x == before.x && p.z == before.z);
    p = to_world(&h, 0.6f, 0.5f, 2.6f);
    CHECK(mtn_house_collide(&h, &p, r));

    /* crossing the door plane inwards */
    vec3 a = to_world(&h, 0, 0.1f, 2.6f), b = to_world(&h, 0, 0.1f, 2.2f);
    CHECK(mtn_house_crossed_in(&h, a, b));
    CHECK(!mtn_house_crossed_in(&h, b, a)); /* walking out */
    CHECK(!mtn_house_crossed_in(&h, a, to_world(&h, 0, 0.1f, 2.4f))); /* not through yet */
    CHECK(!mtn_house_crossed_in(&h, to_world(&h, 0.8f, 0.1f, 2.6f), to_world(&h, 0.8f, 0.1f, 2.2f)));
    CHECK(!mtn_house_crossed_in(&h, to_world(&h, 0, 2.5f, 2.6f), to_world(&h, 0, 2.5f, 2.2f)));
    CHECK(!mtn_house_crossed_in(&h, to_world(&h, 0, -1.5f, 2.6f), to_world(&h, 0, -1.5f, 2.2f)));
    mtn_house_toggle_door(&h);
    CHECK(!mtn_house_crossed_in(&h, a, b)); /* closing */
    mtn_house_update(&h, 2.0f);
    CHECK(!mtn_house_crossed_in(&h, a, b));
    mtn_house_toggle_door(&h);
    mtn_house_update(&h, 0.1f); /* barely open */
    CHECK(!mtn_house_crossed_in(&h, a, b));
    mtn_terrain_free(&t);
}

/* ---------------------------------------------------------------- portals */

static void test_portals(void) {
    mtn_terrain t;
    mtn_house_site s;
    static mtn_house h;
    make_terrain(&t, 100, 0.5f); /* hillside: a door that faces -x */
    CHECK(mtn_house_find_site(&t, NULL, player_at(&t), 0.0f, &s));
    make_present(&h, &s, 11);
    CHECK_NEAR(h.front.x, -1, 1e-4);

    mat4 fe = mtn_house_door_frame_ext(&h);
    vec3 fo = m4_mul_point(fe, v3(0, 0, 0));
    vec3 dp = to_world(&h, 0, 0, 2.5f - 0.15f);
    CHECK_NEAR(fo.x, dp.x, 1e-4);
    CHECK_NEAR(fo.y, dp.y, 1e-4);
    CHECK_NEAR(fo.z, dp.z, 1e-4);
    vec3 ff = m4_mul_dir(fe, v3(0, 0, 1)); /* forward points into the house */
    CHECK_NEAR(ff.x, -h.front.x, 1e-5);
    CHECK_NEAR(ff.z, -h.front.z, 1e-5);
    mat4 fi = mtn_house_door_frame_int();
    vec3 fio = m4_mul_point(fi, v3(0, 0, 0));
    CHECK_NEAR(v3_len(fio), 0, 1e-6);
    CHECK_NEAR(m4_mul_dir(fi, v3(0, 0, 1)).z, -1, 1e-6);

    /* a point just inside the exterior door lands just inside the interior front door */
    mat4 x = mtn_house_ext_to_int(&h);
    vec3 in = m4_mul_point(x, v3_add(fo, v3_add(v3_scale(h.front, -0.2f), v3(0, 1.0f, 0))));
    CHECK_NEAR(in.x, 0, 1e-4);
    CHECK_NEAR(in.y, 1.0f, 1e-4);
    CHECK_NEAR(in.z, 0.2f, 1e-4);
    CHECK(mtn_house_room_at(in.z) == 0);
    /* walking into the house is walking along +z inside */
    vec3 dir = m4_mul_dir(x, v3_scale(h.front, -1.0f));
    CHECK_NEAR(dir.x, 0, 1e-5);
    CHECK_NEAR(dir.y, 0, 1e-5);
    CHECK_NEAR(dir.z, 1, 1e-5);
    /* the door centre maps onto the interior door centre, sideways stays sideways */
    vec3 side = m4_mul_point(x, v3_add(fo, h.right));
    CHECK_NEAR(fabsf(side.x), 1, 1e-4);
    CHECK_NEAR(side.z, 0, 1e-4);
    /* composed with its inverse (and with the way back through the door) is identity */
    CHECK(mat_near(m4_mul(x, m4_inverse(x)), m4_identity(), 1e-4f));
    mat4 back = mtn_portal_transform(fi, fe);
    CHECK(mat_near(m4_mul(back, x), m4_identity(), 1e-3f));
    vec3 rt = m4_mul_point(back, in);
    vec3 orig = v3_add(fo, v3_add(v3_scale(h.front, -0.2f), v3(0, 1.0f, 0)));
    CHECK_NEAR(rt.x, orig.x, 1e-3);
    CHECK_NEAR(rt.y, orig.y, 1e-3);
    CHECK_NEAR(rt.z, orig.z, 1e-3);

    /* windows: room 0 looks out of the side of the house, later rooms at viewpoints */
    t.vp_count = 3;
    for (int i = 0; i < 3; i++) {
        t.vps[i].pos = v3(10.0f + 20.0f * (float)i, 150.0f + (float)i, 30.0f);
        t.vps[i].yaw = 0.7f * (float)i + 0.3f;
    }
    mtn_mesh a, b, c;
    mtn_mesh_init(&a);
    mtn_mesh_init(&b);
    mtn_mesh_init(&c);
    mtn_room r;
    mtn_house_build_room(&h, &t, 0, &r, &a, &b, &c);
    CHECK(r.view == -1);
    vec3 wc = m4_mul_point(r.window_frame, v3(0, 0, 0));
    mat4 ww = mtn_house_window_to_world(&h, &t, &r);
    vec3 m0 = m4_mul_point(ww, wc);
    vec3 side_pt = to_world(&h, 3.0f + 0.3f, 1.9f, 0);
    CHECK_NEAR(m0.x, side_pt.x, 1e-3);
    CHECK_NEAR(m0.y, side_pt.y, 1e-3);
    CHECK_NEAR(m0.z, side_pt.z, 1e-3);
    vec3 look = m4_mul_dir(ww, m4_mul_dir(r.window_frame, v3(0, 0, 1)));
    CHECK_NEAR(look.x, h.right.x, 1e-4); /* looking out sideways */
    CHECK_NEAR(look.z, h.right.z, 1e-4);
    for (int idx = 1; idx <= 4; idx++) {
        mtn_house_build_room(&h, &t, idx, &r, &a, &b, &c);
        CHECK(r.view == (int)(11u % 97u) + idx - 1);
        const mtn_viewpoint *vp = &t.vps[r.view % 3];
        wc = m4_mul_point(r.window_frame, v3(0, 0, 0));
        ww = mtn_house_window_to_world(&h, &t, &r);
        vec3 m = m4_mul_point(ww, wc);
        CHECK_NEAR(m.x, vp->pos.x, 1e-3);
        CHECK_NEAR(m.y, vp->pos.y + 0.2f, 1e-3);
        CHECK_NEAR(m.z, vp->pos.z, 1e-3);
        look = m4_mul_dir(ww, m4_mul_dir(r.window_frame, v3(0, 0, 1)));
        CHECK_NEAR(look.x, sinf(vp->yaw), 1e-4);
        CHECK_NEAR(look.z, -cosf(vp->yaw), 1e-4);
    }
    /* without a terrain or viewpoints every window shows the real view */
    m0 = m4_mul_point(mtn_house_window_to_world(&h, NULL, &r), m4_mul_point(r.window_frame, v3(0, 0, 0)));
    CHECK_NEAR(m0.x, side_pt.x, 1e-3);
    CHECK_NEAR(m0.z, side_pt.z, 1e-3);
    t.vp_count = 0;
    m0 = m4_mul_point(mtn_house_window_to_world(&h, &t, &r), m4_mul_point(r.window_frame, v3(0, 0, 0)));
    CHECK_NEAR(m0.y, side_pt.y, 1e-3);
    mtn_mesh_free(&a);
    mtn_mesh_free(&b);
    mtn_mesh_free(&c);
    mtn_terrain_free(&t);
}

/* ---------------------------------------------------------------- meshes */

static void build_ext(const mtn_house *h, const mtn_terrain *t, mtn_mesh *s, mtn_mesh *sp, mtn_mesh *g) {
    mtn_mesh_init(s);
    mtn_mesh_init(sp);
    mtn_mesh_init(g);
    mtn_house_build_exterior(h, t, NULL, s, sp, g);
}

static void free3(mtn_mesh *a, mtn_mesh *b, mtn_mesh *c) {
    mtn_mesh_free(a);
    mtn_mesh_free(b);
    mtn_mesh_free(c);
}

static void test_exterior_variants(void) {
    mtn_terrain t;
    mtn_house_site s;
    static mtn_house h;
    mtn_mesh so, sp, gl;
    int counts[MTN_HV_COUNT];

    /* cottage */
    make_terrain(&t, 100, 0);
    CHECK(mtn_house_find_site(&t, NULL, player_at(&t), 0.0f, &s));
    make_present(&h, &s, 21);
    build_ext(&h, &t, &so, &sp, &gl);
    CHECK(so.vcount > 1000 && gl.vcount > 0 && sp.vcount > 0); /* flowers by the porch */
    CHECK(mesh_ok(&so) && mesh_ok(&sp) && mesh_ok(&gl));
    vec3 mn, mx;
    mtn_mesh_bounds(&so, &mn, &mx);
    CHECK(mx.y > h.floor_y + 5.3f && mx.y <= h.chimney_top.y + 1e-3f); /* chimney is the top */
    CHECK(mn.y < h.floor_y && mn.y > h.floor_y - 1.5f);
    CHECK(mn.x > 64 - 5 && mx.x < 64 + 5);
    CHECK(count_tile(&gl, 13) == 12); /* three glowing windows */
    CHECK(count_tile(&so, 16) == 24); /* six moss patches in a damp place */
    CHECK(has_color(&so, 5, 235, 228, 215)); /* forest stone */
    counts[MTN_HV_COTTAGE] = so.vcount;
    /* building is deterministic */
    mtn_mesh so2, sp2, gl2;
    build_ext(&h, &t, &so2, &sp2, &gl2);
    CHECK(so2.vcount == so.vcount && so2.icount == so.icount && sp2.vcount == sp.vcount);
    CHECK(memcmp(so2.v, so.v, sizeof(mtn_vertex) * (size_t)so.vcount) == 0);
    free3(&so2, &sp2, &gl2);
    free3(&so, &sp, &gl);
    /* dry rocky ground: pale yellow-grey lichen instead of green moss, grey stone; snow: pale stone */
    fill_terrain(&t, 0.2f, MTN_BIOME_ROCK);
    build_ext(&h, &t, &so, &sp, &gl);
    CHECK(count_tile(&so, 16) > 0);
    CHECK(has_color(&so, 16, 215, 205, 150));
    CHECK(!has_color(&so, 16, 255, 255, 255));
    CHECK(has_color(&so, 5, 200, 196, 190));
    CHECK(!has_color(&so, 5, 235, 228, 215));
    free3(&so, &sp, &gl);
    fill_terrain(&t, 0.2f, MTN_BIOME_SNOW);
    build_ext(&h, &t, &so, &sp, &gl);
    CHECK(has_color(&so, 5, 225, 225, 230));
    free3(&so, &sp, &gl);
    /* without a terrain (default stone, stepping stones at the foundation level) */
    build_ext(&h, NULL, &so, &sp, &gl);
    CHECK(mesh_ok(&so) && so.vcount > 1000 && sp.vcount > 0);
    CHECK(has_color(&so, 5, 235, 228, 215));
    free3(&so, &sp, &gl);
    /* while appearing the house is built already */
    mtn_house_summon(&h, &s, 21);
    build_ext(&h, &t, &so, &sp, &gl);
    CHECK(so.vcount > 1000);
    free3(&so, &sp, &gl);
    mtn_terrain_free(&t);

    /* hillside: rocks embedded uphill */
    make_terrain(&t, 100, 0.5f);
    CHECK(mtn_house_find_site(&t, NULL, player_at(&t), 0.0f, &s));
    make_present(&h, &s, 22);
    build_ext(&h, &t, &so, &sp, &gl);
    CHECK(mesh_ok(&so) && mesh_ok(&gl));
    CHECK(sp.vcount == 0); /* no flower bed */
    counts[MTN_HV_HILLSIDE] = so.vcount;
    CHECK(counts[MTN_HV_HILLSIDE] > counts[MTN_HV_COTTAGE]);
    free3(&so, &sp, &gl);
    build_ext(&h, NULL, &so, &sp, &gl); /* rocks need the terrain */
    CHECK(mesh_ok(&so) && so.vcount < counts[MTN_HV_HILLSIDE]);
    free3(&so, &sp, &gl);
    mtn_terrain_free(&t);

    /* stilts over the water */
    make_terrain(&t, 100, 0);
    set_water(&t, 37, TN - 1, 0, TN - 1, 100.5f);
    CHECK(mtn_house_find_site(&t, NULL, player_at(&t), 0.0f, &s));
    make_present(&h, &s, 23);
    CHECK(h.variant == MTN_HV_STILTS);
    build_ext(&h, &t, &so, &sp, &gl);
    CHECK(mesh_ok(&so) && mesh_ok(&gl) && so.vcount > 1000);
    mtn_mesh_bounds(&so, &mn, &mx);
    CHECK(mn.y < h.ground_min - 0.5f); /* posts reach down into the ground */
    counts[MTN_HV_STILTS] = so.vcount;
    free3(&so, &sp, &gl);
    mtn_terrain_free(&t);

    /* tree house */
    make_terrain(&t, 100, 0);
    mtn_flora f;
    make_flora(&f, v3(64, 100, 56), 15.0f, MTN_PLANT_TREE);
    CHECK(mtn_house_find_site(&t, &f, player_at(&t), 0.0f, &s));
    make_present(&h, &s, 24);
    CHECK(h.variant == MTN_HV_TREE && h.tree == 0);
    mtn_mesh_init(&so);
    mtn_mesh_init(&sp);
    mtn_mesh_init(&gl);
    mtn_house_build_exterior(&h, &t, &f, &so, &sp, &gl);
    CHECK(mesh_ok(&so) && mesh_ok(&gl) && so.vcount > 1000);
    counts[MTN_HV_TREE] = so.vcount;
    CHECK(counts[MTN_HV_TREE] != counts[MTN_HV_STILTS]); /* braces instead of posts */
    /* stairs down from the platform: the lowest point is near the ground */
    mtn_mesh_bounds(&so, &mn, &mx);
    CHECK(mn.y < h.floor_y - 3.0f);
    float y = 0;
    vec3 w = to_world(&h, 0, 0, 3.7f + 2.0f);
    CHECK(mtn_house_surface(&h, w.x, w.z, &y));
    CHECK_NEAR(y, h.floor_y - 1.4f, 1e-3);
    free3(&so, &sp, &gl);
    mtn_flora_free(&f);
    mtn_terrain_free(&t);
}

static void test_door_mesh(void) {
    mtn_mesh m;
    mtn_mesh_init(&m);
    mtn_house_build_door(&m);
    CHECK(m.vcount == 2 * 4 + 3 * 24);
    CHECK(m.icount == 2 * 6 + 3 * 36);
    CHECK(mesh_ok(&m));
    vec3 mn, mx;
    mtn_mesh_bounds(&m, &mn, &mx);
    CHECK_NEAR(mn.x, 0, 1e-5);
    CHECK_NEAR(mx.x, MTN_DOOR_WIDTH, 1e-5);
    CHECK_NEAR(mn.y, 0, 1e-5);
    CHECK_NEAR(mx.y, MTN_DOOR_HEIGHT, 1e-5);
    CHECK_NEAR(mn.z, -0.08f, 1e-5);
    CHECK_NEAR(mx.z, 0.08f, 1e-5);
    mtn_mesh_free(&m);

    mtn_terrain t;
    mtn_house_site s;
    static mtn_house h;
    make_terrain(&t, 100, 0);
    CHECK(mtn_house_find_site(&t, NULL, player_at(&t), 0.0f, &s));
    make_present(&h, &s, 5);
    /* exterior: closed door fills the doorway, hinge on the left (-x local) */
    mat4 e = mtn_house_door_model(&h, 0);
    vec3 hinge = to_local(&h, m4_mul_point(e, v3(0, 0, 0)));
    vec3 tip = to_local(&h, m4_mul_point(e, v3(MTN_DOOR_WIDTH, MTN_DOOR_HEIGHT, 0)));
    CHECK_NEAR(hinge.x, -0.55f, 1e-4);
    CHECK_NEAR(hinge.z, 2.5f, 1e-4);
    CHECK_NEAR(tip.x, 0.55f, 1e-4);
    CHECK_NEAR(tip.y, MTN_DOOR_HEIGHT, 1e-4);
    CHECK_NEAR(tip.z, 2.5f, 1e-4);
    /* interior: hinge on +x, leaf towards -x */
    mat4 i0 = mtn_house_door_model(&h, 1);
    vec3 ih = m4_mul_point(i0, v3(0, 0, 0)), it = m4_mul_point(i0, v3(MTN_DOOR_WIDTH, 0, 0));
    CHECK_NEAR(ih.x, 0.55f, 1e-4);
    CHECK_NEAR(it.x, -0.55f, 1e-4);
    CHECK_NEAR(it.z, 0, 1e-4);
    /* opened: the leaf swings outwards (exterior +z, interior -z) and keeps its length */
    mtn_house_toggle_door(&h);
    mtn_house_update(&h, 1.0f);
    e = mtn_house_door_model(&h, 0);
    hinge = to_local(&h, m4_mul_point(e, v3(0, 0, 0)));
    tip = to_local(&h, m4_mul_point(e, v3(MTN_DOOR_WIDTH, 0, 0)));
    CHECK_NEAR(hinge.x, -0.55f, 1e-4);
    CHECK(tip.z > 2.5f + 1.0f);
    CHECK_NEAR(v3_dist(hinge, tip), MTN_DOOR_WIDTH, 1e-4);
    i0 = mtn_house_door_model(&h, 1);
    it = m4_mul_point(i0, v3(MTN_DOOR_WIDTH, 0, 0));
    CHECK(it.z < -1.0f);
    CHECK_NEAR(v3_dist(m4_mul_point(i0, v3(0, 0, 0)), it), MTN_DOOR_WIDTH, 1e-4);
    mtn_terrain_free(&t);
}

/* A real generated mountain: the trailhead always offers somewhere to settle. */
static void test_generated_terrain(void) {
    mtn_terrain t;
    mtn_terrain_params p;
    mtn_terrain_default_params(&p);
    p.size = 129;
    p.cell = 16;
    p.erosion_drops = 3000;
    CHECK(mtn_terrain_generate(&t, &p));
    static mtn_house h;
    int found = 0;
    for (int k = 0; k < 4; k++) {
        mtn_house_site s;
        vec3 pl = t.trailhead;
        if (!mtn_house_find_site(&t, NULL, pl, (float)k * MTN_PI * 0.5f, &s)) continue;
        found++;
        CHECK(s.ok);
        CHECK(s.variant >= 0 && s.variant < MTN_HV_COUNT);
        CHECK(s.ground_max - s.ground_min <= 9.0f);
        CHECK(v3_dist_xz(s.pos, pl) >= 5.5f && v3_dist_xz(s.pos, pl) <= 17.5f);
        CHECK(s.pos.y > s.ground_min - 1.0f && s.pos.y < s.ground_max + 4.0f);
        make_present(&h, &s, (uint32_t)k);
        mtn_mesh so, sp, gl;
        build_ext(&h, &t, &so, &sp, &gl);
        CHECK(mesh_ok(&so) && mesh_ok(&sp) && mesh_ok(&gl) && so.vcount > 0);
        free3(&so, &sp, &gl);
        /* the real viewpoints are used by the windows of later rooms */
        mtn_room r;
        mtn_mesh_init(&so);
        mtn_mesh_init(&sp);
        mtn_mesh_init(&gl);
        mtn_house_build_room(&h, &t, 2, &r, &so, &sp, &gl);
        mat4 ww = mtn_house_window_to_world(&h, &t, &r);
        vec3 m = m4_mul_point(ww, m4_mul_point(r.window_frame, v3(0, 0, 0)));
        const mtn_viewpoint *vp = &t.vps[r.view % t.vp_count];
        CHECK_NEAR(v3_dist(m, v3_add(vp->pos, v3(0, 0.2f, 0))), 0, 0.05);
        free3(&so, &sp, &gl);
    }
    CHECK(found >= 1);
    mtn_terrain_free(&t);
}

MTN_SUITE(house) {
    TEST_RUN(test_defaults);
    TEST_RUN(test_site_cottage);
    TEST_RUN(test_site_hillside);
    TEST_RUN(test_site_rejected);
    TEST_RUN(test_site_stilts);
    TEST_RUN(test_site_tree);
    TEST_RUN(test_lifecycle);
    TEST_RUN(test_surfaces);
    TEST_RUN(test_collision);
    TEST_RUN(test_portals);
    TEST_RUN(test_exterior_variants);
    TEST_RUN(test_door_mesh);
    TEST_RUN(test_generated_terrain);
}
