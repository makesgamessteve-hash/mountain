/* Mountain engine - OpenGL renderer: low resolution scene with retro post
 * processing, sky, LOD terrain, chunked flora, wildlife, the magic house with
 * stencil portals for its door and windows, weather particles and the UI.
 * Works on OpenGL 3.3 core and OpenGL ES 3.0 / WebGL2. MIT License. */
#include "render/renderer.h"
#include "render/gl.h"

#include "mtn/photo.h"
#include "mtn/rng.h"
#include "mtn/vfs.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TCHUNK 32          /* terrain cells per chunk side */
#define MAX_TCHUNKS 1024
#define MAX_FCHUNKS 4096
#define MAX_THUMBS 32
#define MAX_UNIFORMS 64
#define PRECIP_COUNT 3000

typedef struct {
    GLuint vao, vbo, ibo;
    int count;
    int vcap, icap;
    vec3 mn, mx;
} gmesh;

typedef struct {
    GLuint id;
    int n;
    const char *names[MAX_UNIFORMS];
    GLint locs[MAX_UNIFORMS];
} prog;

typedef struct {
    gmesh solid, leaves;
    int lod, version, built;
} fchunk;

struct mtn_renderer {
    prog sh_world, sh_terrain, sh_water, sh_sky, sh_post, sh_ui, sh_precip, sh_portal;
    GLuint tex_foliage, tex_materials, tex_terrain, tex_moon, tex_font, tex_icons, tex_white;
    GLuint fbo, color_tex, depth_rb, cap_fbo, cap_tex;
    int lw, lh;
    GLuint empty_vao;

    int world_version;
    int tchunks;
    gmesh terrain[3][MAX_TCHUNKS];
    gmesh water[MAX_TCHUNKS];
    gmesh trail, cairns;
    fchunk *flora;
    int fchunks;
    gmesh ground;
    vec3 ground_center;
    int ground_valid;
    gmesh fauna_solid, fauna_sprites, fx, precip, portal_quad, photo_quad;
    gmesh house_solid, house_sprites, house_glow, door;
    int house_version;
    gmesh room_solid[3], room_sprites[3], room_glow[3];
    int rooms_version;
    GLuint ui_vao, ui_vbo;
    int ui_cap;

    char thumb_names[MAX_THUMBS][96];
    GLuint thumbs[MAX_THUMBS];
    int thumb_next;

    mtn_mesh scratch_a, scratch_b;
    float time;
    mtn_render_stats stats;
    /* per view state */
    mat4 view, proj, viewproj;
    vec3 eye;
    mtn_frustum frustum;
    int space; /* 0 world, 1 interior */
    int depth;
    mtn_game *g;
};

/* ------------------------------------------------------------------ GL helpers */

static GLint uloc(prog *p, const char *name) {
    for (int i = 0; i < p->n; i++)
        if (p->names[i] == name || strcmp(p->names[i], name) == 0) return p->locs[i];
    GLint l = glGetUniformLocation(p->id, name);
    if (p->n < MAX_UNIFORMS) {
        p->names[p->n] = name;
        p->locs[p->n] = l;
        p->n++;
    }
    return l;
}

static void u1f(prog *p, const char *n, float v) { glUniform1f(uloc(p, n), v); }
static void u1i(prog *p, const char *n, int v) { glUniform1i(uloc(p, n), v); }
static void u2f(prog *p, const char *n, float a, float b) { glUniform2f(uloc(p, n), a, b); }
static void u3v(prog *p, const char *n, vec3 v) { glUniform3f(uloc(p, n), v.x, v.y, v.z); }
static void um4(prog *p, const char *n, mat4 m) { glUniformMatrix4fv(uloc(p, n), 1, GL_FALSE, m.m); }

static GLuint compile(GLenum type, const char *src, const char *name, char *err, size_t n) {
    const char *header = MTN_GLES ? "#version 300 es\nprecision highp float;\nprecision highp int;\n" : "#version 330 core\n";
    const char *srcs[2] = {header, src};
    GLuint s = glCreateShader(type);
    glShaderSource(s, 2, srcs, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof log, NULL, log);
        snprintf(err, n, "%s: %s", name, log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static int load_prog(prog *p, const char *base, char *err, size_t n) {
    char vpath[128], fpath[128];
    snprintf(vpath, sizeof vpath, "shaders/%s.vert", base);
    snprintf(fpath, sizeof fpath, "shaders/%s.frag", base);
    char *vs = mtn_vfs_read(vpath, NULL), *fs = mtn_vfs_read(fpath, NULL);
    if (!vs || !fs) {
        snprintf(err, n, "missing shader %s", vs ? fpath : vpath);
        free(vs);
        free(fs);
        return 0;
    }
    GLuint v = compile(GL_VERTEX_SHADER, vs, vpath, err, n);
    GLuint f = v ? compile(GL_FRAGMENT_SHADER, fs, fpath, err, n) : 0;
    free(vs);
    free(fs);
    if (!v || !f) return 0;
    memset(p, 0, sizeof *p);
    p->id = glCreateProgram();
    glAttachShader(p->id, v);
    glAttachShader(p->id, f);
    glBindAttribLocation(p->id, 0, "a_pos");
    glBindAttribLocation(p->id, 1, "a_nrm");
    glBindAttribLocation(p->id, 2, "a_uv");
    glBindAttribLocation(p->id, 3, "a_col");
    glBindAttribLocation(p->id, 4, "a_info");
    glLinkProgram(p->id);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p->id, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p->id, sizeof log, NULL, log);
        snprintf(err, n, "link %s: %s", base, log);
        return 0;
    }
    return 1;
}

static GLuint make_texture(const uint8_t *rgba, int w, int h, int linear) {
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return t;
}

static GLuint load_texture(const char *rel, char *err, size_t n) {
    size_t size;
    char *data = mtn_vfs_read(rel, &size);
    if (!data) {
        snprintf(err, n, "missing texture %s", rel);
        return 0;
    }
    int w, h;
    uint8_t *px = mtn_png_decode((const uint8_t *)data, size, &w, &h);
    free(data);
    if (!px) {
        snprintf(err, n, "bad texture %s", rel);
        return 0;
    }
    GLuint t = make_texture(px, w, h, 0);
    mtn_image_free(px);
    return t;
}

static void gmesh_setup(gmesh *m) {
    glGenVertexArrays(1, &m->vao);
    glGenBuffers(1, &m->vbo);
    glGenBuffers(1, &m->ibo);
    glBindVertexArray(m->vao);
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ibo);
    GLsizei st = sizeof(mtn_vertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, st, (void *)offsetof(mtn_vertex, x));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, st, (void *)offsetof(mtn_vertex, nx));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, st, (void *)offsetof(mtn_vertex, u));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, st, (void *)offsetof(mtn_vertex, r));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_FALSE, st, (void *)offsetof(mtn_vertex, tile));
    glBindVertexArray(0);
}

static void gmesh_upload(gmesh *m, const mtn_mesh *src, int dynamic) {
    if (!m->vao) gmesh_setup(m);
    glBindVertexArray(m->vao);
    GLenum usage = dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    if (dynamic && src->vcount <= m->vcap && m->vcap > 0) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(mtn_vertex) * (size_t)src->vcount), src->v);
    } else {
        int cap = dynamic ? src->vcount * 2 + 64 : src->vcount;
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(mtn_vertex) * (size_t)cap), NULL, usage);
        if (src->vcount) glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(mtn_vertex) * (size_t)src->vcount), src->v);
        m->vcap = cap;
    }
    if (dynamic && src->icount <= m->icap && m->icap > 0) {
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(uint32_t) * (size_t)src->icount), src->idx);
    } else {
        int cap = dynamic ? src->icount * 2 + 96 : src->icount;
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(sizeof(uint32_t) * (size_t)cap), NULL, usage);
        if (src->icount) glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(uint32_t) * (size_t)src->icount), src->idx);
        m->icap = cap;
    }
    glBindVertexArray(0);
    m->count = src->icount;
    if (src->vcount) mtn_mesh_bounds(src, &m->mn, &m->mx);
    else m->mn = m->mx = v3(0, 0, 0);
}

static void gmesh_free(gmesh *m) {
    if (m->vao) {
        glDeleteVertexArrays(1, &m->vao);
        glDeleteBuffers(1, &m->vbo);
        glDeleteBuffers(1, &m->ibo);
    }
    memset(m, 0, sizeof *m);
}

static void gmesh_draw(mtn_renderer *r, const gmesh *m) {
    if (!m->vao || m->count == 0) return;
    glBindVertexArray(m->vao);
    glDrawElements(GL_TRIANGLES, m->count, GL_UNSIGNED_INT, 0);
    r->stats.draw_calls++;
    r->stats.triangles += m->count / 3;
}

static int visible(mtn_renderer *r, const gmesh *m) {
    return m->count > 0 && mtn_frustum_aabb(&r->frustum, m->mn, m->mx);
}

/* ------------------------------------------------------------------ create/load */

mtn_renderer *mtn_render_create(void) {
    mtn_renderer *r = calloc(1, sizeof *r);
    mtn_mesh_init(&r->scratch_a);
    mtn_mesh_init(&r->scratch_b);
    r->world_version = -1;
    r->house_version = -1;
    r->rooms_version = -1;
    return r;
}

int mtn_render_load(mtn_renderer *r, char *err, size_t n) {
    if (!load_prog(&r->sh_world, "world", err, n) || !load_prog(&r->sh_terrain, "terrain", err, n) ||
        !load_prog(&r->sh_water, "water", err, n) || !load_prog(&r->sh_sky, "sky", err, n) || !load_prog(&r->sh_post, "post", err, n) ||
        !load_prog(&r->sh_ui, "ui", err, n) || !load_prog(&r->sh_precip, "precip", err, n) || !load_prog(&r->sh_portal, "portal", err, n))
        return 0;
    if (!(r->tex_foliage = load_texture("textures/foliage.png", err, n)) ||
        !(r->tex_materials = load_texture("textures/materials.png", err, n)) ||
        !(r->tex_terrain = load_texture("textures/terrain.png", err, n)) || !(r->tex_moon = load_texture("textures/moon.png", err, n)) ||
        !(r->tex_font = load_texture("fonts/font8x8.png", err, n)) || !(r->tex_icons = load_texture("textures/icons.png", err, n)))
        return 0;
    glBindTexture(GL_TEXTURE_2D, r->tex_moon);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    uint8_t white[4] = {255, 255, 255, 255};
    r->tex_white = make_texture(white, 1, 1, 0);
    glGenVertexArrays(1, &r->empty_vao);
    glGenVertexArrays(1, &r->ui_vao);
    glGenBuffers(1, &r->ui_vbo);
    glBindVertexArray(r->ui_vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->ui_vbo);
    GLsizei st = sizeof(mtn_vertex);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, st, (void *)offsetof(mtn_vertex, x));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, st, (void *)offsetof(mtn_vertex, u));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, st, (void *)offsetof(mtn_vertex, r));
    glBindVertexArray(0);
    /* precipitation particles: static random positions, animated in the shader */
    mtn_mesh m;
    mtn_mesh_init(&m);
    mtn_rng rng;
    mtn_rng_seed(&rng, 99, 1);
    for (int i = 0; i < PRECIP_COUNT; i++) {
        vec3 p = v3(mtn_rng_float(&rng), mtn_rng_float(&rng), mtn_rng_float(&rng));
        int idx = (int)((float)i / PRECIP_COUNT * 65535.0f);
        const float cx[4] = {-1, 1, 1, -1}, cy[4] = {-1, -1, 1, 1};
        int base = m.vcount;
        for (int k = 0; k < 4; k++) {
            mtn_vertex v;
            memset(&v, 0, sizeof v);
            v.x = p.x; v.y = p.y; v.z = p.z;
            v.nx = cx[k]; v.ny = cy[k];
            v.extra = (uint8_t)(idx >> 8);
            v.ao = (uint8_t)(idx & 255);
            mtn_mesh_vertex(&m, &v);
        }
        mtn_mesh_tri(&m, base, base + 1, base + 2);
        mtn_mesh_tri(&m, base, base + 2, base + 3);
    }
    gmesh_upload(&r->precip, &m, 0);
    mtn_mesh_free(&m);
    return 1;
}

static void free_world(mtn_renderer *r) {
    for (int l = 0; l < 3; l++)
        for (int i = 0; i < MAX_TCHUNKS; i++) gmesh_free(&r->terrain[l][i]);
    for (int i = 0; i < MAX_TCHUNKS; i++) gmesh_free(&r->water[i]);
    gmesh_free(&r->trail);
    gmesh_free(&r->cairns);
    if (r->flora) {
        for (int i = 0; i < r->fchunks; i++) {
            gmesh_free(&r->flora[i].solid);
            gmesh_free(&r->flora[i].leaves);
        }
        free(r->flora);
        r->flora = NULL;
    }
    gmesh_free(&r->ground);
    r->ground_valid = 0;
}

void mtn_render_destroy(mtn_renderer *r) {
    if (!r) return;
    free_world(r);
    gmesh_free(&r->fauna_solid);
    gmesh_free(&r->fauna_sprites);
    gmesh_free(&r->fx);
    gmesh_free(&r->precip);
    gmesh_free(&r->portal_quad);
    gmesh_free(&r->photo_quad);
    gmesh_free(&r->house_solid);
    gmesh_free(&r->house_sprites);
    gmesh_free(&r->house_glow);
    gmesh_free(&r->door);
    for (int i = 0; i < 3; i++) {
        gmesh_free(&r->room_solid[i]);
        gmesh_free(&r->room_sprites[i]);
        gmesh_free(&r->room_glow[i]);
    }
    if (r->fbo) {
        glDeleteFramebuffers(1, &r->fbo);
        glDeleteTextures(1, &r->color_tex);
        glDeleteRenderbuffers(1, &r->depth_rb);
    }
    if (r->cap_fbo) {
        glDeleteFramebuffers(1, &r->cap_fbo);
        glDeleteTextures(1, &r->cap_tex);
    }
    for (int i = 0; i < MAX_THUMBS; i++)
        if (r->thumbs[i]) glDeleteTextures(1, &r->thumbs[i]);
    GLuint texs[7] = {r->tex_foliage, r->tex_materials, r->tex_terrain, r->tex_moon, r->tex_font, r->tex_icons, r->tex_white};
    glDeleteTextures(7, texs);
    prog *ps[8] = {&r->sh_world, &r->sh_terrain, &r->sh_water, &r->sh_sky, &r->sh_post, &r->sh_ui, &r->sh_precip, &r->sh_portal};
    for (int i = 0; i < 8; i++)
        if (ps[i]->id) glDeleteProgram(ps[i]->id);
    if (r->empty_vao) glDeleteVertexArrays(1, &r->empty_vao);
    if (r->ui_vao) {
        glDeleteVertexArrays(1, &r->ui_vao);
        glDeleteBuffers(1, &r->ui_vbo);
    }
    mtn_mesh_free(&r->scratch_a);
    mtn_mesh_free(&r->scratch_b);
    free(r);
}

mtn_render_stats mtn_render_get_stats(const mtn_renderer *r) { return r->stats; }

/* ------------------------------------------------------------------ world meshes */

static uint8_t u8f(float v) { return (uint8_t)mtn_clampf(v * 255.0f + 0.5f, 0.0f, 255.0f); }

static void terrain_vertex(const mtn_terrain *t, int x, int z, mtn_vertex *v) {
    float wx = (float)x * t->cell, wz = (float)z * t->cell;
    float h = mtn_terrain_grid(t, x, z);
    vec3 n = mtn_terrain_normal(t, wx, wz);
    int i = mtn_clampi(z, 0, t->n - 1) * t->n + mtn_clampi(x, 0, t->n - 1);
    int b = t->biome[i];
    float nz = mtn_hash_unit(mtn_hash2i(x, z, 17)) * 0.12f;
    vec3 c;
    float veg;
    switch (b) {
    case MTN_BIOME_FOREST: c = v3(0.30f, 0.25f, 0.18f); veg = 0.82f; break;
    case MTN_BIOME_MEADOW: c = v3(0.40f, 0.36f, 0.26f); veg = 1.0f; break;
    case MTN_BIOME_ALPINE: c = v3(0.48f, 0.45f, 0.40f); veg = 0.72f; break;
    case MTN_BIOME_SCREE: c = v3(0.56f, 0.53f, 0.49f); veg = 0.12f; break;
    case MTN_BIOME_ROCK: c = v3(0.47f, 0.44f, 0.41f); veg = 0.0f; break;
    case MTN_BIOME_SNOW: c = v3(0.58f, 0.57f, 0.56f); veg = 0.05f; break;
    default: c = v3(0.33f, 0.30f, 0.25f); veg = 0.3f; break;
    }
    c = v3_scale(c, 0.94f + nz);
    /* ambient occlusion from the surrounding relief */
    float avg = 0.0f;
    for (int k = 0; k < 8; k++) {
        float a = (float)k * MTN_TAU / 8.0f;
        avg += mtn_terrain_grid(t, x + (int)lroundf(cosf(a) * 3.0f), z + (int)lroundf(sinf(a) * 3.0f));
    }
    avg /= 8.0f;
    float ao = mtn_clampf(1.0f - (avg - h) / 40.0f, 0.45f, 1.0f);
    float trail = 1.0f - mtn_smoothstep(1.5f, 7.0f, t->trail_dist[i]);
    memset(v, 0, sizeof *v);
    v->x = wx; v->y = h; v->z = wz;
    v->nx = n.x; v->ny = n.y; v->nz = n.z;
    v->r = u8f(c.x); v->g = u8f(c.y); v->b = u8f(c.z); v->a = u8f(veg);
    v->tile = u8f(trail * 0.7f);
    v->flags = u8f(t->moist[i]);
    v->extra = (uint8_t)b;
    v->ao = u8f(ao);
}

static void build_terrain_chunk(const mtn_terrain *t, int cx, int cz, int step, mtn_mesh *m) {
    mtn_mesh_clear(m);
    int x0 = cx * TCHUNK, z0 = cz * TCHUNK;
    int verts = TCHUNK / step + 1;
    for (int j = 0; j < verts; j++)
        for (int i = 0; i < verts; i++) {
            mtn_vertex v;
            terrain_vertex(t, x0 + i * step, z0 + j * step, &v);
            mtn_mesh_vertex(m, &v);
        }
    for (int j = 0; j < verts - 1; j++)
        for (int i = 0; i < verts - 1; i++) {
            int a = j * verts + i, b = a + 1, c = a + verts, d = c + 1;
            mtn_mesh_tri(m, a, c, b);
            mtn_mesh_tri(m, b, c, d);
        }
    /* skirts hide cracks between neighbouring chunks of different detail */
    int edges[4][2] = {{0, 1}, {verts * (verts - 1), 1}, {0, verts}, {verts - 1, verts}};
    for (int e = 0; e < 4; e++) {
        int start = edges[e][0], stride = edges[e][1];
        int base = m->vcount;
        for (int k = 0; k < verts; k++) {
            mtn_vertex v = m->v[start + k * stride];
            v.y -= 25.0f;
            mtn_mesh_vertex(m, &v);
        }
        for (int k = 0; k < verts - 1; k++) {
            int a = start + k * stride, b = start + (k + 1) * stride;
            mtn_mesh_tri(m, a, b, base + k);
            mtn_mesh_tri(m, b, base + k + 1, base + k);
        }
    }
}

static void build_water_chunk(const mtn_terrain *t, int cx, int cz, mtn_mesh *m) {
    mtn_mesh_clear(m);
    for (int z = cz * TCHUNK; z < (cz + 1) * TCHUNK && z < t->n - 1; z++)
        for (int x = cx * TCHUNK; x < (cx + 1) * TCHUNK && x < t->n - 1; x++) {
            float lvl = MTN_NO_WATER;
            for (int k = 0; k < 4; k++) {
                float w = t->water[(z + k / 2) * t->n + x + (k & 1)];
                if (w > lvl) lvl = w;
            }
            if (lvl == MTN_NO_WATER) continue;
            mtn_material mat = mtn_mat(0xFFFFFFFFu, 255, 0, 1.0f);
            float x0 = (float)x * t->cell, z0 = (float)z * t->cell, s = t->cell;
            mtn_mesh_quad(m, m4_identity(), v3(x0, lvl, z0 + s), v3(x0 + s, lvl, z0 + s), v3(x0 + s, lvl, z0), v3(x0, lvl, z0), &mat,
                          v2(0, 0), v2(1, 1));
        }
}

static void build_trail(const mtn_terrain *t, mtn_mesh *m) {
    mtn_mesh_clear(m);
    for (int i = 0; i + 1 < t->trail_len; i++) {
        vec3 a = t->trail[i], b = t->trail[i + 1];
        vec3 d = v3_norm(v3(b.x - a.x, 0, b.z - a.z));
        vec3 side = v3(-d.z, 0, d.x);
        float w = 0.75f + 0.2f * mtn_hash_unit(mtn_hash_u32((uint32_t)i));
        vec3 p[4] = {v3_add(a, v3_scale(side, -w)), v3_add(a, v3_scale(side, w)), v3_add(b, v3_scale(side, w)), v3_add(b, v3_scale(side, -w))};
        for (int k = 0; k < 4; k++) p[k].y = mtn_terrain_height(t, p[k].x, p[k].z) + 0.07f;
        int shade = 100 + (int)(mtn_hash_u32((uint32_t)i * 7u) % 30u);
        mtn_material mat = mtn_mat(mtn_rgb(shade + 25, shade, shade - 25), 255, MTN_VF_SNOW, 1.0f);
        mtn_mesh_quad(m, m4_identity(), p[1], p[0], p[3], p[2], &mat, v2(0, 0), v2(1, 1));
    }
}

static void build_cairns(const mtn_terrain *t, mtn_mesh *m) {
    mtn_mesh_clear(m);
    for (int c = 0; c < t->cairn_count; c++) {
        mtn_voxels vx;
        mtn_voxels_init(&vx, 12, 16, 12, 0.11f);
        vx.pal[1] = mtn_rgb(138, 134, 128);
        vx.pal[2] = mtn_rgb(112, 108, 102);
        vx.pal[3] = mtn_rgb(160, 156, 148);
        float y = 1.5f;
        for (int s = 0; s < 5; s++) {
            float rad = 4.6f - (float)s * 0.75f;
            mtn_voxels_blob(&vx, 6.0f + (float)((c + s) % 3) * 0.4f - 0.4f, y + rad * 0.5f, 6.0f, rad, (uint8_t)(1 + (s + c) % 3),
                            (uint32_t)(c * 10 + s));
            y += rad * 0.95f;
        }
        vec3 p = t->cairns[c];
        mtn_voxels_mesh(&vx, m, m4_translate(v3(p.x - 0.66f, p.y - 0.25f, p.z - 0.66f)), 255, MTN_VF_SNOW);
        mtn_voxels_free(&vx);
    }
    /* summit flag */
    vec3 s = t->summit;
    mtn_material pole = mtn_mat(mtn_rgb(110, 80, 50), 255, 0, 1.0f);
    mtn_mesh_cylinder(m, m4_identity(), v3(s.x - 1.0f, s.y - 0.3f, s.z), v3(s.x - 1.0f, s.y + 3.2f, s.z), 0.05f, 0.04f, 5, &pole, 1);
    mtn_material flag = mtn_mat(mtn_rgb(200, 40, 40), 255, MTN_VF_WIND, 1.0f);
    mtn_mesh_quad(m, m4_identity(), v3(s.x - 1.0f, s.y + 2.5f, s.z), v3(s.x + 0.2f, s.y + 2.6f, s.z), v3(s.x + 0.2f, s.y + 3.1f, s.z),
                  v3(s.x - 1.0f, s.y + 3.2f, s.z), &flag, v2(0, 0), v2(1, 1));
    mtn_mesh_quad(m, m4_identity(), v3(s.x + 0.2f, s.y + 2.6f, s.z), v3(s.x - 1.0f, s.y + 2.5f, s.z), v3(s.x - 1.0f, s.y + 3.2f, s.z),
                  v3(s.x + 0.2f, s.y + 3.1f, s.z), &flag, v2(0, 0), v2(1, 1));
}

static void sync_world(mtn_renderer *r, mtn_game *g) {
    if (r->world_version == g->world_version) return;
    free_world(r);
    r->world_version = g->world_version;
    const mtn_terrain *t = &g->terrain;
    r->tchunks = (t->n - 1) / TCHUNK;
    if (r->tchunks * r->tchunks > MAX_TCHUNKS) r->tchunks = 32;
    for (int cz = 0; cz < r->tchunks; cz++)
        for (int cx = 0; cx < r->tchunks; cx++) {
            int ci = cz * r->tchunks + cx;
            for (int l = 0; l < 3; l++) {
                build_terrain_chunk(t, cx, cz, 1 << l, &r->scratch_a);
                gmesh_upload(&r->terrain[l][ci], &r->scratch_a, 0);
            }
            build_water_chunk(t, cx, cz, &r->scratch_a);
            if (r->scratch_a.icount) gmesh_upload(&r->water[ci], &r->scratch_a, 0);
        }
    build_trail(t, &r->scratch_a);
    gmesh_upload(&r->trail, &r->scratch_a, 0);
    build_cairns(t, &r->scratch_a);
    gmesh_upload(&r->cairns, &r->scratch_a, 0);
    r->fchunks = g->flora.chunks * g->flora.chunks;
    r->flora = calloc((size_t)(r->fchunks > 0 ? r->fchunks : 1), sizeof(fchunk));
}

static void sync_house(mtn_renderer *r, mtn_game *g) {
    if (r->house_version != g->house_mesh_version) {
        r->house_version = g->house_mesh_version;
        gmesh_upload(&r->house_solid, &g->house_solid, 0);
        gmesh_upload(&r->house_sprites, &g->house_sprites, 0);
        gmesh_upload(&r->house_glow, &g->house_glow, 0);
        if (!r->door.vao) gmesh_upload(&r->door, &g->door_mesh, 0);
    }
    if (r->rooms_version != g->rooms_version) {
        r->rooms_version = g->rooms_version;
        for (int i = 0; i < 3; i++) {
            gmesh_upload(&r->room_solid[i], &g->room_solid[i], 0);
            gmesh_upload(&r->room_sprites[i], &g->room_sprites[i], 0);
            gmesh_upload(&r->room_glow[i], &g->room_glow[i], 0);
        }
    }
}

static int flora_lod_for(float d) { return d < 160.0f ? 0 : (d < 520.0f ? 1 : 2); }

static void update_flora(mtn_renderer *r, mtn_game *g, vec3 cam) {
    if (!r->flora) return;
    int budget = 6;
    int n = g->flora.chunks;
    /* nearest first: walk rings around the camera chunk */
    int cc = mtn_flora_chunk_of(&g->flora, cam.x, cam.z);
    int ccx = cc % n, ccz = cc / n;
    int maxr = (int)(1900.0f / MTN_FLORA_CHUNK) + 1;
    for (int ring = 0; ring <= maxr && budget > 0; ring++)
        for (int dz = -ring; dz <= ring && budget > 0; dz++)
            for (int dx = -ring; dx <= ring && budget > 0; dx++) {
                if (abs(dx) != ring && abs(dz) != ring) continue;
                int x = ccx + dx, z = ccz + dz;
                if (x < 0 || z < 0 || x >= n || z >= n) continue;
                int ci = z * n + x;
                fchunk *fc = &r->flora[ci];
                vec3 c = v3(((float)x + 0.5f) * MTN_FLORA_CHUNK, cam.y, ((float)z + 0.5f) * MTN_FLORA_CHUNK);
                int lod = flora_lod_for(v3_dist_xz(c, cam));
                if (fc->built && fc->lod == lod && fc->version == g->flora.chunk_version[ci]) continue;
                if (g->flora.chunk_count[ci] == 0) {
                    fc->built = 1;
                    fc->lod = lod;
                    fc->version = g->flora.chunk_version[ci];
                    continue;
                }
                mtn_mesh_clear(&r->scratch_a);
                mtn_mesh_clear(&r->scratch_b);
                mtn_flora_chunk_build(&g->flora, ci, lod, &r->scratch_a, &r->scratch_b);
                gmesh_upload(&fc->solid, &r->scratch_a, 1);
                gmesh_upload(&fc->leaves, &r->scratch_b, 1);
                fc->built = 1;
                fc->lod = lod;
                fc->version = g->flora.chunk_version[ci];
                budget--;
            }
}

static void update_ground(mtn_renderer *r, mtn_game *g, vec3 cam) {
    if (r->ground_valid && v3_dist_xz(cam, r->ground_center) < 6.0f) return;
    r->ground_center = cam;
    r->ground_valid = 1;
    static mtn_ground_item items[28];
    mtn_mesh_clear(&r->scratch_a);
    int c0x = (int)floorf(cam.x / 8.0f), c0z = (int)floorf(cam.z / 8.0f);
    for (int dz = -6; dz <= 6; dz++)
        for (int dx = -6; dx <= 6; dx++) {
            int n = mtn_flora_ground_cell(&g->flora, &g->terrain, c0x + dx, c0z + dz, items, 28);
            mtn_ground_build(&g->flora, items, n, &r->scratch_a);
        }
    gmesh_upload(&r->ground, &r->scratch_a, 1);
}

static void update_dynamic(mtn_renderer *r, mtn_game *g, vec3 cam) {
    mtn_mesh_clear(&r->scratch_a);
    mtn_mesh_clear(&r->scratch_b);
    if (!g->interior) mtn_fauna_build(&g->fauna, cam, 260.0f, &r->scratch_a, &r->scratch_b);
    gmesh_upload(&r->fauna_solid, &r->scratch_a, 1);
    gmesh_upload(&r->fauna_sprites, &r->scratch_b, 1);
    /* chimney smoke and summoning sparkles */
    mtn_mesh_clear(&r->scratch_a);
    const mtn_house *h = &g->house;
    if (mtn_house_exists(h)) {
        vec2 wd = g->weather.wind_dir;
        float wind = 0.5f + g->weather.wind * 2.0f;
        for (int i = 0; i < 14 && h->state == MTN_HOUSE_PRESENT; i++) {
            float age = mtn_fract(r->time * 0.12f + (float)i / 14.0f);
            vec3 p = v3_add(h->chimney_top, v3(wd.x * age * 6.0f * wind, age * 7.0f, wd.y * age * 6.0f * wind));
            int a = (int)((1.0f - age) * 150.0f);
            mtn_material m = mtn_mat(mtn_rgb(200, 200, 205) | ((uint32_t)a << 24), 43, 0, 1.0f);
            m.a = (uint8_t)a;
            mtn_mesh_billboard(&r->scratch_a, p, 0.8f + age * 2.5f, 0.8f + age * 2.5f, 0.0f, &m, 0.0f);
        }
        if (h->state == MTN_HOUSE_APPEARING || h->state == MTN_HOUSE_VANISHING) {
            mtn_rng rng;
            mtn_rng_seed(&rng, 7, 3);
            for (int i = 0; i < 60; i++) {
                float a = mtn_rng_range(&rng, 0, MTN_TAU), rad = mtn_rng_range(&rng, 1.0f, 6.0f);
                float y = mtn_fract(mtn_rng_float(&rng) + r->time * 0.5f) * 6.0f;
                vec3 p = v3_add(h->origin, v3(cosf(a + r->time) * rad, y, sinf(a + r->time) * rad));
                int s = (int)(255.0f * (1.0f - fabsf(h->anim - 0.5f) * 1.6f));
                mtn_material m = mtn_mat(mtn_rgb(s, s, s), 42, MTN_VF_EMISSIVE, 1.0f);
                mtn_mesh_billboard(&r->scratch_a, p, 0.35f, 0.35f, 0.0f, &m, 0.0f);
            }
        }
    }
    gmesh_upload(&r->fx, &r->scratch_a, 1);
}

/* ------------------------------------------------------------------ uniforms */

static void world_uniforms(mtn_renderer *r, prog *p, mtn_game *g) {
    const mtn_sky_state *s = &g->sky;
    um4(p, "u_viewproj", r->viewproj);
    um4(p, "u_model", m4_identity());
    u3v(p, "u_cam_pos", r->eye);
    mat4 inv = m4_inverse(r->view);
    u3v(p, "u_cam_right", v3_norm(m4_mul_dir(inv, v3(1, 0, 0))));
    u3v(p, "u_cam_up", v3_norm(m4_mul_dir(inv, v3(0, 1, 0))));
    u1f(p, "u_time", r->time);
    glUniform3f(uloc(p, "u_wind"), g->weather.wind_dir.x, g->weather.wind_dir.y, 0.3f + g->weather.wind * 1.4f);
    u3v(p, "u_light_dir", s->light_dir);
    u3v(p, "u_light_color", s->light_color);
    u3v(p, "u_ambient", s->ambient);
    u3v(p, "u_fog_color", s->fog_color);
    u1f(p, "u_fog_density", s->fog_density);
    float range = g->terrain.max_h - g->terrain.min_h;
    u1f(p, "u_snow_height", g->terrain.min_h + g->snowline * range);
    u3v(p, "u_snow_light", v3_add(s->ambient, v3_scale(s->light_color, 0.8f)));
    u1f(p, "u_emissive", 1.0f);
    u1i(p, "u_interior", r->space);
    u1f(p, "u_alpha_test", 0.5f);
    u1i(p, "u_atlas", 0);
    u1i(p, "u_single", 0);
    float sp[48 * 4], bl[48];
    memset(sp, 0, sizeof sp);
    memset(bl, 0, sizeof bl);
    for (int i = 0; i < g->flora.species_count && i < 48; i++) {
        sp[i * 4] = g->fseason.leaf_color[i].x;
        sp[i * 4 + 1] = g->fseason.leaf_color[i].y;
        sp[i * 4 + 2] = g->fseason.leaf_color[i].z;
        sp[i * 4 + 3] = g->fseason.leaf_amount[i];
        bl[i] = g->fseason.blossom[i];
    }
    glUniform4fv(uloc(p, "u_species"), 48, sp);
    glUniform1fv(uloc(p, "u_blossom"), 48, bl);
}

static void interior_lights(mtn_renderer *r, prog *p, mtn_game *g) {
    float pos[9 * 4], col[9 * 4];
    int n = 0;
    /* daylight through the big window of the current room */
    int cur = mtn_clampi(mtn_house_room_at(r->eye.z) - g->room_base, 0, 2);
    const mtn_room *cr = &g->rooms[cur];
    if (cr->length > 0) {
        vec3 wp = m4_mul_point(cr->window_frame, v3(0, 0, -1.5f));
        vec3 sky = v3_add(v3_scale(g->sky.ambient, 1.6f), v3_scale(g->sky.light_color, 0.35f));
        pos[0] = wp.x; pos[1] = wp.y; pos[2] = wp.z; pos[3] = 9.0f;
        col[0] = sky.x; col[1] = sky.y; col[2] = sky.z; col[3] = 0.0f;
        n = 1;
    }
    for (int k = 0; k < 3; k++) {
        int ri = (cur + k) % 3;
        const mtn_room *rm = &g->rooms[ri];
        for (int i = 0; i < rm->light_count && n < 9; i++) {
            const mtn_light *l = &rm->lights[i];
            pos[n * 4] = l->pos.x; pos[n * 4 + 1] = l->pos.y; pos[n * 4 + 2] = l->pos.z; pos[n * 4 + 3] = l->radius;
            col[n * 4] = l->color.x; col[n * 4 + 1] = l->color.y; col[n * 4 + 2] = l->color.z; col[n * 4 + 3] = l->flicker;
            n++;
        }
    }
    u1i(p, "u_light_count", n);
    if (n) {
        glUniform4fv(uloc(p, "u_lights_pos"), n, pos);
        glUniform4fv(uloc(p, "u_lights_col"), n, col);
    }
}

/* ------------------------------------------------------------------ scene passes */

static void set_camera(mtn_renderer *r, mat4 view, mat4 proj, vec3 eye) {
    r->view = view;
    r->proj = proj;
    r->viewproj = m4_mul(proj, view);
    r->eye = eye;
    r->frustum = mtn_frustum_from(r->viewproj);
}

static void bind_tex(GLuint t) {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, t);
}

static void draw_sky(mtn_renderer *r, mtn_game *g, int interior) {
    prog *p = &r->sh_sky;
    glUseProgram(p->id);
    mat4 vrot = r->view;
    vrot.m[12] = vrot.m[13] = vrot.m[14] = 0.0f;
    um4(p, "u_inv_viewproj", m4_inverse(m4_mul(r->proj, vrot)));
    const mtn_sky_state *s = &g->sky;
    if (interior) {
        vec3 dark = v3(0.05f, 0.035f, 0.025f);
        u3v(p, "u_zenith", dark);
        u3v(p, "u_horizon", dark);
        u3v(p, "u_fog_color", dark);
        u1f(p, "u_sun_vis", 0);
        u1f(p, "u_stars", 0);
        u1i(p, "u_moon_count", 0);
        u1f(p, "u_cloud_cover", 0);
        u1f(p, "u_sunset", 0);
    } else {
        u3v(p, "u_zenith", s->zenith);
        u3v(p, "u_horizon", s->horizon);
        u3v(p, "u_fog_color", s->fog_color);
        u3v(p, "u_sun_dir", s->sun_dir);
        u3v(p, "u_sun_color", g->planet.sun_color);
        u1f(p, "u_sun_size", g->planet.sun_size);
        u1f(p, "u_sun_vis", s->sun_visibility);
        u1f(p, "u_sunset", s->sunset);
        float md[16], mc[16];
        for (int i = 0; i < 4; i++) {
            md[i * 4] = s->moon_dir[i].x; md[i * 4 + 1] = s->moon_dir[i].y; md[i * 4 + 2] = s->moon_dir[i].z;
            md[i * 4 + 3] = i < g->planet.moon_count ? g->planet.moons[i].size : 0.03f;
            vec3 c = i < g->planet.moon_count ? g->planet.moons[i].color : v3(1, 1, 1);
            mc[i * 4] = c.x * (1.0f - s->cloud_cover * 0.7f); mc[i * 4 + 1] = c.y * (1.0f - s->cloud_cover * 0.7f);
            mc[i * 4 + 2] = c.z * (1.0f - s->cloud_cover * 0.7f); mc[i * 4 + 3] = 1.0f;
        }
        u1i(p, "u_moon_count", g->planet.moon_count);
        glUniform4fv(uloc(p, "u_moon_dir"), 4, md);
        glUniform4fv(uloc(p, "u_moon_col"), 4, mc);
        u1i(p, "u_moon_tex", 1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, r->tex_moon);
        glActiveTexture(GL_TEXTURE0);
        u1f(p, "u_stars", s->star_visibility);
        /* celestial sphere rotation around the pole */
        vec3 ax = s->star_axis;
        float a = s->star_angle, c = cosf(a), sn = sinf(a), t = 1.0f - c;
        mat4 rot = m4_identity();
        rot.m[0] = t * ax.x * ax.x + c; rot.m[4] = t * ax.x * ax.y - sn * ax.z; rot.m[8] = t * ax.x * ax.z + sn * ax.y;
        rot.m[1] = t * ax.x * ax.y + sn * ax.z; rot.m[5] = t * ax.y * ax.y + c; rot.m[9] = t * ax.y * ax.z - sn * ax.x;
        rot.m[2] = t * ax.x * ax.z - sn * ax.y; rot.m[6] = t * ax.y * ax.z + sn * ax.x; rot.m[10] = t * ax.z * ax.z + c;
        um4(p, "u_star_rot", rot);
        u1f(p, "u_cloud_cover", s->cloud_cover);
        u3v(p, "u_cloud_color", s->cloud_color);
        u1f(p, "u_time", r->time);
        u2f(p, "u_wind", g->weather.wind_dir.x * (1.0f + g->weather.wind * 3.0f), g->weather.wind_dir.y * (1.0f + g->weather.wind * 3.0f));
    }
    glDepthMask(GL_FALSE);
    glBindVertexArray(r->empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    r->stats.draw_calls++;
    glDepthMask(GL_TRUE);
}

static void draw_terrain(mtn_renderer *r, mtn_game *g) {
    prog *p = &r->sh_terrain;
    glUseProgram(p->id);
    um4(p, "u_viewproj", r->viewproj);
    u3v(p, "u_cam_pos", r->eye);
    u3v(p, "u_light_dir", g->sky.light_dir);
    u3v(p, "u_light_color", g->sky.light_color);
    u3v(p, "u_ambient", g->sky.ambient);
    u1f(p, "u_fog_density", g->sky.fog_density);
    u3v(p, "u_fog_color", g->sky.fog_color);
    /* seasonal grass: fresh in spring, lush in summer, golden in autumn, pale in winter */
    static const vec3 grass[4] = {{0.42f, 0.62f, 0.24f}, {0.33f, 0.52f, 0.20f}, {0.58f, 0.50f, 0.24f}, {0.50f, 0.50f, 0.40f}};
    static const vec3 dry[4] = {{0.52f, 0.55f, 0.30f}, {0.55f, 0.52f, 0.28f}, {0.60f, 0.45f, 0.25f}, {0.52f, 0.48f, 0.40f}};
    vec3 gc = v3(0, 0, 0), dc = v3(0, 0, 0);
    for (int k = 0; k < 4; k++) {
        gc = v3_add(gc, v3_scale(grass[k], g->season.w[k]));
        dc = v3_add(dc, v3_scale(dry[k], g->season.w[k]));
    }
    u3v(p, "u_grass_color", gc);
    u3v(p, "u_dry_grass", dc);
    float range = g->terrain.max_h - g->terrain.min_h;
    u1f(p, "u_snow_height", g->terrain.min_h + g->snowline * range);
    u1f(p, "u_fresh_snow", mtn_smoothstep(0.5f, 1.0f, g->weather.snow_ground));
    u3v(p, "u_snow_light", v3(1, 1, 1));
    u1i(p, "u_detail", 0);
    bind_tex(r->tex_terrain);
    glDisable(GL_CULL_FACE);
    for (int i = 0; i < r->tchunks * r->tchunks; i++) {
        const gmesh *m0 = &r->terrain[0][i];
        if (!visible(r, m0)) continue;
        vec3 c = v3_scale(v3_add(m0->mn, m0->mx), 0.5f);
        float d = v3_dist_xz(c, r->eye);
        int lod = d < 450.0f ? 0 : (d < 1300.0f ? 1 : 2);
        gmesh_draw(r, &r->terrain[lod][i]);
    }
    glEnable(GL_CULL_FACE);
}

static void draw_water(mtn_renderer *r, mtn_game *g) {
    prog *p = &r->sh_water;
    glUseProgram(p->id);
    um4(p, "u_viewproj", r->viewproj);
    u3v(p, "u_cam_pos", r->eye);
    u1f(p, "u_fog_density", g->sky.fog_density);
    u1f(p, "u_time", r->time);
    u3v(p, "u_zenith", g->sky.zenith);
    u3v(p, "u_horizon", g->sky.horizon);
    u3v(p, "u_sun_dir", g->sky.sun_dir);
    u3v(p, "u_light_color", g->sky.light_color);
    u3v(p, "u_ambient", g->sky.ambient);
    u3v(p, "u_fog_color", g->sky.fog_color);
    u1f(p, "u_snow", mtn_smoothstep(0.7f, 0.95f, g->season.w[MTN_WINTER]) * (g->snowline < 0.3f ? 1.0f : 0.6f));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    for (int i = 0; i < r->tchunks * r->tchunks; i++)
        if (visible(r, &r->water[i])) gmesh_draw(r, &r->water[i]);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

static void draw_world_meshes(mtn_renderer *r, mtn_game *g) {
    prog *p = &r->sh_world;
    glUseProgram(p->id);
    world_uniforms(r, p, g);
    /* trail and cairns: untextured */
    bind_tex(r->tex_materials);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.0f, -2.0f);
    gmesh_draw(r, &r->trail);
    glDisable(GL_POLYGON_OFFSET_FILL);
    gmesh_draw(r, &r->cairns);
    /* flora: trunks/boulders (materials atlas), then leaves (foliage atlas) */
    int n = 0;
    for (int i = 0; i < r->fchunks; i++) {
        fchunk *fc = &r->flora[i];
        if (!fc->built || fc->solid.count == 0) continue;
        if (v3_dist_xz(v3_scale(v3_add(fc->solid.mn, fc->solid.mx), 0.5f), r->eye) > 1900.0f) continue;
        if (visible(r, &fc->solid)) gmesh_draw(r, &fc->solid);
    }
    gmesh_draw(r, &r->fauna_solid);
    if (mtn_house_exists(&g->house)) {
        float s = mtn_smoothstep(0.0f, 1.0f, g->house.anim);
        mat4 grow = m4_mul(m4_translate(g->house.origin), m4_mul(m4_scale(v3(1, s, 1)), m4_translate(v3_scale(g->house.origin, -1))));
        um4(p, "u_model", grow);
        gmesh_draw(r, &r->house_solid);
        um4(p, "u_model", m4_mul(grow, mtn_house_door_model(&g->house, 0)));
        gmesh_draw(r, &r->door);
        um4(p, "u_model", m4_identity());
    }
    bind_tex(r->tex_foliage);
    glDisable(GL_CULL_FACE);
    for (int i = 0; i < r->fchunks; i++) {
        fchunk *fc = &r->flora[i];
        if (!fc->built || fc->leaves.count == 0) continue;
        if (v3_dist_xz(v3_scale(v3_add(fc->leaves.mn, fc->leaves.mx), 0.5f), r->eye) > 1900.0f) continue;
        if (visible(r, &fc->leaves)) {
            gmesh_draw(r, &fc->leaves);
            n++;
        }
    }
    gmesh_draw(r, &r->ground);
    if (mtn_house_exists(&g->house)) gmesh_draw(r, &r->house_sprites);
    glEnable(GL_CULL_FACE);
    r->stats.flora_chunks = n;
}

static void draw_blended_world(mtn_renderer *r, mtn_game *g) {
    prog *p = &r->sh_world;
    glUseProgram(p->id);
    world_uniforms(r, p, g);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    u1f(p, "u_alpha_test", 0.02f);
    if (mtn_house_exists(&g->house)) {
        bind_tex(r->tex_materials);
        u1f(p, "u_emissive", mtn_lerpf(1.6f, 0.55f, g->sky.daylight));
        float s = mtn_smoothstep(0.0f, 1.0f, g->house.anim);
        mat4 grow = m4_mul(m4_translate(g->house.origin), m4_mul(m4_scale(v3(1, s, 1)), m4_translate(v3_scale(g->house.origin, -1))));
        um4(p, "u_model", grow);
        gmesh_draw(r, &r->house_glow);
        um4(p, "u_model", m4_identity());
        u1f(p, "u_emissive", 1.0f);
    }
    bind_tex(r->tex_foliage);
    gmesh_draw(r, &r->fauna_sprites);
    u1f(p, "u_emissive", 1.3f);
    gmesh_draw(r, &r->fx);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);
}

static void draw_precip(mtn_renderer *r, mtn_game *g) {
    float amount = g->weather.precipitation;
    if (amount < 0.02f) return;
    prog *p = &r->sh_precip;
    glUseProgram(p->id);
    um4(p, "u_viewproj", r->viewproj);
    u3v(p, "u_cam_pos", r->eye);
    mat4 inv = m4_inverse(r->view);
    u3v(p, "u_cam_right", v3_norm(m4_mul_dir(inv, v3(1, 0, 0))));
    u1f(p, "u_time", r->time);
    u1f(p, "u_amount", amount);
    int snow = g->weather.precip_snow || g->temperature < 1.0f;
    u1i(p, "u_snow", snow);
    glUniform3f(uloc(p, "u_wind"), g->weather.wind_dir.x, g->weather.wind_dir.y, g->weather.wind);
    u3v(p, "u_light", v3_add(g->sky.ambient, v3_scale(g->sky.light_color, 0.5f)));
    u1i(p, "u_atlas", 0);
    bind_tex(r->tex_foliage);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    gmesh_draw(r, &r->precip);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

static void draw_photo_slots(mtn_renderer *r, mtn_game *g, prog *p);

static void draw_rooms(mtn_renderer *r, mtn_game *g, int blended) {
    prog *p = &r->sh_world;
    glUseProgram(p->id);
    world_uniforms(r, p, g);
    interior_lights(r, p, g);
    if (!blended) {
        bind_tex(r->tex_materials);
        for (int i = 0; i < 3; i++) gmesh_draw(r, &r->room_solid[i]);
        if (g->room_base == 0) {
            um4(p, "u_model", mtn_house_door_model(&g->house, 1));
            gmesh_draw(r, &r->door);
            um4(p, "u_model", m4_identity());
        }
        draw_photo_slots(r, g, p);
        bind_tex(r->tex_foliage);
        glDisable(GL_CULL_FACE);
        for (int i = 0; i < 3; i++) gmesh_draw(r, &r->room_sprites[i]);
        glEnable(GL_CULL_FACE);
    } else {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        u1f(p, "u_alpha_test", 0.02f);
        u1f(p, "u_emissive", 1.4f);
        bind_tex(r->tex_materials);
        for (int i = 0; i < 3; i++) gmesh_draw(r, &r->room_glow[i]);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}

/* ------------------------------------------------------------------ photos as textures */

static GLuint photo_texture(mtn_renderer *r, mtn_game *g, int index) {
    if (index < 0 || index >= g->gallery.count) return 0;
    const char *name = g->gallery.names[index];
    for (int i = 0; i < MAX_THUMBS; i++)
        if (r->thumbs[i] && strcmp(r->thumb_names[i], name) == 0) return r->thumbs[i];
    char path[700];
    snprintf(path, sizeof path, "%sphotos/%s", g->user_dir, name);
    size_t size;
    char *data = mtn_file_read(path, &size);
    if (!data) return 0;
    int w, h;
    uint8_t *px = mtn_png_decode((const uint8_t *)data, size, &w, &h);
    free(data);
    if (!px) return 0;
    /* photos are integer upscaled pixel art: shrink by an integer factor and keep it crisp */
    int f = (w + 479) / 480;
    if (f < 1) f = 1;
    int tw = w / f, th = h / f;
    uint8_t *small = mtn_photo_downscale(px, w, h, tw, th);
    mtn_image_free(px);
    int slot = r->thumb_next;
    r->thumb_next = (r->thumb_next + 1) % MAX_THUMBS;
    if (r->thumbs[slot]) glDeleteTextures(1, &r->thumbs[slot]);
    r->thumbs[slot] = make_texture(small, tw, th, 0);
    snprintf(r->thumb_names[slot], sizeof r->thumb_names[slot], "%s", name);
    free(small);
    return r->thumbs[slot];
}

void mtn_render_forget_photo(mtn_renderer *r, const char *name) {
    for (int i = 0; i < MAX_THUMBS; i++)
        if (r->thumbs[i] && strcmp(r->thumb_names[i], name) == 0) {
            glDeleteTextures(1, &r->thumbs[i]);
            r->thumbs[i] = 0;
            r->thumb_names[i][0] = 0;
        }
}

static void draw_photo_slots(mtn_renderer *r, mtn_game *g, prog *p) {
    for (int k = 0; k < 3; k++) {
        const mtn_room *rm = &g->rooms[k];
        for (int i = 0; i < rm->photo_count; i++) {
            const mtn_photo_slot *s = &rm->photos[i];
            GLuint t = photo_texture(r, g, s->photo);
            mtn_mesh_clear(&r->scratch_a);
            mtn_material m = mtn_mat(t ? 0xFFFFFFFFu : mtn_rgb(230, 222, 200), t ? 0 : 255, MTN_VF_ABS_UV, 1.0f);
            vec3 c = s->center, rt = v3_scale(s->right, 0.62f), up = v3_scale(s->up, 0.38f);
            vec3 nrm = v3_cross(s->right, s->up);
            vec3 off = v3_scale(nrm, 0.015f);
            mtn_mesh_quad(&r->scratch_a, m4_identity(), v3_add(v3_sub(v3_sub(c, rt), up), off), v3_add(v3_sub(v3_add(c, rt), up), off),
                          v3_add(v3_add(v3_add(c, rt), up), off), v3_add(v3_add(v3_sub(c, rt), up), off), &m, v2(0, 0), v2(1, 1));
            gmesh_upload(&r->photo_quad, &r->scratch_a, 1);
            if (t) {
                bind_tex(t);
                u1i(p, "u_single", 1);
            }
            gmesh_draw(r, &r->photo_quad);
            u1i(p, "u_single", 0);
            bind_tex(r->tex_materials);
        }
    }
}

/* ------------------------------------------------------------------ portals */

static vec4 plane_to_view(mat4 view, vec3 n, vec3 o) {
    vec4 pl = v4(n.x, n.y, n.z, -v3_dot(n, o));
    mat4 it = m4_transpose(m4_inverse(view));
    return m4_mul_v4(it, pl);
}

static void render_space(mtn_renderer *r, mtn_game *g, int space, int depth);

/* quad: 4 corners in the current space; M maps current space -> target space; target_frame is
 * the portal frame in target space (for the near clip plane) */
static void draw_portal(mtn_renderer *r, mtn_game *g, const vec3 q[4], mat4 m, mat4 target_frame, int target) {
    vec3 c = v3_scale(v3_add(v3_add(q[0], q[1]), v3_add(q[2], q[3])), 0.25f);
    float rad = v3_dist(c, q[0]);
    if (!mtn_frustum_sphere(&r->frustum, c, rad)) return;
    vec3 n = v3_norm(v3_cross(v3_sub(q[1], q[0]), v3_sub(q[2], q[0])));
    if (v3_dot(n, v3_sub(r->eye, c)) <= 0.0f) return; /* seen from behind */
    r->stats.portals++;
    mtn_mesh_clear(&r->scratch_a);
    mtn_material mat = mtn_mat(0xFFFFFFFFu, 255, 0, 1.0f);
    mtn_mesh_quad(&r->scratch_a, m4_identity(), q[0], q[1], q[2], q[3], &mat, v2(0, 0), v2(1, 1));
    gmesh_upload(&r->portal_quad, &r->scratch_a, 1);
    prog *p = &r->sh_portal;
    /* 1. mark the visible part of the opening in the stencil buffer */
    glUseProgram(p->id);
    um4(p, "u_viewproj", r->viewproj);
    u1i(p, "u_far", 0);
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xFF);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    gmesh_draw(r, &r->portal_quad);
    /* 2. push depth to the far plane inside the opening */
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_ALWAYS);
    u1i(p, "u_far", 1);
    gmesh_draw(r, &r->portal_quad);
    glDepthFunc(GL_LESS);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_CULL_FACE);
    /* 3. render the other space through the opening, clipped at the portal plane */
    mat4 sv = r->view, sp = r->proj;
    vec3 se = r->eye;
    int sspace = r->space;
    mat4 nv = m4_mul(r->view, m4_inverse(m));
    vec3 neye = m4_mul_point(m, r->eye);
    vec3 fo = v3(target_frame.m[12], target_frame.m[13], target_frame.m[14]);
    vec3 ff = v3(target_frame.m[8], target_frame.m[9], target_frame.m[10]);
    vec3 pn = v3_scale(ff, -1.0f);
    vec4 cp = plane_to_view(nv, pn, v3_add(fo, v3_scale(pn, -0.02f)));
    mat4 np = m4_oblique_near(r->proj, cp);
    set_camera(r, nv, np, neye);
    render_space(r, g, target, r->depth + 1);
    set_camera(r, sv, sp, se);
    r->space = sspace;
    /* 4. restore the opening's depth and clear the stencil */
    glUseProgram(p->id);
    um4(p, "u_viewproj", r->viewproj);
    u1i(p, "u_far", 0);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDisable(GL_CULL_FACE);
    glStencilFunc(GL_EQUAL, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
    glDepthFunc(GL_ALWAYS);
    gmesh_draw(r, &r->portal_quad);
    glDepthFunc(GL_LESS);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
}

static void door_quad_ext(const mtn_house *h, vec3 q[4]) {
    float dw = MTN_DOOR_WIDTH * 0.5f, z = h->depth * 0.5f - 0.15f;
    q[0] = m4_mul_point(h->frame, v3(-dw, 0, z));
    q[1] = m4_mul_point(h->frame, v3(dw, 0, z));
    q[2] = m4_mul_point(h->frame, v3(dw, MTN_DOOR_HEIGHT, z));
    q[3] = m4_mul_point(h->frame, v3(-dw, MTN_DOOR_HEIGHT, z));
}

static void render_space(mtn_renderer *r, mtn_game *g, int space, int depth) {
    r->space = space;
    r->depth = depth;
    mtn_house *h = &g->house;
    if (space == 0) {
        draw_sky(r, g, 0);
        draw_terrain(r, g);
        draw_world_meshes(r, g);
        draw_water(r, g);
        if (depth == 0 && h->state == MTN_HOUSE_PRESENT && h->door_angle > 0.02f) {
            vec3 q[4];
            door_quad_ext(h, q);
            draw_portal(r, g, q, mtn_house_ext_to_int(h), mtn_house_door_frame_int(), 1);
        }
        draw_blended_world(r, g);
        draw_precip(r, g);
    } else {
        draw_sky(r, g, 1);
        draw_rooms(r, g, 0);
        if (depth == 0) {
            for (int i = 0; i < 3; i++) {
                const mtn_room *rm = &g->rooms[i];
                if (rm->length <= 0) continue;
                vec3 hs = rm->window_size;
                mat4 f = rm->window_frame;
                vec3 q[4] = {m4_mul_point(f, v3(hs.x, -hs.y, 0)), m4_mul_point(f, v3(-hs.x, -hs.y, 0)),
                             m4_mul_point(f, v3(-hs.x, hs.y, 0)), m4_mul_point(f, v3(hs.x, hs.y, 0))};
                mat4 m = mtn_house_window_to_world(h, &g->terrain, rm);
                /* the target frame is where the window lands in the world */
                mat4 tf = m4_mul(m, f);
                tf = m4_mul(tf, m4_rotate_y(MTN_PI));
                draw_portal(r, g, q, m, tf, 0);
            }
            if (g->room_base == 0 && h->door_angle > 0.02f) {
                float dw = MTN_DOOR_WIDTH * 0.5f;
                vec3 q[4] = {v3(-dw, 0, 0), v3(dw, 0, 0), v3(dw, MTN_DOOR_HEIGHT, 0), v3(-dw, MTN_DOOR_HEIGHT, 0)};
                mat4 m = m4_inverse(mtn_house_ext_to_int(h));
                draw_portal(r, g, q, m, mtn_house_door_frame_ext(h), 0);
            }
        }
        r->space = 1;
        draw_rooms(r, g, 1);
    }
}

/* ------------------------------------------------------------------ frame */

static void ensure_targets(mtn_renderer *r, int lw, int lh) {
    if (r->fbo && r->lw == lw && r->lh == lh) return;
    if (r->fbo) {
        glDeleteFramebuffers(1, &r->fbo);
        glDeleteTextures(1, &r->color_tex);
        glDeleteRenderbuffers(1, &r->depth_rb);
        glDeleteFramebuffers(1, &r->cap_fbo);
        glDeleteTextures(1, &r->cap_tex);
    }
    r->lw = lw;
    r->lh = lh;
    r->color_tex = make_texture(NULL, lw, lh, 0);
    glBindTexture(GL_TEXTURE_2D, r->color_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenRenderbuffers(1, &r->depth_rb);
    glBindRenderbuffer(GL_RENDERBUFFER, r->depth_rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, lw, lh);
    glGenFramebuffers(1, &r->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, r->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r->color_tex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, r->depth_rb);
    r->cap_tex = make_texture(NULL, lw, lh, 0);
    glGenFramebuffers(1, &r->cap_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, r->cap_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r->cap_tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void draw_post(mtn_renderer *r, mtn_game *g, int overlays) {
    prog *p = &r->sh_post;
    glUseProgram(p->id);
    u1i(p, "u_scene", 0);
    u2f(p, "u_lowres", (float)r->lw, (float)r->lh);
    u1i(p, "u_filter", overlays && g->camera_mode ? g->photo_filter : 0);
    u1f(p, "u_flash", overlays ? g->photo_flash : 0.0f);
    u1f(p, "u_viewfinder", overlays && g->camera_mode ? 1.0f : 0.0f);
    u1f(p, "u_levels", 31.0f);
    bind_tex(r->color_tex);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(r->empty_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glEnable(GL_DEPTH_TEST);
}

static void ui_push(mtn_vertex *v, float x, float y, float u, float vv, uint32_t c) {
    memset(v, 0, sizeof *v);
    v->x = x; v->y = y;
    v->u = u; v->v = vv;
    v->r = (uint8_t)(c & 255); v->g = (uint8_t)((c >> 8) & 255); v->b = (uint8_t)((c >> 16) & 255); v->a = (uint8_t)(c >> 24);
}

static GLuint ui_texture(mtn_renderer *r, mtn_game *g, int tex) {
    if (tex == MTN_UI_TEX_FONT) return r->tex_font;
    if (tex == MTN_UI_TEX_ICONS) return r->tex_icons;
    if (tex >= MTN_UI_TEX_PHOTO) {
        GLuint t = photo_texture(r, g, tex - MTN_UI_TEX_PHOTO);
        return t ? t : r->tex_white;
    }
    return r->tex_white;
}

static void touch_overlay(mtn_game *g) {
    mtn_ui *ui = &g->ui;
    const mtn_touch *t = &g->touch;
    float s = 1.0f / ui->scale;
    if (g->ui_modal) return;
    float sr = t->stick_r * s;
    mtn_ui_icon(ui, t->stick_cx * s - sr, t->stick_cy * s - sr, sr * 2.0f, 30, 0x90FFFFFFu);
    float kx = (t->stick_cx + t->stick_x * t->stick_r) * s, ky = (t->stick_cy + t->stick_y * t->stick_r) * s;
    mtn_ui_icon(ui, kx - sr * 0.45f, ky - sr * 0.45f, sr * 0.9f, 31, 0xB0FFFFFFu);
    for (int i = 0; i < t->button_count; i++) {
        const mtn_touch_button *b = &t->buttons[i];
        float r = b->r * s;
        uint32_t col = b->finger >= 0 ? 0xE0FFFFFFu : 0x90FFFFFFu;
        mtn_ui_icon(ui, b->x * s - r, b->y * s - r, r * 2.0f, 30, col);
        mtn_ui_icon(ui, b->x * s - r * 0.55f, b->y * s - r * 0.55f, r * 1.1f, b->icon, col);
    }
}

static void draw_ui(mtn_renderer *r, mtn_game *g, int win_w, int win_h) {
    if (g->input.mode == MTN_MODE_TOUCH) touch_overlay(g);
    mtn_ui *ui = &g->ui;
    if (ui->count == 0) return;
    int nv = ui->count * 6;
    mtn_vertex *verts = malloc(sizeof(mtn_vertex) * (size_t)nv);
    float s = ui->scale;
    for (int i = 0; i < ui->count; i++) {
        const mtn_ui_quad *q = &ui->q[i];
        float x0 = q->x * s, y0 = q->y * s, x1 = (q->x + q->w) * s, y1 = (q->y + q->h) * s;
        mtn_vertex *v = verts + i * 6;
        ui_push(&v[0], x0, y0, q->u0, q->v0, q->color);
        ui_push(&v[1], x1, y0, q->u1, q->v0, q->color);
        ui_push(&v[2], x1, y1, q->u1, q->v1, q->color);
        ui_push(&v[3], x0, y0, q->u0, q->v0, q->color);
        ui_push(&v[4], x1, y1, q->u1, q->v1, q->color);
        ui_push(&v[5], x0, y1, q->u0, q->v1, q->color);
    }
    glBindVertexArray(r->ui_vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->ui_vbo);
    if (nv > r->ui_cap) {
        r->ui_cap = nv * 2;
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(sizeof(mtn_vertex) * (size_t)r->ui_cap), NULL, GL_DYNAMIC_DRAW);
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(sizeof(mtn_vertex) * (size_t)nv), verts);
    free(verts);
    prog *p = &r->sh_ui;
    glUseProgram(p->id);
    u2f(p, "u_screen", (float)win_w, (float)win_h);
    u1i(p, "u_tex", 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    int start = 0;
    while (start < ui->count) {
        int tex = ui->q[start].tex, end = start;
        while (end < ui->count && ui->q[end].tex == tex) end++;
        bind_tex(ui_texture(r, g, tex));
        glDrawArrays(GL_TRIANGLES, start * 6, (end - start) * 6);
        r->stats.draw_calls++;
        start = end;
    }
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glBindVertexArray(0);
}

static void prepare(mtn_renderer *r, mtn_game *g, vec3 world_eye) {
    r->g = g;
    r->time = (float)g->real_time;
    memset(&r->stats, 0, sizeof r->stats);
    sync_world(r, g);
    sync_house(r, g);
    if (g->has_world) {
        update_flora(r, g, world_eye);
        update_ground(r, g, world_eye);
        update_dynamic(r, g, world_eye);
    }
}

static vec3 world_eye_of(mtn_game *g) {
    if (!g->interior) return mtn_player_eye(&g->player);
    return m4_mul_point(m4_inverse(mtn_house_ext_to_int(&g->house)), mtn_player_eye(&g->player));
}

static void render_scene(mtn_renderer *r, mtn_game *g, mat4 view, mat4 proj, vec3 eye, int lw, int lh) {
    ensure_targets(r, lw, lh);
    glBindFramebuffer(GL_FRAMEBUFFER, r->fbo);
    glViewport(0, 0, lw, lh);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glStencilMask(0xFF);
    glClearColor(0.1f, 0.12f, 0.15f, 1.0f);
    glClearStencil(0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    r->stats.lw = lw;
    r->stats.lh = lh;
    if (!g->has_world) return;
    set_camera(r, view, proj, eye);
    render_space(r, g, g->interior ? 1 : 0, 0);
}

void mtn_render_frame(mtn_renderer *r, mtn_game *g, int win_w, int win_h) {
    if (win_w < 1) win_w = 1;
    if (win_h < 1) win_h = 1;
    prepare(r, g, world_eye_of(g));
    int lh = mtn_clampi(g->settings.pixel_lines, 120, 720);
    int lw = mtn_clampi((int)lroundf((float)lh * (float)win_w / (float)win_h), 64, 2048);
    float aspect = (float)win_w / (float)win_h;
    mat4 proj = m4_perspective(mtn_game_current_fov(g) * MTN_DEG2RAD, aspect, 0.08f, 5000.0f);
    render_scene(r, g, mtn_player_view(&g->player), proj, mtn_player_eye(&g->player), lw, lh);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, win_w, win_h);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw_post(r, g, 1);
    draw_ui(r, g, win_w, win_h);
}

void mtn_render_view(mtn_renderer *r, mtn_game *g, mat4 view, mat4 proj, vec3 eye, unsigned fbo, int x, int y, int w, int h,
                     int with_ui) {
    prepare(r, g, world_eye_of(g));
    int lh = mtn_clampi(h / 3, 120, 720);
    int lw = mtn_clampi(w / 3, 64, 2048);
    render_scene(r, g, view, proj, eye, lw, lh);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)fbo);
    glViewport(x, y, w, h);
    draw_post(r, g, 1);
    if (with_ui) draw_ui(r, g, w, h);
}

uint8_t *mtn_render_capture(mtn_renderer *r, mtn_game *g, int *w, int *h) {
    if (!r->cap_fbo) return NULL;
    glBindFramebuffer(GL_FRAMEBUFFER, r->cap_fbo);
    glViewport(0, 0, r->lw, r->lh);
    draw_post(r, g, 0);
    uint8_t *px = malloc((size_t)r->lw * (size_t)r->lh * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, r->lw, r->lh, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    mtn_photo_flip_y(px, r->lw, r->lh);
    *w = r->lw;
    *h = r->lh;
    return px;
}

uint8_t *mtn_render_read_window(int w, int h) {
    uint8_t *px = malloc((size_t)w * (size_t)h * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    mtn_photo_flip_y(px, w, h);
    return px;
}
