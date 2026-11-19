/* Mountain engine - vector/matrix math. MIT License. */
#include "mtn/math.h"

#include <math.h>
#include <string.h>

float mtn_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
float mtn_lerpf(float a, float b, float t) { return a + (b - a) * t; }
float mtn_smoothstep(float e0, float e1, float x) {
    float t = mtn_clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
float mtn_fract(float x) { return x - floorf(x); }
float mtn_wrap_angle(float a) {
    a = fmodf(a + MTN_PI, MTN_TAU);
    if (a < 0.0f) a += MTN_TAU;
    return a - MTN_PI;
}
float mtn_minf(float a, float b) { return a < b ? a : b; }
float mtn_maxf(float a, float b) { return a > b ? a : b; }
int mtn_clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

vec2 v2(float x, float y) { vec2 r = {x, y}; return r; }
vec3 v3(float x, float y, float z) { vec3 r = {x, y, z}; return r; }
vec4 v4(float x, float y, float z, float w) { vec4 r = {x, y, z, w}; return r; }
vec3 v3_add(vec3 a, vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
vec3 v3_sub(vec3 a, vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
vec3 v3_scale(vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
vec3 v3_mul(vec3 a, vec3 b) { return v3(a.x * b.x, a.y * b.y, a.z * b.z); }
float v3_dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
vec3 v3_cross(vec3 a, vec3 b) {
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
float v3_len(vec3 a) { return sqrtf(v3_dot(a, a)); }
vec3 v3_norm(vec3 a) {
    float l = v3_len(a);
    return l > 1e-12f ? v3_scale(a, 1.0f / l) : v3(0, 0, 0);
}
vec3 v3_lerp(vec3 a, vec3 b, float t) {
    return v3(mtn_lerpf(a.x, b.x, t), mtn_lerpf(a.y, b.y, t), mtn_lerpf(a.z, b.z, t));
}
float v3_dist(vec3 a, vec3 b) { return v3_len(v3_sub(a, b)); }
float v3_dist_xz(vec3 a, vec3 b) {
    float dx = a.x - b.x, dz = a.z - b.z;
    return sqrtf(dx * dx + dz * dz);
}
vec3 v3_rotate_y(vec3 v, float a) {
    float c = cosf(a), s = sinf(a);
    return v3(c * v.x + s * v.z, v.y, -s * v.x + c * v.z);
}

mat4 m4_identity(void) {
    mat4 r;
    memset(&r, 0, sizeof r);
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

mat4 m4_mul(mat4 a, mat4 b) {
    mat4 r;
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++) {
            float s = 0.0f;
            for (int k = 0; k < 4; k++) s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}

mat4 m4_translate(vec3 t) {
    mat4 r = m4_identity();
    r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
    return r;
}

mat4 m4_scale(vec3 s) {
    mat4 r = m4_identity();
    r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
    return r;
}

mat4 m4_rotate_x(float a) {
    mat4 r = m4_identity();
    float c = cosf(a), s = sinf(a);
    r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c;
    return r;
}

mat4 m4_rotate_y(float a) {
    mat4 r = m4_identity();
    float c = cosf(a), s = sinf(a);
    r.m[0] = c; r.m[2] = -s; r.m[8] = s; r.m[10] = c;
    return r;
}

mat4 m4_rotate_z(float a) {
    mat4 r = m4_identity();
    float c = cosf(a), s = sinf(a);
    r.m[0] = c; r.m[1] = s; r.m[4] = -s; r.m[5] = c;
    return r;
}

mat4 m4_perspective(float fovy, float aspect, float n, float f) {
    mat4 r;
    memset(&r, 0, sizeof r);
    float t = 1.0f / tanf(fovy * 0.5f);
    r.m[0] = t / aspect;
    r.m[5] = t;
    r.m[10] = (f + n) / (n - f);
    r.m[11] = -1.0f;
    r.m[14] = 2.0f * f * n / (n - f);
    return r;
}

mat4 m4_ortho(float l, float r_, float b, float t, float n, float f) {
    mat4 r = m4_identity();
    r.m[0] = 2.0f / (r_ - l);
    r.m[5] = 2.0f / (t - b);
    r.m[10] = -2.0f / (f - n);
    r.m[12] = -(r_ + l) / (r_ - l);
    r.m[13] = -(t + b) / (t - b);
    r.m[14] = -(f + n) / (f - n);
    return r;
}

mat4 m4_look_at(vec3 eye, vec3 target, vec3 up) {
    vec3 f = v3_norm(v3_sub(target, eye));
    vec3 s = v3_norm(v3_cross(f, up));
    vec3 u = v3_cross(s, f);
    mat4 r = m4_identity();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -v3_dot(s, eye);
    r.m[13] = -v3_dot(u, eye);
    r.m[14] = v3_dot(f, eye);
    return r;
}

mat4 m4_from_basis(vec3 o, vec3 x, vec3 y, vec3 z) {
    mat4 r = m4_identity();
    r.m[0] = x.x; r.m[1] = x.y; r.m[2] = x.z;
    r.m[4] = y.x; r.m[5] = y.y; r.m[6] = y.z;
    r.m[8] = z.x; r.m[9] = z.y; r.m[10] = z.z;
    r.m[12] = o.x; r.m[13] = o.y; r.m[14] = o.z;
    return r;
}

mat4 m4_inverse(mat4 mm) {
    const float *m = mm.m;
    float inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
             m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
             m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
             m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
              m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
             m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
             m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
             m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
              m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
             m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
             m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
              m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
              m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
             m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
              m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
              m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    mat4 r;
    if (fabsf(det) < 1e-20f) return m4_identity();
    det = 1.0f / det;
    for (int i = 0; i < 16; i++) r.m[i] = inv[i] * det;
    return r;
}

mat4 m4_transpose(mat4 a) {
    mat4 r;
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++) r.m[c * 4 + row] = a.m[row * 4 + c];
    return r;
}

vec3 m4_mul_point(mat4 m, vec3 p) {
    vec4 r = m4_mul_v4(m, v4(p.x, p.y, p.z, 1.0f));
    if (r.w != 0.0f && r.w != 1.0f) return v3(r.x / r.w, r.y / r.w, r.z / r.w);
    return v3(r.x, r.y, r.z);
}

vec3 m4_mul_dir(mat4 m, vec3 d) {
    vec4 r = m4_mul_v4(m, v4(d.x, d.y, d.z, 0.0f));
    return v3(r.x, r.y, r.z);
}

vec4 m4_mul_v4(mat4 m, vec4 v) {
    vec4 r;
    r.x = m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12] * v.w;
    r.y = m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13] * v.w;
    r.z = m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14] * v.w;
    r.w = m.m[3] * v.x + m.m[7] * v.y + m.m[11] * v.z + m.m[15] * v.w;
    return r;
}

static float sgnf(float a) { return a > 0.0f ? 1.0f : (a < 0.0f ? -1.0f : 0.0f); }

mat4 m4_oblique_near(mat4 proj, vec4 c) {
    mat4 p = proj;
    vec4 q;
    q.x = (sgnf(c.x) + p.m[8]) / p.m[0];
    q.y = (sgnf(c.y) + p.m[9]) / p.m[5];
    q.z = -1.0f;
    q.w = (1.0f + p.m[10]) / p.m[14];
    float d = c.x * q.x + c.y * q.y + c.z * q.z + c.w * q.w;
    if (fabsf(d) < 1e-12f) return proj;
    float s = 2.0f / d;
    p.m[2] = c.x * s;
    p.m[6] = c.y * s;
    p.m[10] = c.z * s + 1.0f;
    p.m[14] = c.w * s;
    return p;
}

static vec4 plane_norm(vec4 p) {
    float l = sqrtf(p.x * p.x + p.y * p.y + p.z * p.z);
    if (l < 1e-20f) return p;
    return v4(p.x / l, p.y / l, p.z / l, p.w / l);
}

mtn_frustum mtn_frustum_from(mat4 vp) {
    mtn_frustum f;
    const float *m = vp.m;
    vec4 r0 = v4(m[0], m[4], m[8], m[12]);
    vec4 r1 = v4(m[1], m[5], m[9], m[13]);
    vec4 r2 = v4(m[2], m[6], m[10], m[14]);
    vec4 r3 = v4(m[3], m[7], m[11], m[15]);
    f.p[0] = plane_norm(v4(r3.x + r0.x, r3.y + r0.y, r3.z + r0.z, r3.w + r0.w));
    f.p[1] = plane_norm(v4(r3.x - r0.x, r3.y - r0.y, r3.z - r0.z, r3.w - r0.w));
    f.p[2] = plane_norm(v4(r3.x + r1.x, r3.y + r1.y, r3.z + r1.z, r3.w + r1.w));
    f.p[3] = plane_norm(v4(r3.x - r1.x, r3.y - r1.y, r3.z - r1.z, r3.w - r1.w));
    f.p[4] = plane_norm(v4(r3.x + r2.x, r3.y + r2.y, r3.z + r2.z, r3.w + r2.w));
    f.p[5] = plane_norm(v4(r3.x - r2.x, r3.y - r2.y, r3.z - r2.z, r3.w - r2.w));
    return f;
}

int mtn_frustum_sphere(const mtn_frustum *f, vec3 c, float r) {
    for (int i = 0; i < 6; i++) {
        const vec4 *p = &f->p[i];
        if (p->x * c.x + p->y * c.y + p->z * c.z + p->w < -r) return 0;
    }
    return 1;
}

int mtn_frustum_aabb(const mtn_frustum *f, vec3 mn, vec3 mx) {
    for (int i = 0; i < 6; i++) {
        const vec4 *p = &f->p[i];
        vec3 v = v3(p->x >= 0 ? mx.x : mn.x, p->y >= 0 ? mx.y : mn.y, p->z >= 0 ? mx.z : mn.z);
        if (p->x * v.x + p->y * v.y + p->z * v.z + p->w < 0) return 0;
    }
    return 1;
}

mat4 mtn_portal_frame(vec3 origin, vec3 forward) {
    vec3 z = v3_norm(v3(forward.x, 0.0f, forward.z));
    vec3 y = v3(0, 1, 0);
    vec3 x = v3_cross(y, z);
    return m4_from_basis(origin, x, y, z);
}

mat4 mtn_portal_transform(mat4 a, mat4 b) {
    return m4_mul(b, m4_mul(m4_rotate_y(MTN_PI), m4_inverse(a)));
}

uint32_t mtn_pack_rgba(float r, float g, float b, float a) {
    uint32_t R = (uint32_t)(mtn_clampf(r, 0, 1) * 255.0f + 0.5f);
    uint32_t G = (uint32_t)(mtn_clampf(g, 0, 1) * 255.0f + 0.5f);
    uint32_t B = (uint32_t)(mtn_clampf(b, 0, 1) * 255.0f + 0.5f);
    uint32_t A = (uint32_t)(mtn_clampf(a, 0, 1) * 255.0f + 0.5f);
    return R | (G << 8) | (B << 16) | (A << 24);
}
