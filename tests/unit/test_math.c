#include "test.h"
#include "mtn/math.h"

static int mat_near(mat4 a, mat4 b, float eps) {
    for (int i = 0; i < 16; i++)
        if (fabsf(a.m[i] - b.m[i]) > eps) return 0;
    return 1;
}

static void test_scalars(void) {
    CHECK_NEAR(mtn_clampf(5, 0, 1), 1, 0);
    CHECK_NEAR(mtn_clampf(-5, 0, 1), 0, 0);
    CHECK_NEAR(mtn_clampf(0.5f, 0, 1), 0.5f, 0);
    CHECK_NEAR(mtn_lerpf(2, 4, 0.5f), 3, 1e-6);
    CHECK_NEAR(mtn_smoothstep(0, 1, 0.5f), 0.5f, 1e-6);
    CHECK_NEAR(mtn_smoothstep(0, 1, -1), 0, 1e-6);
    CHECK_NEAR(mtn_fract(1.25f), 0.25f, 1e-6);
    CHECK_NEAR(mtn_fract(-0.25f), 0.75f, 1e-6);
    CHECK_NEAR(mtn_wrap_angle(3 * MTN_PI), -MTN_PI, 1e-5);
    CHECK_NEAR(mtn_wrap_angle(-3 * MTN_PI - 0.1f), MTN_PI - 0.1f, 1e-4);
    CHECK_NEAR(mtn_minf(1, 2), 1, 0);
    CHECK_NEAR(mtn_maxf(1, 2), 2, 0);
    CHECK(mtn_clampi(7, 0, 3) == 3);
    CHECK(mtn_clampi(-7, 0, 3) == 0);
    CHECK(mtn_clampi(2, 0, 3) == 2);
}

static void test_vectors(void) {
    vec3 a = v3(1, 2, 3), b = v3(4, 5, 6);
    CHECK_NEAR(v3_dot(a, b), 32, 1e-6);
    vec3 c = v3_cross(v3(1, 0, 0), v3(0, 1, 0));
    CHECK_NEAR(c.z, 1, 1e-6);
    CHECK_NEAR(v3_len(v3(3, 4, 0)), 5, 1e-6);
    vec3 n = v3_norm(v3(0, 0, 9));
    CHECK_NEAR(n.z, 1, 1e-6);
    vec3 z = v3_norm(v3(0, 0, 0));
    CHECK_NEAR(z.x + z.y + z.z, 0, 0);
    vec3 s = v3_sub(b, a);
    CHECK_NEAR(s.x + s.y + s.z, 9, 1e-6);
    vec3 m = v3_mul(a, b);
    CHECK_NEAR(m.z, 18, 1e-6);
    vec3 l = v3_lerp(a, b, 0.5f);
    CHECK_NEAR(l.y, 3.5f, 1e-6);
    CHECK_NEAR(v3_dist(a, b), sqrtf(27), 1e-5);
    CHECK_NEAR(v3_dist_xz(v3(0, 9, 0), v3(3, -9, 4)), 5, 1e-6);
    vec3 r = v3_rotate_y(v3(0, 0, -1), MTN_PI / 2);
    CHECK_NEAR(r.x, -1, 1e-5);
    vec2 q = v2(1, 2);
    vec4 w = v4(1, 2, 3, 4);
    CHECK_NEAR(q.y + w.w, 6, 0);
    vec3 sum = v3_add(a, v3_scale(b, 2));
    CHECK_NEAR(sum.x, 9, 1e-6);
}

static void test_matrices(void) {
    mat4 t = m4_translate(v3(1, 2, 3));
    vec3 p = m4_mul_point(t, v3(1, 1, 1));
    CHECK_NEAR(p.x, 2, 1e-6);
    CHECK_NEAR(p.z, 4, 1e-6);
    vec3 d = m4_mul_dir(t, v3(1, 1, 1));
    CHECK_NEAR(d.x, 1, 1e-6);
    mat4 s = m4_scale(v3(2, 3, 4));
    mat4 inv = m4_inverse(m4_mul(t, s));
    CHECK(mat_near(m4_mul(inv, m4_mul(t, s)), m4_identity(), 1e-5f));
    mat4 sing;
    memset(&sing, 0, sizeof sing);
    CHECK(mat_near(m4_inverse(sing), m4_identity(), 0));
    mat4 rx = m4_rotate_x(MTN_PI / 2);
    vec3 py = m4_mul_point(rx, v3(0, 1, 0));
    CHECK_NEAR(py.z, 1, 1e-5);
    mat4 rz = m4_rotate_z(MTN_PI / 2);
    vec3 px = m4_mul_point(rz, v3(1, 0, 0));
    CHECK_NEAR(px.y, 1, 1e-5);
    mat4 ry = m4_rotate_y(MTN_PI / 2);
    vec3 pz = m4_mul_point(ry, v3(1, 0, 0));
    CHECK_NEAR(pz.z, -1, 1e-5);
    mat4 tt = m4_transpose(m4_transpose(rx));
    CHECK(mat_near(tt, rx, 0));
    mat4 la = m4_look_at(v3(0, 0, 5), v3(0, 0, 0), v3(0, 1, 0));
    vec3 o = m4_mul_point(la, v3(0, 0, 0));
    CHECK_NEAR(o.z, -5, 1e-5);
    mat4 pr = m4_perspective(MTN_PI / 2, 1.0f, 1.0f, 100.0f);
    vec3 nearp = m4_mul_point(pr, v3(0, 0, -1));
    CHECK_NEAR(nearp.z, -1, 1e-4);
    vec3 farp = m4_mul_point(pr, v3(0, 0, -100));
    CHECK_NEAR(farp.z, 1, 1e-4);
    mat4 orth = m4_ortho(0, 10, 0, 10, -1, 1);
    vec3 op = m4_mul_point(orth, v3(10, 10, 0));
    CHECK_NEAR(op.x, 1, 1e-5);
    CHECK_NEAR(op.y, 1, 1e-5);
    mat4 b = m4_from_basis(v3(1, 2, 3), v3(1, 0, 0), v3(0, 1, 0), v3(0, 0, 1));
    CHECK(mat_near(b, t, 0));
    CHECK(mtn_pack_rgba(1, 0, 0, 1) == 0xFF0000FFu);
    CHECK(mtn_pack_rgba(2, -1, 0, 0) == 0x000000FFu);
}

static void test_oblique(void) {
    mat4 pr = m4_perspective(1.0f, 1.5f, 0.1f, 100.0f);
    /* plane z = -5 facing away from camera: -z - 5 >= 0 visible */
    vec4 plane = v4(0, 0, -1, -5);
    mat4 ob = m4_oblique_near(pr, plane);
    vec4 on = m4_mul_v4(ob, v4(0, 0, -5, 1));
    CHECK_NEAR(on.z / on.w, -1, 1e-3);
    vec4 behind = m4_mul_v4(ob, v4(0, 0, -50, 1));
    CHECK(behind.z / behind.w > -1 && behind.z / behind.w <= 1.0001f);
    vec4 front = m4_mul_v4(ob, v4(0, 0, -2, 1));
    CHECK(front.z / front.w < -1);
    mat4 same = m4_oblique_near(pr, v4(0, 0, 0, 0));
    CHECK(mat_near(same, pr, 0));
}

static void test_frustum(void) {
    mat4 vp = m4_mul(m4_perspective(1.2f, 1.0f, 0.1f, 100.0f), m4_look_at(v3(0, 0, 0), v3(0, 0, -1), v3(0, 1, 0)));
    mtn_frustum f = mtn_frustum_from(vp);
    CHECK(mtn_frustum_sphere(&f, v3(0, 0, -10), 1));
    CHECK(!mtn_frustum_sphere(&f, v3(0, 0, 10), 1));
    CHECK(!mtn_frustum_sphere(&f, v3(0, 0, -500), 1));
    CHECK(mtn_frustum_aabb(&f, v3(-1, -1, -11), v3(1, 1, -9)));
    CHECK(!mtn_frustum_aabb(&f, v3(-1, -1, 9), v3(1, 1, 11)));
    mat4 zero;
    memset(&zero, 0, sizeof zero);
    mtn_frustum fz = mtn_frustum_from(zero);
    CHECK(mtn_frustum_sphere(&fz, v3(0, 0, 0), 1));
}

static void test_portal(void) {
    /* exterior door at origin facing +z (into the house), interior door at (0,0,100) facing -z */
    mat4 a = mtn_portal_frame(v3(0, 0, 0), v3(0, 0, 1));
    mat4 b = mtn_portal_frame(v3(10, 0, 100), v3(0, 0, -1));
    mat4 m = mtn_portal_transform(a, b);
    vec3 inside = m4_mul_point(m, v3(0, 1, 2)); /* 2 m behind door A */
    CHECK_NEAR(inside.x, 10, 1e-4);
    CHECK_NEAR(inside.y, 1, 1e-4);
    CHECK_NEAR(inside.z, 102, 1e-4); /* 2 m into the interior (+z) */
    vec3 dir = m4_mul_dir(m, v3(0, 0, 1));
    CHECK_NEAR(dir.z, 1, 1e-4);
}

MTN_SUITE(math) {
    TEST_RUN(test_scalars);
    TEST_RUN(test_vectors);
    TEST_RUN(test_matrices);
    TEST_RUN(test_oblique);
    TEST_RUN(test_frustum);
    TEST_RUN(test_portal);
}
