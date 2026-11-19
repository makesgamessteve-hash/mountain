#include "test.h"
#include "mtn/mesh.h"

static vec3 vpos(const mtn_vertex *v) { return v3(v->x, v->y, v->z); }
static vec3 vnrm(const mtn_vertex *v) { return v3(v->nx, v->ny, v->nz); }

/* every triangle's winding must agree with the stored (flat) normal */
static int windings_match_normals(const mtn_mesh *m) {
    for (int i = 0; i + 2 < m->icount; i += 3) {
        const mtn_vertex *a = &m->v[m->idx[i]], *b = &m->v[m->idx[i + 1]], *c = &m->v[m->idx[i + 2]];
        vec3 cr = v3_cross(v3_sub(vpos(b), vpos(a)), v3_sub(vpos(c), vpos(a)));
        if (v3_len(cr) < 1e-5f) continue; /* degenerate triangle */
        if (v3_dot(cr, vnrm(a)) <= 0.0f) return 0;
    }
    return 1;
}

static int indices_valid(const mtn_mesh *m) {
    if (m->icount % 3) return 0;
    for (int i = 0; i < m->icount; i++)
        if ((int)m->idx[i] >= m->vcount) return 0;
    return 1;
}

static void test_buffers(void) {
    mtn_mesh m;
    mtn_mesh_init(&m);
    CHECK(m.v == NULL && m.idx == NULL && m.vcount == 0 && m.icount == 0);
    mtn_vertex v;
    memset(&v, 0, sizeof v);
    for (int i = 0; i < 600; i++) {
        v.x = (float)i;
        CHECK(mtn_mesh_vertex(&m, &v) == i);
    }
    CHECK(m.vcount == 600);
    CHECK(m.vcap >= 600);
    CHECK_NEAR(m.v[599].x, 599.0f, 0);
    CHECK_NEAR(m.v[0].x, 0.0f, 0);
    for (int i = 0; i < 300; i++) mtn_mesh_tri(&m, i, i + 1, i + 2);
    CHECK(m.icount == 900);
    CHECK(m.icap >= 900);
    CHECK(m.idx[897] == 299 && m.idx[899] == 301);
    vec3 mn, mx;
    mtn_mesh_bounds(&m, &mn, &mx);
    CHECK_NEAR(mn.x, 0.0f, 0);
    CHECK_NEAR(mx.x, 599.0f, 0);
    int cap = m.vcap;
    mtn_mesh_clear(&m);
    CHECK(m.vcount == 0 && m.icount == 0);
    CHECK(m.vcap == cap && m.v != NULL);
    mtn_mesh_bounds(&m, &mn, &mx); /* empty: inverted bounds */
    CHECK(mn.x > mx.x);
    mtn_mesh_free(&m);
    CHECK(m.v == NULL && m.idx == NULL && m.vcap == 0 && m.icap == 0);
}

static void test_colors(void) {
    uint32_t c = mtn_rgb(300, -5, 128);
    CHECK((c & 255) == 255);
    CHECK(((c >> 8) & 255) == 0);
    CHECK(((c >> 16) & 255) == 128);
    CHECK((c >> 24) == 255);
    uint32_t s = mtn_rgb_scale(0x80FF8040u, 0.5f);
    CHECK((s & 255) == 0x20);
    CHECK(((s >> 8) & 255) == 0x40);
    CHECK(((s >> 16) & 255) == 0x7F);
    CHECK((s >> 24) == 0x80); /* alpha preserved */
    uint32_t b = mtn_rgb_scale(mtn_rgb(200, 200, 200), 2.0f);
    CHECK((b & 0xFFFFFF) == 0xFFFFFF); /* saturates */
    mtn_material m = mtn_mat(0x11223344u, 5, MTN_VF_WIND | MTN_VF_SNOW, 0.25f);
    CHECK(m.r == 0x44 && m.g == 0x33 && m.b == 0x22 && m.a == 0x11);
    CHECK(m.tile == 5);
    CHECK(m.flags == (MTN_VF_WIND | MTN_VF_SNOW));
    CHECK(m.extra == 0);
    CHECK_NEAR(m.uv_scale, 0.25f, 0);
}

static void test_quad(void) {
    mtn_mesh m;
    mtn_mesh_init(&m);
    mtn_material mat = mtn_mat(mtn_rgb(10, 20, 30), 3, MTN_VF_EMISSIVE, 1.0f);
    mtn_mesh_quad(&m, m4_translate(v3(0, 0, 5)), v3(0, 0, 0), v3(2, 0, 0), v3(2, 1, 0), v3(0, 1, 0), &mat, v2(0, 0),
                  v2(2, 1));
    CHECK(m.vcount == 4);
    CHECK(m.icount == 6);
    CHECK(indices_valid(&m));
    for (int i = 0; i < 4; i++) {
        CHECK_NEAR(m.v[i].z, 5.0f, 1e-6);
        CHECK_NEAR(m.v[i].nz, 1.0f, 1e-6);
        CHECK(m.v[i].r == 10 && m.v[i].g == 20 && m.v[i].b == 30 && m.v[i].a == 255);
        CHECK(m.v[i].tile == 3 && m.v[i].flags == MTN_VF_EMISSIVE && m.v[i].ao == 255);
    }
    /* uv: bottom left gets (u0, v1) (texture v grows downwards) */
    CHECK_NEAR(m.v[0].u, 0.0f, 0);
    CHECK_NEAR(m.v[0].v, 1.0f, 0);
    CHECK_NEAR(m.v[2].u, 2.0f, 0);
    CHECK_NEAR(m.v[2].v, 0.0f, 0);
    CHECK(windings_match_normals(&m));

    /* a quad with two coincident corners (a triangle) still gets a proper normal */
    mtn_mesh_clear(&m);
    mtn_mesh_quad(&m, m4_identity(), v3(0, 0, 0), v3(0, 0, 0), v3(1, 0, 0), v3(0, 1, 0), &mat, v2(0, 0), v2(1, 1));
    CHECK_NEAR(m.v[0].nz, 1.0f, 1e-6);
    CHECK_NEAR(v3_len(vnrm(&m.v[0])), 1.0f, 1e-5);
    mtn_mesh_free(&m);
}

static void check_box_outward(const mtn_mesh *m, vec3 centre) {
    for (int q = 0; q + 3 < m->vcount; q += 4) {
        vec3 fc = v3(0, 0, 0);
        for (int k = 0; k < 4; k++) fc = v3_add(fc, v3_scale(vpos(&m->v[q + k]), 0.25f));
        vec3 n = vnrm(&m->v[q]);
        CHECK_NEAR(v3_len(n), 1.0f, 1e-5);
        CHECK(v3_dot(n, v3_sub(fc, centre)) > 0.0f);
        for (int k = 1; k < 4; k++) CHECK_NEAR(v3_dot(n, vnrm(&m->v[q + k])), 1.0f, 1e-5);
    }
}

static void test_box(void) {
    mtn_mesh m;
    mtn_mesh_init(&m);
    mtn_material mat = mtn_mat(mtn_rgb(200, 200, 200), 1, 0, 0.5f);
    mtn_mesh_box(&m, m4_identity(), v3(-1, 0, -2), v3(1, 3, 2), &mat, 63);
    CHECK(m.vcount == 24);
    CHECK(m.icount == 36);
    CHECK(indices_valid(&m));
    CHECK(windings_match_normals(&m));
    check_box_outward(&m, v3(0, 1.5f, 0));
    /* one face per axis direction */
    int dirs[6] = {0};
    for (int q = 0; q < 24; q += 4) {
        vec3 n = vnrm(&m.v[q]);
        if (n.x < -0.5f) dirs[0]++;
        if (n.x > 0.5f) dirs[1]++;
        if (n.y < -0.5f) dirs[2]++;
        if (n.y > 0.5f) dirs[3]++;
        if (n.z < -0.5f) dirs[4]++;
        if (n.z > 0.5f) dirs[5]++;
    }
    for (int i = 0; i < 6; i++) CHECK(dirs[i] == 1);
    vec3 mn, mx;
    mtn_mesh_bounds(&m, &mn, &mx);
    CHECK_NEAR(mn.x, -1, 0);
    CHECK_NEAR(mn.y, 0, 0);
    CHECK_NEAR(mn.z, -2, 0);
    CHECK_NEAR(mx.x, 1, 0);
    CHECK_NEAR(mx.y, 3, 0);
    CHECK_NEAR(mx.z, 2, 0);
    /* uv scale: top face spans dx * s by dz * s */
    float umax = 0, vmax = 0;
    for (int i = 12; i < 16; i++) {
        umax = mtn_maxf(umax, m.v[i].u);
        vmax = mtn_maxf(vmax, m.v[i].v);
    }
    CHECK_NEAR(umax, 1.0f, 1e-6);
    CHECK_NEAR(vmax, 2.0f, 1e-6);

    /* face subsets */
    mtn_mesh_clear(&m);
    mtn_mesh_box(&m, m4_identity(), v3(0, 0, 0), v3(1, 1, 1), &mat, 8);
    CHECK(m.vcount == 4);
    CHECK_NEAR(m.v[0].ny, 1.0f, 1e-6);
    mtn_mesh_clear(&m);
    mtn_mesh_box(&m, m4_identity(), v3(0, 0, 0), v3(1, 1, 1), &mat, 1 | 2 | 16 | 32);
    CHECK(m.vcount == 16);
    for (int i = 0; i < m.vcount; i++) CHECK_NEAR(m.v[i].ny, 0.0f, 1e-6);
    mtn_mesh_clear(&m);
    mtn_mesh_box(&m, m4_identity(), v3(0, 0, 0), v3(1, 1, 1), &mat, 0);
    CHECK(m.vcount == 0);

    /* transformed box stays closed and outward facing */
    mtn_mesh_clear(&m);
    mat4 xf = m4_mul(m4_translate(v3(10, 0, 0)), m4_rotate_y(0.7f));
    mtn_mesh_box(&m, xf, v3(-1, -1, -1), v3(1, 1, 1), &mat, 63);
    check_box_outward(&m, v3(10, 0, 0));
    CHECK(windings_match_normals(&m));
    mtn_mesh_free(&m);
}

static void test_cylinder(void) {
    mtn_mesh m;
    mtn_mesh_init(&m);
    mtn_material mat = mtn_mat(mtn_rgb(90, 60, 30), 6, 0, 1.0f);

    /* fewer than 3 sides is clamped */
    mtn_mesh_cylinder(&m, m4_identity(), v3(0, 0, 0), v3(0, 2, 0), 0.5f, 0.5f, 2, &mat, 0);
    CHECK(m.vcount == 8);
    CHECK(m.icount == 18);
    mtn_mesh_clear(&m);

    /* vertical capped cylinder */
    const int sides = 6;
    mtn_mesh_cylinder(&m, m4_identity(), v3(0, 0, 0), v3(0, 2, 0), 0.5f, 0.5f, sides, &mat, 1);
    CHECK(m.vcount == (sides + 1) * 2 + 2 * (1 + sides + 1));
    CHECK(m.icount == sides * 6 + 2 * sides * 3);
    CHECK(indices_valid(&m));
    CHECK(windings_match_normals(&m));
    for (int i = 0; i < (sides + 1) * 2; i++) {
        vec3 p = vpos(&m.v[i]), n = vnrm(&m.v[i]);
        CHECK_NEAR(sqrtf(p.x * p.x + p.z * p.z), 0.5f, 1e-5);
        CHECK_NEAR(n.y, 0.0f, 1e-5);
        CHECK(v3_dot(n, v3(p.x, 0, p.z)) > 0.0f); /* outward */
    }
    int base = (sides + 1) * 2;
    CHECK_NEAR(m.v[base].ny, -1.0f, 1e-5);            /* bottom cap centre */
    CHECK_NEAR(m.v[base].y, 0.0f, 1e-6);
    CHECK_NEAR(m.v[base + sides + 2].ny, 1.0f, 1e-5); /* top cap centre */
    CHECK_NEAR(m.v[base + sides + 2].y, 2.0f, 1e-6);
    vec3 mn, mx;
    mtn_mesh_bounds(&m, &mn, &mx);
    CHECK_NEAR(mx.y, 2.0f, 1e-6);
    CHECK_NEAR(mx.z, 0.5f, 1e-5);
    CHECK(mx.x > 0.4f && mx.x <= 0.5f + 1e-6f);
    mtn_mesh_clear(&m);

    /* horizontal cone: normals lean towards the narrow end */
    mtn_mesh_cylinder(&m, m4_identity(), v3(0, 1, 0), v3(4, 1, 0), 1.0f, 0.2f, 8, &mat, 0);
    CHECK(windings_match_normals(&m));
    for (int i = 0; i < m.vcount; i++) {
        CHECK(m.v[i].nx > 0.1f);
        CHECK_NEAR(v3_len(vnrm(&m.v[i])), 1.0f, 1e-5);
    }
    CHECK_NEAR(m.v[0].v, 4.0f, 1e-5); /* v runs along the length */
    CHECK_NEAR(m.v[1].v, 0.0f, 0);
    mtn_mesh_clear(&m);

    /* zero length: no NaNs */
    mtn_mesh_cylinder(&m, m4_identity(), v3(1, 1, 1), v3(1, 1, 1), 0.3f, 0.3f, 4, &mat, 1);
    CHECK(m.vcount > 0);
    for (int i = 0; i < m.vcount; i++) CHECK(m.v[i].x == m.v[i].x && m.v[i].nx == m.v[i].nx);
    mtn_mesh_free(&m);
}

static void test_billboard(void) {
    mtn_mesh m;
    mtn_mesh_init(&m);
    mtn_material mat = mtn_mat(mtn_rgb(255, 255, 255), 9, MTN_VF_WIND, 1.0f);
    mtn_mesh_billboard(&m, v3(1, 2, 3), 2.0f, 4.0f, 1.0f, &mat, 0.5f);
    CHECK(m.vcount == 4);
    CHECK(m.icount == 6);
    float miny = 1e9f, maxy = -1e9f;
    for (int i = 0; i < 4; i++) {
        const mtn_vertex *v = &m.v[i];
        CHECK_NEAR(v->x, 1, 0);
        CHECK_NEAR(v->y, 2, 0);
        CHECK_NEAR(v->z, 3, 0);
        CHECK(v->flags == (MTN_VF_WIND | MTN_VF_BILLBOARD | MTN_VF_ABS_UV));
        CHECK_NEAR(fabsf(v->nx), 1.0f, 0);
        miny = mtn_minf(miny, v->ny);
        maxy = mtn_maxf(maxy, v->ny);
        /* only the upper corners sway */
        CHECK_NEAR(v->nz, v->ny > 0.0f ? 0.5f : 0.0f, 0);
    }
    CHECK_NEAR(miny, 0.0f, 0); /* standing on its bottom edge */
    CHECK_NEAR(maxy, 4.0f, 0);
    vec3 mn, mx;
    mtn_mesh_bounds(&m, &mn, &mx); /* corner offsets pad the bounds */
    CHECK(mn.x <= 0.0f && mx.x >= 2.0f);
    CHECK(mx.y >= 6.0f);
    mtn_mesh_clear(&m);
    mtn_mesh_billboard(&m, v3(0, 0, 0), 2.0f, 4.0f, 0.0f, &mat, 0.0f);
    CHECK_NEAR(m.v[0].ny, -2.0f, 0); /* centred */
    CHECK_NEAR(m.v[2].ny, 2.0f, 0);
    mtn_mesh_free(&m);
}

static void test_rock(void) {
    mtn_mesh a, b;
    mtn_mesh_init(&a);
    mtn_mesh_init(&b);
    mtn_material mat = mtn_mat(mtn_rgb(128, 128, 128), 2, MTN_VF_SNOW, 1.0f);
    mtn_mesh_rock(&a, m4_identity(), 2.0f, 17, &mat);
    CHECK(a.vcount == 5 * 9 * 4);
    CHECK(a.icount == 5 * 9 * 6);
    CHECK(indices_valid(&a));
    for (int i = 0; i < a.vcount; i++) {
        vec3 p = vpos(&a.v[i]);
        CHECK(v3_len(p) <= 2.0f * 1.17f + 1e-4f);
        CHECK(fabsf(p.y) <= 2.0f * 1.17f * 0.72f + 1e-4f); /* flattened */
        CHECK_NEAR(v3_len(vnrm(&a.v[i])), 1.0f, 1e-4);   /* every facet has a usable normal */
    }
    /* the first ring of facets meets at the top pole: they face up and can hold snow */
    for (int q = 0; q < 9; q++) CHECK(a.v[q * 4].ny > 0.5f);
    for (int q = 36; q < 45; q++) CHECK(a.v[q * 4].ny < -0.3f); /* bottom ring faces down */
    /* facets point away from the centre */
    int outward = 0;
    for (int q = 0; q < a.vcount; q += 4) {
        vec3 fc = v3(0, 0, 0);
        for (int k = 0; k < 4; k++) fc = v3_add(fc, vpos(&a.v[q + k]));
        outward += v3_dot(vnrm(&a.v[q]), fc) > 0.0f;
    }
    CHECK(outward == 45);
    CHECK(windings_match_normals(&a));
    /* deterministic per seed */
    mtn_mesh_rock(&b, m4_identity(), 2.0f, 17, &mat);
    CHECK(memcmp(a.v, b.v, sizeof(mtn_vertex) * (size_t)a.vcount) == 0);
    mtn_mesh_clear(&b);
    mtn_mesh_rock(&b, m4_identity(), 2.0f, 18, &mat);
    CHECK(memcmp(a.v, b.v, sizeof(mtn_vertex) * (size_t)a.vcount) != 0);
    mtn_mesh_free(&a);
    mtn_mesh_free(&b);
}

static void test_append(void) {
    mtn_mesh src, dst;
    mtn_mesh_init(&src);
    mtn_mesh_init(&dst);
    mtn_material mat = mtn_mat(mtn_rgb(255, 0, 0), 0, 0, 1.0f);
    mtn_mesh_box(&src, m4_identity(), v3(0, 0, 0), v3(1, 1, 1), &mat, 2); /* +x face */
    mtn_mesh_billboard(&src, v3(0, 0, 0), 1.0f, 1.0f, 0.0f, &mat, 0.3f);
    mtn_mesh_quad(&dst, m4_identity(), v3(0, 0, 0), v3(1, 0, 0), v3(1, 1, 0), v3(0, 1, 0), &mat, v2(0, 0), v2(1, 1));
    mat4 xf = m4_mul(m4_translate(v3(5, 0, 0)), m4_rotate_y(MTN_PI * 0.5f));
    mtn_mesh_append(&dst, &src, xf);
    CHECK(dst.vcount == 4 + 8);
    CHECK(dst.icount == 6 + 12);
    CHECK(indices_valid(&dst));
    for (int i = 6; i < 18; i++) CHECK(dst.idx[i] >= 4);
    /* box face normal rotated from +x to -z, positions moved */
    CHECK_NEAR(dst.v[4].nz, -1.0f, 1e-5);
    CHECK_NEAR(dst.v[4].nx, 0.0f, 1e-5);
    CHECK_NEAR(dst.v[4].x, 5.0f + src.v[0].z, 1e-5);
    /* billboard corner offsets are left alone */
    for (int i = 8; i < 12; i++) {
        CHECK_NEAR(dst.v[i].nx, src.v[i - 4].nx, 0);
        CHECK_NEAR(dst.v[i].ny, src.v[i - 4].ny, 0);
        CHECK_NEAR(dst.v[i].x, 5.0f, 1e-5);
    }
    CHECK(windings_match_normals(&dst));
    mtn_mesh_free(&src);
    mtn_mesh_free(&dst);
}

static void test_voxels_basic(void) {
    mtn_voxels vx;
    mtn_voxels_init(&vx, 4, 5, 6, 0.5f);
    CHECK(vx.sx == 4 && vx.sy == 5 && vx.sz == 6);
    CHECK(mtn_voxels_count(&vx) == 0);
    CHECK(vx.pal[7] == mtn_rgb(7, 7, 7));
    mtn_voxels_set(&vx, 3, 4, 5, 9);
    CHECK(mtn_voxels_get(&vx, 3, 4, 5) == 9);
    CHECK(mtn_voxels_count(&vx) == 1);
    /* out of range writes are ignored, reads are empty */
    mtn_voxels_set(&vx, -1, 0, 0, 1);
    mtn_voxels_set(&vx, 0, -1, 0, 1);
    mtn_voxels_set(&vx, 0, 0, -1, 1);
    mtn_voxels_set(&vx, 4, 0, 0, 1);
    mtn_voxels_set(&vx, 0, 5, 0, 1);
    mtn_voxels_set(&vx, 0, 0, 6, 1);
    CHECK(mtn_voxels_count(&vx) == 1);
    CHECK(mtn_voxels_get(&vx, -1, 0, 0) == 0);
    CHECK(mtn_voxels_get(&vx, 0, 0, 6) == 0);
    CHECK(mtn_voxels_get(&vx, 4, 4, 5) == 0);
    /* box is inclusive and clipped */
    mtn_voxels_box(&vx, 0, 0, 0, 1, 2, 3, 4);
    CHECK(mtn_voxels_count(&vx) == 1 + 2 * 3 * 4);
    mtn_voxels_box(&vx, -5, -5, -5, 10, 10, 10, 2);
    CHECK(mtn_voxels_count(&vx) == 4 * 5 * 6);
    CHECK(mtn_voxels_get(&vx, 3, 4, 5) == 2);
    mtn_voxels_free(&vx);
    CHECK(vx.v == NULL);

    /* blobs are roughly spherical and deterministic */
    mtn_voxels a, b;
    mtn_voxels_init(&a, 16, 16, 16, 1.0f);
    mtn_voxels_init(&b, 16, 16, 16, 1.0f);
    mtn_voxels_blob(&a, 8, 8, 8, 4.0f, 3, 5);
    mtn_voxels_blob(&b, 8, 8, 8, 4.0f, 3, 5);
    int n = mtn_voxels_count(&a);
    CHECK(n == mtn_voxels_count(&b));
    CHECK(memcmp(a.v, b.v, 16 * 16 * 16) == 0);
    CHECK(n > 100 && n < 600); /* volume of r=4 sphere ~ 268 */
    CHECK(mtn_voxels_get(&a, 8, 8, 8) == 3);
    CHECK(mtn_voxels_get(&a, 0, 0, 0) == 0);
    CHECK(mtn_voxels_get(&a, 8, 8, 15) == 0);
    /* near the edge: clipped without crashing */
    mtn_voxels_blob(&b, 0.5f, 0.5f, 0.5f, 3.0f, 1, 9);
    CHECK(mtn_voxels_get(&b, 0, 0, 0) == 1);
    mtn_voxels_free(&a);
    mtn_voxels_free(&b);
}

static void test_voxels_mesh(void) {
    mtn_voxels vx;
    mtn_mesh m;
    mtn_mesh_init(&m);
    mtn_voxels_init(&vx, 3, 3, 3, 0.5f);
    vx.pal[1] = mtn_rgb(100, 150, 200);

    /* a lone voxel: 6 faces, outward normals, no occlusion */
    mtn_voxels_set(&vx, 1, 1, 1, 1);
    mtn_voxels_mesh(&vx, &m, m4_identity(), 4, MTN_VF_SNOW);
    CHECK(m.vcount == 24);
    CHECK(m.icount == 36);
    CHECK(windings_match_normals(&m));
    check_box_outward(&m, v3(0.75f, 0.75f, 0.75f));
    for (int i = 0; i < m.vcount; i++) {
        CHECK(m.v[i].ao == 255);
        CHECK(m.v[i].tile == 4 && m.v[i].flags == MTN_VF_SNOW);
        CHECK(m.v[i].x >= 0.5f - 1e-6f && m.v[i].x <= 1.0f + 1e-6f);
        /* colour jitter of +-12% around the palette colour */
        CHECK(m.v[i].r >= 87 && m.v[i].r <= 112);
        CHECK(m.v[i].b >= 175 && m.v[i].b <= 224);
    }

    /* faces between solid neighbours are culled */
    mtn_mesh_clear(&m);
    mtn_voxels_set(&vx, 2, 1, 1, 1);
    mtn_voxels_mesh(&vx, &m, m4_identity(), 0, 0);
    CHECK(m.vcount == 10 * 4);
    CHECK(m.icount == 10 * 6);
    for (int q = 0; q < m.vcount; q += 4) {
        /* no face lies on the shared plane x = 1.0 */
        int on_plane = 1;
        for (int k = 0; k < 4; k++) on_plane &= fabsf(m.v[q + k].x - 1.0f) < 1e-6f;
        CHECK(!on_plane);
    }

    /* a full block only shows its shell */
    mtn_mesh_clear(&m);
    mtn_voxels_box(&vx, 0, 0, 0, 2, 2, 2, 1);
    mtn_voxels_mesh(&vx, &m, m4_identity(), 0, 0);
    CHECK(m.vcount == 6 * 9 * 4);
    check_box_outward(&m, v3(0.75f, 0.75f, 0.75f));
    mtn_voxels_free(&vx);

    /* ambient occlusion: neighbours next to a face darken its corners */
    mtn_voxels_init(&vx, 3, 3, 3, 1.0f);
    mtn_voxels_set(&vx, 1, 1, 1, 1);
    mtn_voxels_set(&vx, 2, 2, 1, 1); /* above-right: touches the +x edge of the top face */
    mtn_voxels_set(&vx, 1, 2, 2, 1); /* above-front: touches the +z edge */
    mtn_mesh_clear(&m);
    mtn_voxels_mesh(&vx, &m, m4_identity(), 0, 0);
    int found_top = 0;
    for (int q = 0; q < m.vcount; q += 4) {
        vec3 fc = v3(0, 0, 0);
        for (int k = 0; k < 4; k++) fc = v3_add(fc, v3_scale(vpos(&m.v[q + k]), 0.25f));
        /* the top face of the centre voxel */
        if (m.v[q].ny < 0.9f || v3_dist(fc, v3(1.5f, 2.0f, 1.5f)) > 1e-5f) continue;
        found_top = 1;
        for (int k = 0; k < 4; k++) {
            const mtn_vertex *v = &m.v[q + k];
            int px = v->x > 1.5f, pz = v->z > 1.5f;
            int expect = (px && pz) ? 3 : px + pz;
            CHECK(v->ao == (uint8_t)(255 - expect * 50));
        }
    }
    CHECK(found_top);
    int darkest = 255;
    for (int i = 0; i < m.vcount; i++) darkest = m.v[i].ao < darkest ? m.v[i].ao : darkest;
    CHECK(darkest == 105);

    /* transform moves and rotates the voxel mesh */
    mtn_mesh_clear(&m);
    mtn_voxels_box(&vx, 0, 0, 0, 2, 2, 2, 0);
    mtn_voxels_set(&vx, 0, 0, 0, 1);
    mtn_voxels_mesh(&vx, &m, m4_mul(m4_translate(v3(10, 0, 0)), m4_rotate_y(MTN_PI)), 0, 0);
    CHECK(m.vcount == 24);
    check_box_outward(&m, v3(9.5f, 0.5f, -0.5f));
    mtn_voxels_free(&vx);
    mtn_mesh_free(&m);
}

MTN_SUITE(mesh) {
    TEST_RUN(test_buffers);
    TEST_RUN(test_colors);
    TEST_RUN(test_quad);
    TEST_RUN(test_box);
    TEST_RUN(test_cylinder);
    TEST_RUN(test_billboard);
    TEST_RUN(test_rock);
    TEST_RUN(test_append);
    TEST_RUN(test_voxels_basic);
    TEST_RUN(test_voxels_mesh);
}
