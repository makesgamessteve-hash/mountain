#include "test.h"
#include "mtn/house.h"

#include <math.h>
#include <stdlib.h>

/* every prefab the interior knows about */
static const char *PREFABS[] = {"bed",         "kitchen",     "desk",       "bookshelf", "fireplace", "armchair",  "rug",
                                "plant",       "lamp",        "table",      "chair",     "tea_table", "bathtub",   "piano",
                                "telescope",   "planter",     "chest",      "candles",   "photo",     "star_chart", "window_seat",
                                "barrel",      "basket",      "sauna_stove", "sauna_bench", "bucket", "long_table", "sofa",
                                "wardrobe",    "dresser",     "workbench",  "tool_rack", "globe",     "chandelier"};
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
    {"barrel", 1, 0, 0, 0, 0, 0},     {"basket", 0, 0, 0, 0, 0, 0},   {"sauna_stove", 1, 1, 0, 0, 0, 1},
    {"sauna_bench", 1, 0, 0, 0, 0, 0}, {"bucket", 0, 0, 0, 0, 0, 0},  {"long_table", 1, 1, 0, 0, 0, 1},
    {"sofa", 1, 0, 0, 0, 0, 0},       {"wardrobe", 1, 0, 0, 0, 0, 0}, {"dresser", 1, 0, 0, 0, 0, 0},
    {"workbench", 1, 0, 0, 0, 0, 0},  {"tool_rack", 0, 0, 0, 0, 0, 0}, {"globe", 1, 0, 0, 0, 0, 0},
    {"chandelier", 0, 1, 0, 0, 0, 1},
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

static const int DI[4] = {1, -1, 0, 0}, DJ[4] = {0, 0, 1, -1};

/* ---------------------------------------------------------------- grid */

static void test_closed_sides(void) {
    static mtn_house h;
    mtn_house_init(&h);
    int seen_h = 0, seen_v = 0;
    for (uint32_t seed = 1; seed <= 6; seed++) {
        h.seed = seed * 7919u;
        /* the cabin's closed side is the front wall (south), shared with the room south of it */
        CHECK(mtn_house_closed_side(&h, 0, 0) == MTN_SIDE_SOUTH);
        CHECK(mtn_house_closed_side(&h, 0, -1) == MTN_SIDE_NORTH);
        for (int cj = -12; cj <= 12; cj++)
            for (int ci = -12; ci <= 12; ci++) {
                int s = mtn_house_closed_side(&h, ci, cj);
                CHECK(s >= 0 && s < 4);
                /* dominoes: the neighbour behind the closed side closes the same wall */
                int back = mtn_house_closed_side(&h, ci + DI[s], cj + DJ[s]);
                CHECK(back == (s ^ 1));
                seen_h |= s == MTN_SIDE_EAST;
                seen_v |= s == MTN_SIDE_NORTH;
            }
    }
    CHECK(seen_h && seen_v); /* both kinds of blocks occur */
    CHECK_STR(mtn_side_name(MTN_SIDE_EAST), "east");
    CHECK_STR(mtn_side_name(MTN_SIDE_SOUTH), "south");
    CHECK_STR(mtn_side_name(7), "none");
}

static void test_types(void) {
    static mtn_house h, h2;
    mtn_house_init(&h);
    h.seed = 1234;
    /* no types at all */
    CHECK(mtn_house_room_type_at(&h, 0, 0) == -1);
    CHECK(mtn_house_room_type_at(&h, 5, -3) == -1);
    /* only a cabin: every room is a cabin room */
    add_types(&h, 0, 0);
    CHECK(mtn_house_room_type_at(&h, 0, 0) == 0);
    CHECK(mtn_house_room_type_at(&h, 3, 4) == 0);
    /* one other type: everywhere except the cabin's cell */
    mtn_house_init(&h);
    h.seed = 1234;
    add_types(&h, 1, 1);
    CHECK(mtn_house_room_type_at(&h, 0, 0) == 1);
    CHECK(mtn_house_room_type_at(&h, 1, 0) == 0);
    CHECK(mtn_house_room_type_at(&h, -7, 9) == 0);
    /* with 2, 3, 5 and 12 types neighbours always differ */
    int counts[4] = {2, 3, 5, 12};
    for (int c = 0; c < 4; c++) {
        mtn_house_init(&h);
        h.seed = 77u + (uint32_t)c;
        add_types(&h, 0, counts[c]);
        int bad = 0, seen = 0;
        for (int cj = -20; cj <= 20; cj++)
            for (int ci = -20; ci <= 20; ci++) {
                int t = mtn_house_room_type_at(&h, ci, cj);
                CHECK(t >= 0 && t <= counts[c]);
                if (ci != 0 || cj != 0) bad += t == 0; /* the cabin type only once */
                seen |= 1 << t;
                bad += t == mtn_house_room_type_at(&h, ci + 1, cj);
                bad += t == mtn_house_room_type_at(&h, ci, cj + 1);
            }
        CHECK(bad == 0);
        CHECK(seen == (1 << (counts[c] + 1)) - 1); /* every type appears */
    }
    /* ten rooms in a row show many different types */
    int seen = 0, n = 0;
    for (int ci = 1; ci <= 10; ci++) seen |= 1 << mtn_house_room_type_at(&h, ci, 3);
    for (int k = 0; k < 16; k++) n += (seen >> k) & 1;
    CHECK(n >= 5);
    /* the same seed gives the same rooms, another seed other rooms */
    mtn_house_init(&h2);
    h2.seed = h.seed;
    add_types(&h2, 0, 12);
    int same = 1, diff = 0;
    for (int ci = -5; ci <= 5; ci++)
        for (int cj = -5; cj <= 5; cj++) {
            same &= mtn_house_room_type_at(&h, ci, cj) == mtn_house_room_type_at(&h2, ci, cj);
            h2.seed = h.seed + 1;
            diff += mtn_house_room_type_at(&h, ci, cj) != mtn_house_room_type_at(&h2, ci, cj);
            h2.seed = h.seed;
        }
    CHECK(same);
    CHECK(diff > 20);
}

static void test_plan(void) {
    static mtn_house h;
    mtn_house_init(&h);
    h.seed = 55;
    mtn_room_plan p;
    /* defaults without types */
    mtn_house_room_plan(&h, 2, -1, &p);
    CHECK(p.type == -1 && p.ci == 2 && p.cj == -1);
    CHECK_NEAR(p.width, 8, 0);
    CHECK_NEAR(p.length, 9, 0);
    CHECK_NEAR(p.height, 3.6f, 1e-6);
    CHECK(!p.balcony);
    vec3 c = mtn_house_cell_center(&h, 2, -1);
    CHECK_NEAR(c.x, 2 * MTN_ROOM_CELL, 1e-4);
    CHECK_NEAR(c.z, -MTN_ROOM_CELL + 4.0f, 1e-4);
    /* a cabin 9 m deep puts the front wall at z = 0 */
    mtn_room_type t;
    named_type(&t, "cabin", 1);
    t.width = 9.0f;
    t.balcony = 1.0f; /* never for the cabin */
    mtn_house_add_room_type(&h, &t);
    named_type(&t, "hall", 0);
    t.width_min = 6.0f;
    t.width_max = 30.0f; /* clamped to the grid */
    t.length_min = 2.0f;
    t.length_max = 5.0f;
    t.height = 5.5f;
    t.balcony = 1.0f;
    mtn_house_add_room_type(&h, &t);
    mtn_house_room_plan(&h, 0, 0, &p);
    CHECK(p.type == 0 && !p.balcony);
    CHECK_NEAR(p.width, 9.0f, 0);
    CHECK_NEAR(p.center.z, 4.5f, 1e-5);
    vec3 front = m4_mul_point(p.xf, v3(p.width * 0.5f, 0, 0));
    CHECK_NEAR(front.z, 0.0f, 1e-5); /* local +x is the closed side: the front wall */
    int sizes = 0;
    float wmin = 1e9f, wmax = 0;
    for (int ci = -6; ci <= 6; ci++)
        for (int cj = -6; cj <= 6; cj++) {
            if (ci == 0 && cj == 0) continue;
            mtn_house_room_plan(&h, ci, cj, &p);
            CHECK(p.type == 1 && p.balcony);
            CHECK(p.width >= 6.0f && p.width <= MTN_ROOM_MAX_SIZE);
            CHECK(p.length >= MTN_ROOM_MIN_SIZE && p.length <= 5.0f);
            CHECK_NEAR(p.height, 5.5f, 1e-6);
            wmin = mtn_minf(wmin, p.width);
            wmax = mtn_maxf(wmax, p.width);
            /* local +x points to the closed side */
            vec3 d = m4_mul_dir(p.xf, v3(1, 0, 0));
            CHECK_NEAR(d.x, (float)DI[p.closed], 1e-5);
            CHECK_NEAR(d.z, (float)DJ[p.closed], 1e-5);
            int ri, rj;
            mtn_house_cell_at(&h, v3_add(p.center, v3(2.0f, 0, -3.0f)), &ri, &rj);
            CHECK(ri == ci && rj == cj);
            sizes++;
        }
    CHECK(wmax - wmin > 4.0f); /* sizes vary */
    CHECK(sizes == 168);
    /* a type without a size range keeps its width */
    mtn_house_init(&h);
    named_type(&t, "plain", 0);
    t.width = 7.0f;
    mtn_house_add_room_type(&h, &t);
    mtn_house_room_plan(&h, 1, 1, &p);
    CHECK_NEAR(p.width, 7.0f, 0);
    CHECK_NEAR(p.length, 9.0f, 0);
}

/* ---------------------------------------------------------------- shell */

static void build(const mtn_house *h, int ci, int cj, mtn_room *r, meshes *m) {
    m_init(m);
    mtn_house_build_room(h, NULL, ci, cj, r, &m->solid, &m->sprites, &m->glow);
}

/* walking from a to b in a straight line never collides with the given rooms */
static int clear_walk(const mtn_room *rs, int n, vec3 a, vec3 b) {
    for (float f = 0.0f; f <= 1.0f; f += 0.01f) {
        vec3 p = v3_lerp(a, b, f), q = p;
        for (int i = 0; i < n; i++)
            if (mtn_room_collide(&rs[i], &q, 0.3f)) return 0;
        int inside = 0;
        for (int i = 0; i < n; i++) inside |= mtn_room_contains(&rs[i], p);
        if (!inside) return 0;
    }
    return 1;
}

static void test_shell_and_doors(void) {
    static mtn_house h;
    mtn_house_init(&h);
    h.seed = 200;
    add_types(&h, 0, 5);
    static mtn_room r, nb;
    meshes m, mn;
    for (int ci = -2; ci <= 2; ci++)
        for (int cj = -2; cj <= 2; cj++) {
            build(&h, ci, cj, &r, &m);
            CHECK(r.built && r.ci == ci && r.cj == cj);
            CHECK(mesh_ok(&m.solid) && mesh_ok(&m.glow) && m.solid.vcount > 200);
            CHECK(r.light_count >= 1 && r.light_count <= 8);
            int doors = 0;
            for (int s = 0; s < 4; s++) {
                if (s == r.closed) continue;
                doors++;
                vec3 d = v3((float)DI[s], 0, (float)DJ[s]);
                float half = 0.5f * ((DI[s] != 0) == (r.closed <= MTN_SIDE_WEST) ? r.width : r.length);
                /* through the doorway, the corridor and into the next room */
                build(&h, ci + DI[s], cj + DJ[s], &nb, &mn);
                mtn_room pair[2] = {r, nb};
                CHECK(clear_walk(pair, 2, r.center, nb.center));
                /* the wall beside the doorway is solid */
                vec3 side = v3_add(v3_add(r.center, v3_scale(d, half - 0.1f)), v3(d.z * 0.9f, 0, d.x * 0.9f));
                vec3 q = side;
                CHECK(mtn_room_collide(&r, &q, 0.3f));
                m_free(&mn);
            }
            CHECK(doors == 3);
            /* the closed side: a window looking out (or a balcony) */
            vec3 out = v3((float)DI[r.closed], 0, (float)DJ[r.closed]);
            vec3 wf = m4_mul_dir(r.window_frame, v3(0, 0, 1));
            CHECK_NEAR(v3_dot(wf, out), 1.0f, 1e-4);
            vec3 beyond = v3_add(r.center, v3_scale(out, 0.5f * r.width + (r.balcony ? 3.0f : 0.5f)));
            CHECK(!mtn_room_contains(&r, beyond));
            m_free(&m);
        }
}

static void test_cabin(void) {
    static mtn_house h;
    mtn_house_init(&h);
    h.seed = 3;
    add_types(&h, 0, 3);
    h.types[0].width = 9.0f;
    h.types[0].length_min = h.types[0].length_max = 11.0f;
    static mtn_room r;
    meshes m;
    build(&h, 0, 0, &r, &m);
    CHECK(r.view == -1 && !r.balcony && r.closed == MTN_SIDE_SOUTH);
    CHECK_NEAR(r.center.z, 4.5f, 1e-5);
    /* the front door in the south wall at the origin: open for walking out */
    vec3 p = v3(0, 0, 0.2f);
    CHECK(!mtn_room_collide(&r, &p, 0.3f));
    CHECK(p.z == 0.2f);
    p = v3(-2.0f, 0, 0.2f); /* the wall beside it */
    CHECK(mtn_room_collide(&r, &p, 0.3f));
    CHECK(p.z > 0.3f);
    /* the window beside the front door looks south */
    vec3 wc = m4_mul_point(r.window_frame, v3(0, 0, 0));
    CHECK_NEAR(wc.z, 0, 1e-4);
    CHECK(wc.x > 1.2f && wc.x < 2.8f);
    CHECK_NEAR(m4_mul_dir(r.window_frame, v3(0, 0, 1)).z, -1, 1e-5);
    CHECK(mtn_room_contains(&r, v3(0, 0, 1)) && mtn_room_contains(&r, v3(5.4f, 0, 8.9f)));
    CHECK(!mtn_room_contains(&r, v3(0, 0, -0.3f)));
    m_free(&m);
}

static void test_balcony(void) {
    static mtn_house h;
    mtn_house_init(&h);
    h.seed = 8;
    mtn_room_type t;
    named_type(&t, "cabin", 1);
    mtn_house_add_room_type(&h, &t);
    named_type(&t, "lookout", 0);
    t.balcony = 1.0f;
    t.width_min = t.width_max = 7.0f;
    t.length_min = t.length_max = 8.0f;
    mtn_house_add_room_type(&h, &t);
    static mtn_room r;
    meshes m;
    build(&h, 3, 2, &r, &m);
    CHECK(r.balcony);
    vec3 out = v3((float)DI[r.closed], 0, (float)DJ[r.closed]);
    vec3 side = v3(out.z, 0, out.x);
    /* through the balcony door and out onto the balcony */
    vec3 on = v3_add(r.center, v3_scale(out, 3.5f + 1.0f));
    CHECK(mtn_room_contains(&r, on));
    CHECK(clear_walk(&r, 1, r.center, on));
    /* the railing keeps the player on it */
    vec3 p = v3_add(r.center, v3_scale(out, 3.5f + 1.95f));
    CHECK(mtn_room_collide(&r, &p, 0.3f));
    CHECK(v3_dot(v3_sub(p, r.center), out) < 3.5f + 1.95f);
    p = v3_add(on, v3_scale(side, 1.75f));
    CHECK(mtn_room_collide(&r, &p, 0.3f));
    /* beside the balcony door the wall is solid */
    p = v3_add(v3_add(r.center, v3_scale(out, 3.4f)), v3_scale(side, 2.0f));
    CHECK(mtn_room_collide(&r, &p, 0.3f));
    /* the box of view: five quads around the balcony, all facing inwards */
    vec3 box_c = v3_add(r.center, v3_add(v3_scale(out, 3.5f + 1.0f), v3(0, 1.5f, 0)));
    for (int f = 0; f < 5; f++) {
        const vec3 *q = r.balcony_quads[f];
        vec3 n = v3_norm(v3_cross(v3_sub(q[1], q[0]), v3_sub(q[2], q[0])));
        vec3 qc = v3_scale(v3_add(v3_add(q[0], q[1]), v3_add(q[2], q[3])), 0.25f);
        CHECK(v3_dot(n, v3_sub(box_c, qc)) > 0.5f);
    }
    /* the window frame sits in the wall plane, looking out */
    vec3 wc = m4_mul_point(r.window_frame, v3(0, 0, 0));
    CHECK_NEAR(v3_dot(v3_sub(wc, r.center), out), 3.5f, 1e-4);
    CHECK(mesh_ok(&m.solid) && m.solid.vcount > 400);
    m_free(&m);
}

/* ---------------------------------------------------------------- prefabs */

/* a room whose window side is east, so room-local axes are the interior axes */
static void east_cell(const mtn_house *h, int *ci, int *cj) {
    for (int j = 1; j < 20; j++)
        for (int i = 1; i < 20; i++)
            if (mtn_house_closed_side(h, i, j) == MTN_SIDE_EAST) {
                *ci = i;
                *cj = j;
                return;
            }
}

static void build_single(mtn_house *h, const char *prefab, int arg, mtn_room *r, meshes *m, vec3 *at) {
    mtn_house_init(h);
    h->seed = 3;
    mtn_room_type t;
    named_type(&t, "store", 0);
    if (prefab) mtn_room_add_item(&t, prefab, 0.5f, 1.5f, 0, 1, arg);
    mtn_house_add_room_type(h, &t);
    int ci = 1, cj = 1;
    east_cell(h, &ci, &cj);
    build(h, ci, cj, r, m);
    *at = v3_add(mtn_house_cell_center(h, ci, cj), v3(0.5f, 0, 1.5f));
}

static void test_prefabs(void) {
    static mtn_house h;
    meshes base, m;
    static mtn_room r0, r;
    vec3 at;
    build_single(&h, NULL, 0, &r0, &base, &at);
    CHECK(NPREFABS == (int)(sizeof EXPECT / sizeof EXPECT[0]));
    for (int i = 0; i < NPREFABS; i++) {
        const prefab_expect *e = &EXPECT[i];
        CHECK_STR(PREFABS[i], e->name);
        build_single(&h, e->name, 2, &r, &m, &at);
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
        for (int v = base.solid.vcount; v < m.solid.vcount; v++) {
            const mtn_vertex *vx = &m.solid.v[v];
            CHECK(fabsf(vx->x - r.center.x) <= r.width * 0.5f + 1e-3f && fabsf(vx->z - r.center.z) <= r.length * 0.5f + 1e-3f &&
                  vx->y <= r.height + 2.5f);
        }
        if (e->collider) {
            const mtn_aabb *b = &r.colliders[r.collider_count - 1];
            /* furniture colliders sit on the floor around the item position */
            CHECK(b->mn.y == 0 && b->mx.y > 0.3f);
            CHECK(b->mn.x < at.x && b->mx.x > at.x && b->mn.z < at.z && b->mx.z > at.z);
        }
        m_free(&m);
    }
    /* unknown prefab: nothing is added */
    build_single(&h, "spaceship", 0, &r, &m, &at);
    CHECK(m.solid.vcount == base.solid.vcount && m.glow.vcount == base.glow.vcount && m.sprites.vcount == 0);
    CHECK(r.collider_count == r0.collider_count && r.light_count == r0.light_count);
    m_free(&m);
    int nl = r0.light_count;
    /* details: the fireplace, photos, lights */
    build_single(&h, "fireplace", 0, &r, &m, &at);
    CHECK(r.has_fire);
    CHECK_NEAR(r.fire_pos.x, at.x, 1e-4);
    CHECK_NEAR(r.fire_pos.y, 0.5f, 1e-5);
    CHECK_NEAR(r.fire_pos.z, at.z, 1e-4);
    CHECK_NEAR(r.lights[nl].flicker, 1.0f, 0);
    CHECK_NEAR(r.lights[nl].radius, 7.0f, 0);
    m_free(&m);
    build_single(&h, "photo", 5, &r, &m, &at);
    CHECK(r.photo_count == 1);
    CHECK(r.photos[0].photo == 5);
    CHECK_NEAR(r.photos[0].center.x, at.x, 1e-4);
    CHECK_NEAR(r.photos[0].center.y, 1.6f, 1e-5);
    CHECK_NEAR(r.photos[0].center.z, at.z - 0.02f, 1e-4);
    CHECK_NEAR(r.photos[0].right.x, 1, 1e-5);
    CHECK_NEAR(r.photos[0].up.y, 1, 0);
    m_free(&m);
    build_single(&h, "lamp", 0, &r, &m, &at);
    CHECK_NEAR(r.lights[nl].pos.x, at.x, 1e-4);
    CHECK_NEAR(r.lights[nl].pos.y, 1.6f, 1e-5);
    CHECK_NEAR(r.lights[nl].flicker, 0, 0);
    m_free(&m);
    build_single(&h, "chandelier", 0, &r, &m, &at);
    CHECK_NEAR(r.lights[nl].pos.y, 3.0f, 1e-5);
    m_free(&m);
    /* rug size from its argument */
    vec3 mn, mx;
    build_single(&h, "rug", 0, &r, &m, &at);
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
    build_single(&h, "rug", 4, &r, &m, &at);
    mn = mx = v3(m.solid.v[nb].x, 0, m.solid.v[nb].z);
    for (int i = nb; i < m.solid.vcount; i++) {
        mn.x = mtn_minf(mn.x, m.solid.v[i].x);
        mx.x = mtn_maxf(mx.x, m.solid.v[i].x);
    }
    CHECK_NEAR(mx.x - mn.x, 4.0f, 1e-4);
    m_free(&m);
    /* plants use one of two leaf tiles depending on the seed */
    build_single(&h, "plant", 0, &r, &m, &at);
    CHECK(m.sprites.vcount == 5 * 4);
    CHECK(m.sprites.v[0].tile == 8 || m.sprites.v[0].tile == 9);
    m_free(&m);
    m_free(&base);
}

/* where the last collider of a one-item room ends up */
static vec3 item_center(mtn_room_item *it, int *placed, vec3 *center, mtn_room *r) {
    static mtn_house h;
    mtn_house_init(&h);
    h.seed = 3;
    mtn_room_type t;
    named_type(&t, "store", 0);
    t.width_min = t.width_max = 8.0f;
    t.length_min = t.length_max = 10.0f;
    mtn_room_add_item_ex(&t, it);
    mtn_house_add_room_type(&h, &t);
    int ci = 1, cj = 1;
    east_cell(&h, &ci, &cj);
    meshes m;
    static mtn_room base;
    mtn_house_init(&h);
    h.seed = 3;
    t.item_count = 0;
    mtn_house_add_room_type(&h, &t);
    build(&h, ci, cj, &base, &m);
    m_free(&m);
    h.types[0].item_count = 0;
    mtn_room_add_item_ex(&h.types[0], it);
    build(&h, ci, cj, r, &m);
    m_free(&m);
    *placed = r->collider_count > base.collider_count;
    *center = r->center;
    const mtn_aabb *b = &r->colliders[r->collider_count - 1];
    return v3_sub(v3_scale(v3_add(b->mn, b->mx), 0.5f), r->center);
}

static void test_item_placement(void) {
    mtn_room_item it;
    int placed;
    vec3 c, ctr;
    static mtn_room r;
    /* a chest (1.0 x 0.6) against each wall, facing into the room */
    const char *walls[4] = {"window", "back", "right", "left"};
    (void)walls;
    for (int w = MTN_WALL_WINDOW; w <= MTN_WALL_LEFT; w++) {
        mtn_room_item_defaults(&it);
        snprintf(it.prefab, sizeof it.prefab, "chest");
        it.wall = w;
        it.along = 0.6f;
        it.inset = 0.32f;
        c = item_center(&it, &placed, &ctr, &r);
        CHECK(placed);
        switch (w) {
        case MTN_WALL_WINDOW: CHECK_NEAR(c.x, 4.0f - 0.32f, 1e-3); CHECK_NEAR(c.z, 3.0f, 1e-3); break;
        case MTN_WALL_BACK: CHECK_NEAR(c.x, -4.0f + 0.32f, 1e-3); CHECK_NEAR(c.z, 3.0f, 1e-3); break;
        case MTN_WALL_RIGHT: CHECK_NEAR(c.z, 5.0f - 0.32f, 1e-3); CHECK_NEAR(c.x, 2.4f, 1e-3); break;
        default: CHECK_NEAR(c.z, -5.0f + 0.32f, 1e-3); CHECK_NEAR(c.x, 2.4f, 1e-3); break;
        }
        /* turned to face into the room: 0.6 m deep across the wall */
        const mtn_aabb *b = &r.colliders[r.collider_count - 1];
        float across = (w == MTN_WALL_WINDOW || w == MTN_WALL_BACK) ? b->mx.x - b->mn.x : b->mx.z - b->mn.z;
        CHECK_NEAR(across, 0.6f, 1e-3);
    }
    /* fractions of the room size, metres and scale */
    mtn_room_item_defaults(&it);
    snprintf(it.prefab, sizeof it.prefab, "chest");
    it.fx = 0.5f;
    it.fz = -0.5f;
    it.z = 0.5f;
    it.scale = 2.0f;
    it.rot = 90.0f;
    c = item_center(&it, &placed, &ctr, &r);
    CHECK(placed);
    CHECK_NEAR(c.x, 2.0f, 1e-3);
    CHECK_NEAR(c.z, -2.0f, 1e-3);
    const mtn_aabb *b = &r.colliders[r.collider_count - 1];
    CHECK_NEAR(b->mx.x - b->mn.x, 1.2f, 1e-3); /* turned: 2 x 0.3 deep */
    CHECK_NEAR(b->mx.z - b->mn.z, 2.0f, 1e-3);
    /* positions outside the room are pulled in */
    it.fx = 0;
    it.fz = 0;
    it.x = 30.0f;
    it.z = 1.0f;
    it.scale = 1.0f;
    it.rot = 0;
    c = item_center(&it, &placed, &ctr, &r);
    CHECK(placed);
    CHECK_NEAR(c.x, 4.0f - 0.15f, 1e-3);
    /* nothing is placed in front of a doorway */
    mtn_room_item_defaults(&it);
    snprintf(it.prefab, sizeof it.prefab, "chest");
    it.wall = MTN_WALL_BACK;
    it.along = 0.05f;
    c = item_center(&it, &placed, &ctr, &r);
    CHECK(!placed);
    it.wall = MTN_WALL_RIGHT;
    c = item_center(&it, &placed, &ctr, &r);
    CHECK(!placed);
    it.wall = MTN_WALL_LEFT;
    c = item_center(&it, &placed, &ctr, &r);
    CHECK(!placed);
    it.wall = MTN_WALL_WINDOW; /* a window, not a door */
    c = item_center(&it, &placed, &ctr, &r);
    CHECK(placed);
    /* invalid walls fall back to free placement, bad scales to 1 */
    mtn_room_type t;
    named_type(&t, "x", 0);
    it.wall = 9;
    it.scale = -1.0f;
    CHECK(mtn_room_add_item_ex(&t, &it) == 0);
    CHECK(t.items[0].wall == MTN_WALL_NONE && t.items[0].scale == 1.0f);
}

static void test_item_caps(void) {
    static mtn_house h;
    mtn_house_init(&h);
    h.seed = 9;
    mtn_room_type t;
    named_type(&t, "store", 0);
    t.width_min = t.width_max = 9.0f;
    t.length_min = t.length_max = 12.0f;
    for (int i = 0; i < 10; i++) mtn_room_add_item(&t, "lamp", -3.0f, -5.0f + 0.8f * (float)i, 0, 1, 0);
    for (int i = 0; i < 10; i++) mtn_room_add_item(&t, "photo", 2.0f, -5.0f + 0.8f * (float)i, -90, 1, i);
    mtn_house_add_room_type(&h, &t);
    static mtn_room r;
    meshes m;
    build(&h, 4, 4, &r, &m);
    /* caps: 8 lights (ceiling lamps, corridor lamps and room lamps), 8 photo slots */
    CHECK(r.light_count == 8);
    CHECK(r.photo_count == 8);
    CHECK(r.photos[7].photo == 7);
    CHECK(mesh_ok(&m.solid) && mesh_ok(&m.glow));
    m_free(&m);
    /* too many items */
    while (t.item_count < MTN_MAX_ROOM_ITEMS) mtn_room_add_item(&t, "rug", 0, 0, 0, 1, 0);
    CHECK(mtn_room_add_item(&t, "rug", 0, 0, 0, 1, 0) == -1);
    CHECK(mtn_room_add_item(&t, "a_prefab_name_that_is_much_too_long", 0, 0, 0, 1, 0) == -1);
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

MTN_SUITE(room) {
    TEST_RUN(test_closed_sides);
    TEST_RUN(test_types);
    TEST_RUN(test_plan);
    TEST_RUN(test_shell_and_doors);
    TEST_RUN(test_cabin);
    TEST_RUN(test_balcony);
    TEST_RUN(test_prefabs);
    TEST_RUN(test_item_placement);
    TEST_RUN(test_item_caps);
    TEST_RUN(test_collide_box);
}
