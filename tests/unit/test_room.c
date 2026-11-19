#include "test.h"
#include "mtn/house.h"

#include <stdlib.h>

/* every prefab the interior knows about */
static const char *PREFABS[] = {"bed",       "kitchen", "desk",    "bookshelf", "fireplace",  "armchair",    "rug",
                                "plant",     "lamp",    "table",   "chair",     "tea_table",  "bathtub",     "piano",
                                "telescope", "planter", "chest",   "candles",   "photo",      "star_chart",  "window_seat",
                                "barrel",    "basket"};
#define NPREFABS ((int)(sizeof PREFABS / sizeof PREFABS[0]))

typedef struct {
    const char *name;
    int collider, light, photo, fire, sprites, glow;
} prefab_expect;

static const prefab_expect EXPECT[] = {
    {"bed", 1, 0, 0, 0, 0, 0},        {"kitchen", 1, 0, 0, 0, 0, 1},  {"desk", 1, 1, 0, 0, 0, 1},
    {"bookshelf", 1, 0, 0, 0, 0, 0},  {"fireplace", 1, 1, 0, 1, 0, 1}, {"armchair", 1, 0, 0, 0, 0, 0},
    {"rug", 0, 0, 0, 0, 0, 0},        {"plant", 1, 0, 0, 0, 1, 0},    {"lamp", 0, 1, 0, 0, 0, 1},
    {"table", 1, 0, 0, 0, 0, 0},      {"chair", 1, 0, 0, 0, 0, 0},    {"tea_table", 1, 0, 0, 0, 0, 0},
    {"bathtub", 1, 0, 0, 0, 0, 0},    {"piano", 1, 0, 0, 0, 0, 0},    {"telescope", 1, 0, 0, 0, 0, 0},
    {"planter", 1, 0, 0, 0, 1, 0},    {"chest", 1, 0, 0, 0, 0, 0},    {"candles", 1, 1, 0, 0, 0, 1},
    {"photo", 0, 0, 1, 0, 0, 0},      {"star_chart", 0, 0, 0, 0, 0, 0}, {"window_seat", 1, 0, 0, 0, 0, 0},
    {"barrel", 1, 0, 0, 0, 0, 0},     {"basket", 0, 0, 0, 0, 0, 0},
};

typedef struct {
    mtn_mesh solid, sprites, glow;
} meshes;

static void m_init(meshes *m) {
    mtn_mesh_init(&m->solid);
    mtn_mesh_init(&m->sprites);
    mtn_mesh_init(&m->glow);
}
static void m_free(meshes *m) {
    mtn_mesh_free(&m->solid);
    mtn_mesh_free(&m->sprites);
    mtn_mesh_free(&m->glow);
}

static int mesh_ok(const mtn_mesh *m) {
    if (m->icount % 3 != 0) return 0;
    for (int i = 0; i < m->vcount; i++) {
        const mtn_vertex *v = &m->v[i];
        if (!isfinite(v->x) || !isfinite(v->y) || !isfinite(v->z) || !isfinite(v->nx) || !isfinite(v->ny) || !isfinite(v->nz))
            return 0;
    }
    for (int i = 0; i < m->icount; i++)
        if (m->idx[i] >= (uint32_t)m->vcount) return 0;
    return 1;
}

static void named_type(mtn_room_type *t, const char *name, int cabin) {
    mtn_room_type_defaults(t);
    snprintf(t->name, sizeof t->name, "%s", name);
    t->is_cabin = cabin;
}

static void add_types(mtn_house *h, int cabin_at, int others) {
    mtn_room_type t;
    char name[32];
    for (int i = 0; i < others + (cabin_at >= 0); i++) {
        int cabin = i == cabin_at;
        if (cabin) snprintf(name, sizeof name, "cabin");
        else snprintf(name, sizeof name, "type%d", i);
        named_type(&t, name, cabin);
        mtn_house_add_room_type(h, &t);
    }
}

/* ---------------------------------------------------------------- sequence */

static void test_sequence(void) {
    static mtn_house h;
    mtn_house_init(&h);
    h.seed = 1234;
    /* no types at all */
    CHECK(mtn_house_room_type_for(&h, 0) == -1);
    CHECK(mtn_house_room_type_for(&h, 5) == -1);

    /* cabin listed second, four other rooms */
    add_types(&h, 1, 4);
    CHECK(h.type_count == 5);
    CHECK(mtn_house_room_type_for(&h, 0) == 1);
    CHECK(mtn_house_room_type_for(&h, -3) == 1);
    int prev = mtn_house_room_type_for(&h, 1);
    CHECK(prev != 1 && prev >= 0 && prev < 5);
    int repeats = 0, cabins = 0, counts[5] = {0};
    for (int i = 1; i <= 500; i++) {
        int ty = mtn_house_room_type_for(&h, i);
        CHECK(ty >= 0 && ty < 5);
        if (ty < 0 || ty >= 5) continue;
        counts[ty]++;
        if (ty == 1) cabins++;
        if (i > 1 && ty == prev) repeats++;
        prev = ty;
    }
    CHECK(repeats == 0); /* consecutive rooms never repeat */
    CHECK(cabins == 0);  /* the cabin is only the first room */
    CHECK(counts[0] == 125 && counts[2] == 125 && counts[3] == 125 && counts[4] == 125);
    /* every block of k rooms is a permutation of the non-cabin types */
    for (int b = 0; b < 50; b++) {
        int seen = 0;
        for (int p = 0; p < 4; p++) seen |= 1 << mtn_house_room_type_for(&h, 1 + b * 4 + p);
        CHECK(seen == (1 | 4 | 8 | 16));
    }
    /* deterministic for a seed, different for another */
    static mtn_house h2;
    h2 = h;
    int same = 1;
    for (int i = 0; i < 40; i++) same &= mtn_house_room_type_for(&h, i) == mtn_house_room_type_for(&h2, i);
    CHECK(same);
    h2.seed = 99;
    int diff = 0;
    for (int i = 1; i < 40; i++) diff += mtn_house_room_type_for(&h, i) != mtn_house_room_type_for(&h2, i);
    CHECK(diff > 0);
    CHECK(mtn_house_room_type_for(&h2, 0) == 1);

    /* only two non-cabin types: strict alternation */
    mtn_house_init(&h);
    h.seed = 5;
    add_types(&h, 0, 2);
    for (int i = 1; i < 200; i++) CHECK(mtn_house_room_type_for(&h, i) != mtn_house_room_type_for(&h, i + 1));
    CHECK(mtn_house_room_type_for(&h, 0) == 0);

    /* any number of types >= 2, with or without a cabin, many seeds: never twice in a row */
    for (int k = 2; k <= 6; k++)
        for (int cab = -1; cab <= 0; cab++)
            for (uint32_t seed = 1; seed <= 4; seed++) {
                mtn_house_init(&h);
                h.seed = seed * 7919u;
                add_types(&h, cab, k);
                int bad = 0, last = mtn_house_room_type_for(&h, 1);
                for (int i = 2; i <= 500; i++) {
                    int ty = mtn_house_room_type_for(&h, i);
                    bad += ty == last || ty < 0 || ty >= h.type_count || (cab == 0 && ty == 0);
                    last = ty;
                }
                if (bad) fprintf(stderr, "  k=%d cabin=%d seed=%u: %d repeats\n", k, cab, h.seed, bad);
                CHECK(bad == 0);
            }

    /* a single non-cabin type repeats forever */
    mtn_house_init(&h);
    add_types(&h, 0, 1);
    CHECK(mtn_house_room_type_for(&h, 0) == 0);
    for (int i = 1; i < 20; i++) CHECK(mtn_house_room_type_for(&h, i) == 1);

    /* only a cabin: every room is the cabin */
    mtn_house_init(&h);
    add_types(&h, 0, 0);
    CHECK(h.type_count == 1);
    for (int i = 0; i < 10; i++) CHECK(mtn_house_room_type_for(&h, i) == 0);

    /* no cabin flagged: room 0 is the first type, all types take turns afterwards */
    mtn_house_init(&h);
    h.seed = 77;
    add_types(&h, -1, 3);
    CHECK(h.type_count == 3);
    CHECK(mtn_house_room_type_for(&h, 0) == 0);
    int seen = 0;
    for (int i = 1; i <= 3; i++) seen |= 1 << mtn_house_room_type_for(&h, i);
    CHECK(seen == 7);
    for (int i = 1; i < 100; i++) CHECK(mtn_house_room_type_for(&h, i) != mtn_house_room_type_for(&h, i + 1));
}

static void test_room_at(void) {
    CHECK(mtn_house_room_at(-5.0f) == 0);
    CHECK(mtn_house_room_at(-0.01f) == 0);
    CHECK(mtn_house_room_at(0.0f) == 0);
    CHECK(mtn_house_room_at(8.99f) == 0);
    CHECK(mtn_house_room_at(9.0f) == 1);
    CHECK(mtn_house_room_at(9.0f * 7 + 4.5f) == 7);
    CHECK(mtn_house_room_at(1000.0f) == 111);
}

/* ---------------------------------------------------------------- shell */

static void test_shell(void) {
    static mtn_house h;
    mtn_house_init(&h);
    h.seed = 200;
    meshes m;
    mtn_room r;
    /* without room types a default room is built */
    m_init(&m);
    mtn_house_build_room(&h, NULL, 0, &r, &m.solid, &m.sprites, &m.glow);
    CHECK(r.type == -1 && r.index == 0);
    CHECK_NEAR(r.width, 8, 0);
    CHECK_NEAR(r.height, 3.6f, 1e-6);
    CHECK_NEAR(r.z0, 0, 0);
    CHECK_NEAR(r.length, MTN_ROOM_LENGTH, 0);
    CHECK(r.view == -1);
    CHECK(r.collider_count == 6); /* 2 per end wall + 2 side walls */
    CHECK(r.light_count == 1);    /* the ceiling lamp */
    CHECK(r.photo_count == 0 && !r.has_fire);
    CHECK_NEAR(r.lights[0].pos.y, 3.6f - 0.8f, 1e-5);
    CHECK_NEAR(r.lights[0].pos.z, 4.5f, 1e-5);
    CHECK_NEAR(r.lights[0].radius, 9, 0);
    CHECK(m.solid.vcount > 100 && m.glow.vcount > 0 && m.sprites.vcount == 0);
    CHECK(mesh_ok(&m.solid) && mesh_ok(&m.glow));
    vec3 mn, mx;
    mtn_mesh_bounds(&m.solid, &mn, &mx);
    CHECK_NEAR(mn.x, -4, 1e-4);
    CHECK_NEAR(mx.x, 4, 1e-4);
    CHECK_NEAR(mn.y, 0, 1e-4);
    CHECK_NEAR(mx.y, 3.6f, 1e-4);
    CHECK_NEAR(mn.z, 0, 1e-4);
    CHECK_NEAR(mx.z, 9, 1e-4);
    /* window on the right wall (+x), looking out */
    vec3 wc = m4_mul_point(r.window_frame, v3(0, 0, 0));
    vec3 wf = m4_mul_dir(r.window_frame, v3(0, 0, 1));
    CHECK_NEAR(wc.x, 4, 1e-5);
    CHECK_NEAR(wc.y, (0.8f + 3.0f) * 0.5f, 1e-5);
    CHECK_NEAR(wc.z, 4.5f, 1e-5);
    CHECK_NEAR(wf.x, 1, 1e-5);
    CHECK_NEAR(r.window_size.x, 2, 0);
    CHECK_NEAR(r.window_size.y, 1.1f, 1e-5);
    /* the wall colliders leave a door-wide gap at both ends */
    for (int i = 0; i < 4; i++) {
        const mtn_aabb *b = &r.colliders[i];
        CHECK(b->mx.x <= -MTN_DOOR_WIDTH * 0.5f + 1e-5f || b->mn.x >= MTN_DOOR_WIDTH * 0.5f - 1e-5f);
        CHECK(b->mn.y == 0 && b->mx.y == 3.6f);
    }
    m_free(&m);

    /* a low narrow room type further down the corridor */
    mtn_room_type t;
    named_type(&t, "attic", 0);
    t.width = 5;
    t.height = 3.0f;
    t.light_color = v3(0.2f, 0.3f, 0.4f);
    mtn_house_add_room_type(&h, &t);
    named_type(&t, "cabin", 1);
    mtn_house_add_room_type(&h, &t);
    m_init(&m);
    mtn_house_build_room(&h, NULL, 3, &r, &m.solid, &m.sprites, &m.glow);
    CHECK(r.type == 0 && r.index == 3);
    CHECK_NEAR(r.z0, 27, 0);
    CHECK_NEAR(r.width, 5, 0);
    CHECK(r.view == (int)(200u % 97u) + 2);
    CHECK_NEAR(r.window_size.y, (2.6f - 0.8f) * 0.5f, 1e-5); /* window top below the ceiling */
    CHECK_NEAR(r.lights[0].color.z, 0.4f, 1e-6);
    mtn_mesh_bounds(&m.solid, &mn, &mx);
    CHECK_NEAR(mn.z, 27, 1e-4);
    CHECK_NEAR(mx.z, 36, 1e-4);
    CHECK_NEAR(mx.x, 2.5f, 1e-4);
    CHECK(mtn_room_contains(&r, v3(0, 1, 30)));
    CHECK(mtn_room_contains(&r, v3(2.5f, 1, 27)));  /* edges are inside */
    CHECK(mtn_room_contains(&r, v3(-2.5f, 1, 36)));
    CHECK(!mtn_room_contains(&r, v3(2.6f, 1, 30)));
    CHECK(!mtn_room_contains(&r, v3(-2.6f, 1, 30)));
    CHECK(!mtn_room_contains(&r, v3(0, 1, 26.9f)));
    CHECK(!mtn_room_contains(&r, v3(0, 1, 36.1f)));
    m_free(&m);
    /* room 0 is the cabin */
    m_init(&m);
    mtn_house_build_room(&h, NULL, 0, &r, &m.solid, &m.sprites, &m.glow);
    CHECK(r.type == 1 && r.view == -1);
    CHECK_NEAR(r.width, 8, 0);
    m_free(&m);
}

/* ---------------------------------------------------------------- prefabs */

static void build_single(mtn_house *h, const char *prefab, int arg, mtn_room *r, meshes *m) {
    mtn_house_init(h);
    h->seed = 3;
    mtn_room_type t;
    named_type(&t, "cabin", 1);
    if (prefab) mtn_room_add_item(&t, prefab, 0.5f, 4.5f, 0, 1, arg);
    mtn_house_add_room_type(h, &t);
    m_init(m);
    mtn_house_build_room(h, NULL, 0, r, &m->solid, &m->sprites, &m->glow);
}

static void test_prefabs(void) {
    static mtn_house h;
    meshes base, m;
    mtn_room r0, r;
    build_single(&h, NULL, 0, &r0, &base);
    CHECK(NPREFABS == (int)(sizeof EXPECT / sizeof EXPECT[0]));
    for (int i = 0; i < NPREFABS; i++) {
        const prefab_expect *e = &EXPECT[i];
        CHECK_STR(PREFABS[i], e->name);
        build_single(&h, e->name, 2, &r, &m);
        int ok = 1;
        ok &= r.collider_count == r0.collider_count + e->collider;
        ok &= r.light_count == r0.light_count + e->light;
        ok &= r.photo_count == e->photo;
        ok &= r.has_fire == e->fire;
        ok &= (m.sprites.vcount > 0) == e->sprites;
        ok &= (m.glow.vcount > base.glow.vcount) == e->glow;
        ok &= m.solid.vcount > base.solid.vcount;
        ok &= mesh_ok(&m.solid) && mesh_ok(&m.sprites) && mesh_ok(&m.glow);
        if (!ok) fprintf(stderr, "  prefab '%s' does not match its expectations\n", e->name);
        CHECK(ok);
        /* furniture stays inside the room */
        vec3 mn, mx;
        mtn_mesh_bounds(&m.solid, &mn, &mx);
        CHECK(mn.x >= -4.0f - 1e-4f && mx.x <= 4.0f + 1e-4f && mx.y <= 4.0f + 1e-4f);
        if (e->collider) {
            const mtn_aabb *b = &r.colliders[r.collider_count - 1];
            /* furniture colliders sit on the floor around the item position */
            CHECK(b->mn.y == 0 && b->mx.y > 0.3f);
            CHECK(b->mn.x < 0.5f && b->mx.x > 0.5f && b->mn.z < 4.5f && b->mx.z > 4.5f);
        }
        m_free(&m);
    }
    /* unknown prefab: nothing is added */
    build_single(&h, "spaceship", 0, &r, &m);
    CHECK(m.solid.vcount == base.solid.vcount && m.glow.vcount == base.glow.vcount && m.sprites.vcount == 0);
    CHECK(r.collider_count == r0.collider_count && r.light_count == r0.light_count);
    m_free(&m);

    /* details: the fireplace, photos, lights */
    build_single(&h, "fireplace", 0, &r, &m);
    CHECK(r.has_fire);
    CHECK_NEAR(r.fire_pos.x, 0.5f, 1e-5);
    CHECK_NEAR(r.fire_pos.y, 0.5f, 1e-5);
    CHECK_NEAR(r.fire_pos.z, 4.5f, 1e-5);
    CHECK_NEAR(r.lights[1].flicker, 1.0f, 0);
    CHECK_NEAR(r.lights[1].radius, 7.0f, 0);
    m_free(&m);
    build_single(&h, "photo", 5, &r, &m);
    CHECK(r.photo_count == 1);
    CHECK(r.photos[0].photo == 5);
    CHECK_NEAR(r.photos[0].center.x, 0.5f, 1e-5);
    CHECK_NEAR(r.photos[0].center.y, 1.6f, 1e-5);
    CHECK_NEAR(r.photos[0].center.z, 4.5f - 0.02f, 1e-5);
    CHECK_NEAR(r.photos[0].right.x, 1, 1e-5);
    CHECK_NEAR(r.photos[0].up.y, 1, 0);
    m_free(&m);
    build_single(&h, "lamp", 0, &r, &m);
    CHECK_NEAR(r.lights[1].pos.x, 0.5f, 1e-5);
    CHECK_NEAR(r.lights[1].pos.y, 1.6f, 1e-5);
    CHECK_NEAR(r.lights[1].flicker, 0, 0);
    m_free(&m);
    /* rug size from its argument */
    vec3 mn, mx;
    build_single(&h, "rug", 0, &r, &m);
    int nb = base.solid.vcount;
    CHECK(m.solid.vcount == nb + 24);
    mn = mx = v3(m.solid.v[nb].x, m.solid.v[nb].y, m.solid.v[nb].z);
    for (int i = nb; i < m.solid.vcount; i++) {
        mn = v3(mtn_minf(mn.x, m.solid.v[i].x), mtn_minf(mn.y, m.solid.v[i].y), mtn_minf(mn.z, m.solid.v[i].z));
        mx = v3(mtn_maxf(mx.x, m.solid.v[i].x), mtn_maxf(mx.y, m.solid.v[i].y), mtn_maxf(mx.z, m.solid.v[i].z));
    }
    CHECK_NEAR(mx.x - mn.x, 3.0f, 1e-4);
    CHECK_NEAR(mx.z - mn.z, 2.1f, 1e-4);
    CHECK_NEAR(mx.y, 0.02f, 1e-5);
    m_free(&m);
    build_single(&h, "rug", 4, &r, &m);
    mn = mx = v3(m.solid.v[nb].x, 0, m.solid.v[nb].z);
    for (int i = nb; i < m.solid.vcount; i++) {
        mn.x = mtn_minf(mn.x, m.solid.v[i].x);
        mx.x = mtn_maxf(mx.x, m.solid.v[i].x);
    }
    CHECK_NEAR(mx.x - mn.x, 4.0f, 1e-4);
    m_free(&m);
    /* plants use one of two leaf tiles depending on the seed */
    build_single(&h, "plant", 0, &r, &m);
    CHECK(m.sprites.vcount == 5 * 4);
    CHECK(m.sprites.v[0].tile == 8 || m.sprites.v[0].tile == 9);
    m_free(&m);
    m_free(&base);
}

static void test_item_transform_and_caps(void) {
    static mtn_house h;
    mtn_house_init(&h);
    h.seed = 9;
    mtn_room_type t;
    named_type(&t, "store", 0);
    /* a chest turned by 90 degrees and scaled 2x */
    mtn_room_add_item(&t, "chest", 2.0f, 3.0f, 90.0f, 2.0f, 0);
    for (int i = 0; i < 10; i++) mtn_room_add_item(&t, "lamp", -3.0f, 0.5f + 0.8f * (float)i, 0, 1, 0);
    for (int i = 0; i < 10; i++) mtn_room_add_item(&t, "photo", 3.9f, 0.5f + 0.8f * (float)i, -90, 1, i);
    mtn_house_add_room_type(&h, &t);
    meshes m;
    mtn_room r;
    m_init(&m);
    mtn_house_build_room(&h, NULL, 2, &r, &m.solid, &m.sprites, &m.glow);
    CHECK(r.type == 0);
    CHECK(r.collider_count == 7);
    const mtn_aabb *b = &r.colliders[6];
    CHECK_NEAR(b->mn.x, 2.0f - 0.6f, 1e-4);
    CHECK_NEAR(b->mx.x, 2.0f + 0.6f, 1e-4);
    CHECK_NEAR(b->mn.z, 18.0f + 3.0f - 1.0f, 1e-4);
    CHECK_NEAR(b->mx.z, 18.0f + 3.0f + 1.0f, 1e-4);
    CHECK_NEAR(b->mn.y, 0, 1e-5);
    CHECK_NEAR(b->mx.y, 1.2f, 1e-4);
    /* caps: 8 lights (ceiling + 7 lamps), 8 photo slots */
    CHECK(r.light_count == 8);
    CHECK_NEAR(r.lights[7].pos.z, 18.0f + 0.5f + 0.8f * 6.0f, 1e-4);
    CHECK(r.photo_count == 8);
    CHECK(r.photos[7].photo == 7);
    /* photos hang on the right wall facing into the room (2 cm in front of the item origin) */
    CHECK_NEAR(r.photos[0].center.x, 3.92f, 1e-4);
    CHECK_NEAR(fabsf(r.photos[0].right.z), 1, 1e-4);
    CHECK(mesh_ok(&m.solid) && mesh_ok(&m.glow));
    m_free(&m);
}

/* ---------------------------------------------------------------- collision */

static void test_collide_box(void) {
    mtn_room r;
    memset(&r, 0, sizeof r);
    r.collider_count = 2;
    r.colliders[0].mn = v3(-1, 0, -1);
    r.colliders[0].mx = v3(1, 1, 1);
    r.colliders[1].mn = v3(5, 2.0f, 5); /* a shelf high on the wall */
    r.colliders[1].mx = v3(6, 2.5f, 6);
    const float rad = 0.3f;
    vec3 p;
    p = v3(-1.1f, 0, 0.2f);
    CHECK(mtn_room_collide(&r, &p, rad));
    CHECK_NEAR(p.x, -1.3f, 1e-5);
    CHECK_NEAR(p.z, 0.2f, 1e-6);
    p = v3(1.1f, 0, -0.2f);
    CHECK(mtn_room_collide(&r, &p, rad));
    CHECK_NEAR(p.x, 1.3f, 1e-5);
    p = v3(0.2f, 0, -1.05f);
    CHECK(mtn_room_collide(&r, &p, rad));
    CHECK_NEAR(p.z, -1.3f, 1e-5);
    CHECK_NEAR(p.x, 0.2f, 1e-6);
    p = v3(-0.2f, 0, 1.2f);
    CHECK(mtn_room_collide(&r, &p, rad));
    CHECK_NEAR(p.z, 1.3f, 1e-5);
    /* touching the expanded edge is not a contact */
    p = v3(-1.3f, 0, 0);
    CHECK(!mtn_room_collide(&r, &p, rad));
    p = v3(0, 0, 1.31f);
    CHECK(!mtn_room_collide(&r, &p, rad));
    /* standing on top of the box (its top below the knees) is free */
    p = v3(0, 0.8f, 0);
    CHECK(!mtn_room_collide(&r, &p, rad));
    CHECK_NEAR(p.x, 0, 0);
    /* a collider above the head is ignored, one at head height is not */
    p = v3(5.5f, 0.0f, 5.5f);
    CHECK(!mtn_room_collide(&r, &p, rad));
    p = v3(5.5f, 0.5f, 5.1f);
    CHECK(mtn_room_collide(&r, &p, rad));
    CHECK_NEAR(p.z, 4.7f, 1e-5);
    /* no colliders */
    r.collider_count = 0;
    p = v3(0, 0, 0);
    CHECK(!mtn_room_collide(&r, &p, rad));
}

static void test_collide_room(void) {
    static mtn_house h;
    mtn_house_init(&h);
    h.seed = 1;
    mtn_room_type t;
    named_type(&t, "cabin", 1);
    mtn_room_add_item(&t, "bed", -2.0f, 3.0f, 0, 1, 0);
    mtn_house_add_room_type(&h, &t);
    named_type(&t, "hall", 0);
    mtn_house_add_room_type(&h, &t);
    meshes m;
    mtn_room r;
    m_init(&m);
    mtn_house_build_room(&h, NULL, 0, &r, &m.solid, &m.sprites, &m.glow);
    const float rad = 0.3f;
    vec3 p;
    /* walking through the doorways at both ends */
    for (float z = -0.5f; z <= 9.5f; z += 0.25f) {
        p = v3(0, 0, z);
        CHECK(!mtn_room_collide(&r, &p, rad));
        CHECK(p.x == 0 && p.z == z);
    }
    /* but not through the end walls beside them */
    p = v3(-2.0f, 0, 0.2f);
    CHECK(mtn_room_collide(&r, &p, rad));
    CHECK_NEAR(p.z, 0.45f, 1e-5);
    p = v3(2.0f, 0, -0.2f); /* from the previous room */
    CHECK(mtn_room_collide(&r, &p, rad));
    CHECK_NEAR(p.z, -0.45f, 1e-5);
    p = v3(2.0f, 0, 8.9f);
    CHECK(mtn_room_collide(&r, &p, rad));
    CHECK_NEAR(p.z, 9.0f - 0.45f, 1e-5);
    /* side walls */
    p = v3(-3.9f, 0, 6.0f);
    CHECK(mtn_room_collide(&r, &p, rad));
    CHECK_NEAR(p.x, -3.7f, 1e-5);
    p = v3(3.95f, 0, 6.0f);
    CHECK(mtn_room_collide(&r, &p, rad));
    CHECK_NEAR(p.x, 3.7f, 1e-5);
    /* the bed (x -3.25..-0.75, z 1.55..4.35) */
    p = v3(-0.6f, 0, 3.0f);
    CHECK(mtn_room_collide(&r, &p, rad));
    CHECK_NEAR(p.x, -0.75f + rad, 1e-4);
    p = v3(-2.0f, 0, 4.5f);
    CHECK(mtn_room_collide(&r, &p, rad));
    CHECK_NEAR(p.z, 4.35f + rad, 1e-4);
    /* open floor */
    p = v3(1.5f, 0, 6.0f);
    CHECK(!mtn_room_collide(&r, &p, rad));
    CHECK(mtn_room_contains(&r, p));
    m_free(&m);
}

MTN_SUITE(room) {
    TEST_RUN(test_sequence);
    TEST_RUN(test_room_at);
    TEST_RUN(test_shell);
    TEST_RUN(test_prefabs);
    TEST_RUN(test_item_transform_and_caps);
    TEST_RUN(test_collide_box);
    TEST_RUN(test_collide_room);
}
