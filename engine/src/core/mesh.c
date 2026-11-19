/* Mountain engine - CPU mesh building (3D primitives, billboards, voxels). MIT License. */
#include "mtn/mesh.h"
#include "mtn/rng.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void mtn_mesh_init(mtn_mesh *m) { memset(m, 0, sizeof *m); }

void mtn_mesh_free(mtn_mesh *m) {
    free(m->v);
    free(m->idx);
    memset(m, 0, sizeof *m);
}

void mtn_mesh_clear(mtn_mesh *m) {
    m->vcount = 0;
    m->icount = 0;
}

int mtn_mesh_vertex(mtn_mesh *m, const mtn_vertex *v) {
    if (m->vcount == m->vcap) {
        m->vcap = m->vcap ? m->vcap * 2 : 256;
        m->v = realloc(m->v, sizeof(mtn_vertex) * (size_t)m->vcap);
    }
    m->v[m->vcount] = *v;
    return m->vcount++;
}

void mtn_mesh_tri(mtn_mesh *m, int a, int b, int c) {
    if (m->icount + 3 > m->icap) {
        m->icap = m->icap ? m->icap * 2 : 384;
        m->idx = realloc(m->idx, sizeof(uint32_t) * (size_t)m->icap);
    }
    m->idx[m->icount++] = (uint32_t)a;
    m->idx[m->icount++] = (uint32_t)b;
    m->idx[m->icount++] = (uint32_t)c;
}

void mtn_mesh_append(mtn_mesh *dst, const mtn_mesh *src, mat4 xf) {
    int base = dst->vcount;
    for (int i = 0; i < src->vcount; i++) {
        mtn_vertex v = src->v[i];
        vec3 p = m4_mul_point(xf, v3(v.x, v.y, v.z));
        v.x = p.x; v.y = p.y; v.z = p.z;
        if (!(v.flags & MTN_VF_BILLBOARD)) {
            vec3 n = v3_norm(m4_mul_dir(xf, v3(v.nx, v.ny, v.nz)));
            v.nx = n.x; v.ny = n.y; v.nz = n.z;
        }
        mtn_mesh_vertex(dst, &v);
    }
    for (int i = 0; i + 2 < src->icount; i += 3)
        mtn_mesh_tri(dst, base + (int)src->idx[i], base + (int)src->idx[i + 1], base + (int)src->idx[i + 2]);
}

void mtn_mesh_bounds(const mtn_mesh *m, vec3 *mn, vec3 *mx) {
    *mn = v3(1e30f, 1e30f, 1e30f);
    *mx = v3(-1e30f, -1e30f, -1e30f);
    for (int i = 0; i < m->vcount; i++) {
        const mtn_vertex *v = &m->v[i];
        float pad = (v->flags & MTN_VF_BILLBOARD) ? fabsf(v->nx) + fabsf(v->ny) : 0.0f;
        mn->x = mtn_minf(mn->x, v->x - pad); mx->x = mtn_maxf(mx->x, v->x + pad);
        mn->y = mtn_minf(mn->y, v->y - pad); mx->y = mtn_maxf(mx->y, v->y + pad);
        mn->z = mtn_minf(mn->z, v->z - pad); mx->z = mtn_maxf(mx->z, v->z + pad);
    }
}

uint32_t mtn_rgb(int r, int g, int b) {
    return (uint32_t)mtn_clampi(r, 0, 255) | ((uint32_t)mtn_clampi(g, 0, 255) << 8) |
           ((uint32_t)mtn_clampi(b, 0, 255) << 16) | 0xFF000000u;
}

uint32_t mtn_rgb_scale(uint32_t c, float s) {
    int r = (int)((float)(c & 255) * s), g = (int)((float)((c >> 8) & 255) * s), b = (int)((float)((c >> 16) & 255) * s);
    return (mtn_rgb(r, g, b) & 0x00FFFFFFu) | (c & 0xFF000000u);
}

mtn_material mtn_mat(uint32_t rgba, int tile, int flags, float uv_scale) {
    mtn_material m;
    m.r = (uint8_t)(rgba & 255);
    m.g = (uint8_t)((rgba >> 8) & 255);
    m.b = (uint8_t)((rgba >> 16) & 255);
    m.a = (uint8_t)((rgba >> 24) & 255);
    m.tile = (uint8_t)tile;
    m.flags = (uint8_t)flags;
    m.extra = 0;
    m.uv_scale = uv_scale;
    return m;
}

static mtn_vertex make_vertex(vec3 p, vec3 n, vec2 uv, const mtn_material *mat) {
    mtn_vertex v;
    v.x = p.x; v.y = p.y; v.z = p.z;
    v.nx = n.x; v.ny = n.y; v.nz = n.z;
    v.u = uv.x; v.v = uv.y;
    v.r = mat->r; v.g = mat->g; v.b = mat->b; v.a = mat->a;
    v.tile = mat->tile; v.flags = mat->flags; v.extra = mat->extra; v.ao = 255;
    return v;
}

void mtn_mesh_quad(mtn_mesh *m, mat4 xf, vec3 p0, vec3 p1, vec3 p2, vec3 p3, const mtn_material *mat, vec2 uv0,
                   vec2 uv1) {
    vec3 w0 = m4_mul_point(xf, p0), w1 = m4_mul_point(xf, p1), w2 = m4_mul_point(xf, p2), w3 = m4_mul_point(xf, p3);
    vec3 n = v3_cross(v3_sub(w1, w0), v3_sub(w2, w0));
    /* a collapsed corner (e.g. the pole of a rock) makes the first triangle degenerate: use the second */
    if (v3_dot(n, n) < 1e-12f) n = v3_cross(v3_sub(w2, w0), v3_sub(w3, w0));
    n = v3_norm(n);
    mtn_vertex a = make_vertex(w0, n, v2(uv0.x, uv1.y), mat);
    mtn_vertex b = make_vertex(w1, n, v2(uv1.x, uv1.y), mat);
    mtn_vertex c = make_vertex(w2, n, v2(uv1.x, uv0.y), mat);
    mtn_vertex d = make_vertex(w3, n, v2(uv0.x, uv0.y), mat);
    int ia = mtn_mesh_vertex(m, &a), ib = mtn_mesh_vertex(m, &b), ic = mtn_mesh_vertex(m, &c), id = mtn_mesh_vertex(m, &d);
    mtn_mesh_tri(m, ia, ib, ic);
    mtn_mesh_tri(m, ia, ic, id);
}

void mtn_mesh_box(mtn_mesh *m, mat4 xf, vec3 a, vec3 b, const mtn_material *mat, int faces) {
    float s = mat->uv_scale;
    float dx = (b.x - a.x) * s, dy = (b.y - a.y) * s, dz = (b.z - a.z) * s;
    /* uv1.y is the bottom of the texture (texture v grows downwards in the atlas) */
    if (faces & 1) mtn_mesh_quad(m, xf, v3(a.x, a.y, a.z), v3(a.x, a.y, b.z), v3(a.x, b.y, b.z), v3(a.x, b.y, a.z), mat, v2(0, 0), v2(dz, dy));
    if (faces & 2) mtn_mesh_quad(m, xf, v3(b.x, a.y, b.z), v3(b.x, a.y, a.z), v3(b.x, b.y, a.z), v3(b.x, b.y, b.z), mat, v2(0, 0), v2(dz, dy));
    if (faces & 4) mtn_mesh_quad(m, xf, v3(a.x, a.y, a.z), v3(b.x, a.y, a.z), v3(b.x, a.y, b.z), v3(a.x, a.y, b.z), mat, v2(0, 0), v2(dx, dz));
    if (faces & 8) mtn_mesh_quad(m, xf, v3(a.x, b.y, b.z), v3(b.x, b.y, b.z), v3(b.x, b.y, a.z), v3(a.x, b.y, a.z), mat, v2(0, 0), v2(dx, dz));
    if (faces & 16) mtn_mesh_quad(m, xf, v3(b.x, a.y, a.z), v3(a.x, a.y, a.z), v3(a.x, b.y, a.z), v3(b.x, b.y, a.z), mat, v2(0, 0), v2(dx, dy));
    if (faces & 32) mtn_mesh_quad(m, xf, v3(a.x, a.y, b.z), v3(b.x, a.y, b.z), v3(b.x, b.y, b.z), v3(a.x, b.y, b.z), mat, v2(0, 0), v2(dx, dy));
}

void mtn_mesh_cylinder(mtn_mesh *m, mat4 xf, vec3 a, vec3 b, float ra, float rb, int sides, const mtn_material *mat,
                       int caps) {
    if (sides < 3) sides = 3;
    vec3 axis = v3_sub(b, a);
    float len = v3_len(axis);
    vec3 d = v3_norm(axis);
    vec3 ref = fabsf(d.y) < 0.9f ? v3(0, 1, 0) : v3(1, 0, 0);
    vec3 u = v3_norm(v3_cross(d, ref));
    vec3 w = v3_cross(d, u);
    int base = m->vcount;
    float circ = MTN_TAU * 0.5f * (ra + rb) * mat->uv_scale;
    float slope = (ra - rb) / (len > 1e-6f ? len : 1.0f);
    for (int i = 0; i <= sides; i++) {
        float t = (float)i / (float)sides;
        float an = t * MTN_TAU;
        vec3 radial = v3_add(v3_scale(u, cosf(an)), v3_scale(w, sinf(an)));
        vec3 n = v3_norm(v3_add(radial, v3_scale(d, slope)));
        vec3 pa = v3_add(a, v3_scale(radial, ra));
        vec3 pb = v3_add(b, v3_scale(radial, rb));
        mtn_vertex va = make_vertex(m4_mul_point(xf, pa), v3_norm(m4_mul_dir(xf, n)), v2(t * circ, len * mat->uv_scale), mat);
        mtn_vertex vb = make_vertex(m4_mul_point(xf, pb), v3_norm(m4_mul_dir(xf, n)), v2(t * circ, 0), mat);
        mtn_mesh_vertex(m, &va);
        mtn_mesh_vertex(m, &vb);
    }
    for (int i = 0; i < sides; i++) {
        int i0 = base + i * 2;
        mtn_mesh_tri(m, i0, i0 + 2, i0 + 3);
        mtn_mesh_tri(m, i0, i0 + 3, i0 + 1);
    }
    if (caps) {
        for (int end = 0; end < 2; end++) {
            vec3 c = end ? b : a;
            float r = end ? rb : ra;
            vec3 n = end ? d : v3_scale(d, -1.0f);
            mtn_vertex vc = make_vertex(m4_mul_point(xf, c), v3_norm(m4_mul_dir(xf, n)), v2(0.5f, 0.5f), mat);
            int ci = mtn_mesh_vertex(m, &vc);
            int rim = m->vcount;
            for (int i = 0; i <= sides; i++) {
                float an = (float)i / (float)sides * MTN_TAU;
                vec3 radial = v3_add(v3_scale(u, cosf(an)), v3_scale(w, sinf(an)));
                vec3 p = v3_add(c, v3_scale(radial, r));
                mtn_vertex vr = make_vertex(m4_mul_point(xf, p), v3_norm(m4_mul_dir(xf, n)),
                                            v2(0.5f + 0.5f * cosf(an), 0.5f + 0.5f * sinf(an)), mat);
                mtn_mesh_vertex(m, &vr);
            }
            for (int i = 0; i < sides; i++) {
                if (end) mtn_mesh_tri(m, ci, rim + i, rim + i + 1);
                else mtn_mesh_tri(m, ci, rim + i + 1, rim + i);
            }
        }
    }
}

void mtn_mesh_billboard(mtn_mesh *m, vec3 c, float w, float h, float anchor, const mtn_material *mat, float wind) {
    float hw = w * 0.5f;
    float y0 = -h * 0.5f * (1.0f - anchor), y1 = y0 + h;
    const float cx[4] = {-hw, hw, hw, -hw};
    const float cy[4] = {y0, y0, y1, y1};
    const float uu[4] = {0, 1, 1, 0};
    const float vv[4] = {1, 1, 0, 0};
    int base = m->vcount;
    for (int i = 0; i < 4; i++) {
        mtn_vertex v = make_vertex(c, v3(cx[i], cy[i], cy[i] > y0 ? wind : 0.0f), v2(uu[i], vv[i]), mat);
        v.flags |= MTN_VF_BILLBOARD | MTN_VF_ABS_UV;
        mtn_mesh_vertex(m, &v);
    }
    mtn_mesh_tri(m, base, base + 1, base + 2);
    mtn_mesh_tri(m, base, base + 2, base + 3);
}

void mtn_mesh_rock(mtn_mesh *m, mat4 xf, float radius, uint32_t seed, const mtn_material *mat) {
    /* Displaced UV sphere (5 rings x 9 segments): a craggy, flat shaded low-poly boulder. */
    enum { RINGS = 5, SEGS = 9 };
    vec3 p[RINGS + 1][SEGS];
    for (int r = 0; r <= RINGS; r++) {
        float phi = (float)r / (float)RINGS * MTN_PI;
        for (int s = 0; s < SEGS; s++) {
            float th = (float)s / (float)SEGS * MTN_TAU;
            vec3 d = v3(sinf(phi) * cosf(th), cosf(phi), sinf(phi) * sinf(th));
            float jitter = 0.72f + 0.45f * mtn_hash_unit(mtn_hash3i(r, s, 0, seed));
            if (r == 0 || r == RINGS) jitter = 0.8f;
            vec3 q = v3_scale(d, radius * jitter);
            q.y *= 0.72f; /* boulders are flatter than they are wide */
            p[r][s] = q;
        }
    }
    for (int r = 0; r < RINGS; r++)
        for (int s = 0; s < SEGS; s++) {
            int s2 = (s + 1) % SEGS;
            vec3 a = p[r][s], b = p[r][s2], c = p[r + 1][s2], d = p[r + 1][s];
            /* flat shaded facets */
            mtn_mesh_quad(m, xf, a, b, c, d, mat, v2(0, 0), v2(radius * mat->uv_scale, radius * mat->uv_scale));
        }
}

/* ------------------------------------------------------------------ voxels */

void mtn_voxels_init(mtn_voxels *vx, int sx, int sy, int sz, float size) {
    vx->sx = sx; vx->sy = sy; vx->sz = sz;
    vx->size = size;
    vx->v = calloc((size_t)sx * (size_t)sy * (size_t)sz, 1);
    for (int i = 0; i < 256; i++) vx->pal[i] = mtn_rgb(i, i, i);
}

void mtn_voxels_free(mtn_voxels *vx) {
    free(vx->v);
    vx->v = NULL;
}

void mtn_voxels_set(mtn_voxels *vx, int x, int y, int z, uint8_t c) {
    if (x < 0 || y < 0 || z < 0 || x >= vx->sx || y >= vx->sy || z >= vx->sz) return;
    vx->v[((size_t)z * (size_t)vx->sy + (size_t)y) * (size_t)vx->sx + (size_t)x] = c;
}

uint8_t mtn_voxels_get(const mtn_voxels *vx, int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= vx->sx || y >= vx->sy || z >= vx->sz) return 0;
    return vx->v[((size_t)z * (size_t)vx->sy + (size_t)y) * (size_t)vx->sx + (size_t)x];
}

void mtn_voxels_box(mtn_voxels *vx, int x0, int y0, int z0, int x1, int y1, int z1, uint8_t c) {
    for (int z = z0; z <= z1; z++)
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) mtn_voxels_set(vx, x, y, z, c);
}

void mtn_voxels_blob(mtn_voxels *vx, float cx, float cy, float cz, float r, uint8_t c, uint32_t seed) {
    int r1 = (int)ceilf(r * 1.3f);
    for (int z = (int)cz - r1; z <= (int)cz + r1; z++)
        for (int y = (int)cy - r1; y <= (int)cy + r1; y++)
            for (int x = (int)cx - r1; x <= (int)cx + r1; x++) {
                float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, dz = (float)z + 0.5f - cz;
                float d = sqrtf(dx * dx + dy * dy * 1.3f + dz * dz);
                float nr = r * (0.85f + 0.3f * mtn_hash_unit(mtn_hash3i(x / 2, y / 2, z / 2, seed)));
                if (d <= nr) mtn_voxels_set(vx, x, y, z, c);
            }
}

int mtn_voxels_count(const mtn_voxels *vx) {
    int n = 0;
    size_t total = (size_t)vx->sx * (size_t)vx->sy * (size_t)vx->sz;
    for (size_t i = 0; i < total; i++) n += vx->v[i] != 0;
    return n;
}

static int solid(const mtn_voxels *vx, int x, int y, int z) { return mtn_voxels_get(vx, x, y, z) != 0; }

void mtn_voxels_mesh(const mtn_voxels *vx, mtn_mesh *m, mat4 xf, int tile, int flags) {
    static const int dirs[6][3] = {{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}};
    /* corner offsets of each face (ccw from outside), in voxel units */
    static const float corners[6][4][3] = {
        {{0, 0, 0}, {0, 0, 1}, {0, 1, 1}, {0, 1, 0}},
        {{1, 0, 1}, {1, 0, 0}, {1, 1, 0}, {1, 1, 1}},
        {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1}},
        {{0, 1, 1}, {1, 1, 1}, {1, 1, 0}, {0, 1, 0}},
        {{1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {1, 1, 0}},
        {{0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}},
    };
    float s = vx->size;
    for (int z = 0; z < vx->sz; z++)
        for (int y = 0; y < vx->sy; y++)
            for (int x = 0; x < vx->sx; x++) {
                uint8_t c = mtn_voxels_get(vx, x, y, z);
                if (!c) continue;
                float jit = 0.88f + 0.24f * mtn_hash_unit(mtn_hash3i(x, y, z, 99));
                uint32_t col = mtn_rgb_scale(vx->pal[c], jit);
                for (int f = 0; f < 6; f++) {
                    int nx = x + dirs[f][0], ny = y + dirs[f][1], nz = z + dirs[f][2];
                    if (solid(vx, nx, ny, nz)) continue;
                    mtn_material mat = mtn_mat(col, tile, flags, 1.0f);
                    vec3 n = v3_norm(m4_mul_dir(xf, v3((float)dirs[f][0], (float)dirs[f][1], (float)dirs[f][2])));
                    int idx[4];
                    for (int k = 0; k < 4; k++) {
                        const float *co = corners[f][k];
                        vec3 p = v3(((float)x + co[0]) * s, ((float)y + co[1]) * s, ((float)z + co[2]) * s);
                        /* ambient occlusion from the 3 voxels touching this corner in front of the face */
                        int ax = dirs[f][0] != 0 ? 1 : 0, ay = dirs[f][1] != 0 ? 1 : 0;
                        int t1[3] = {0, 0, 0}, t2[3] = {0, 0, 0};
                        if (ax) { t1[1] = co[1] > 0.5f ? 1 : -1; t2[2] = co[2] > 0.5f ? 1 : -1; }
                        else if (ay) { t1[0] = co[0] > 0.5f ? 1 : -1; t2[2] = co[2] > 0.5f ? 1 : -1; }
                        else { t1[0] = co[0] > 0.5f ? 1 : -1; t2[1] = co[1] > 0.5f ? 1 : -1; }
                        int s1 = solid(vx, nx + t1[0], ny + t1[1], nz + t1[2]);
                        int s2 = solid(vx, nx + t2[0], ny + t2[1], nz + t2[2]);
                        int sc = solid(vx, nx + t1[0] + t2[0], ny + t1[1] + t2[1], nz + t1[2] + t2[2]);
                        int occ = (s1 && s2) ? 3 : s1 + s2 + sc;
                        mtn_vertex v = make_vertex(m4_mul_point(xf, p), n, v2(co[0] + co[2], co[1]), &mat);
                        v.ao = (uint8_t)(255 - occ * 50);
                        idx[k] = mtn_mesh_vertex(m, &v);
                    }
                    mtn_mesh_tri(m, idx[0], idx[1], idx[2]);
                    mtn_mesh_tri(m, idx[0], idx[2], idx[3]);
                }
            }
}
