/* Mountain engine - the magic house: site analysis, exterior variants, door and portals. MIT License. */
#include "mtn/house.h"
#include "mtn/rng.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define NO_TEXTURE 255
#define WHITE 0xFFFFFFFFu
#define PORCH_DEPTH 1.6f
#define PORCH_HALF 1.6f
#define STAIR_GRADE MTN_STAIR_GRADE
#define HOUSE_W 6.0f
#define HOUSE_D 5.0f
#define DECK 1.2f
#define PLANE_INSET 0.15f

void mtn_house_init(mtn_house *h) {
    memset(h, 0, sizeof *h);
    h->width = HOUSE_W;
    h->depth = HOUSE_D;
    h->wall_h = 2.7f;
}

void mtn_room_type_defaults(mtn_room_type *t) {
    memset(t, 0, sizeof *t);
    strcpy(t->name, "room");
    t->width = 8.0f;
    t->height = 3.6f;
    t->wall_tile = 3;
    t->floor_tile = 2;
    t->wall_color = WHITE;
    t->floor_color = WHITE;
    t->light_color = v3(1.0f, 0.82f, 0.6f);
    t->weight = 1.0f;
}

int mtn_house_add_room_type(mtn_house *h, const mtn_room_type *t) {
    if (h->type_count >= MTN_MAX_ROOM_TYPES) return -1;
    h->types[h->type_count] = *t;
    return h->type_count++;
}

void mtn_room_item_defaults(mtn_room_item *it) {
    memset(it, 0, sizeof *it);
    it->wall = MTN_WALL_NONE;
    it->inset = 0.5f;
    it->scale = 1.0f;
}

int mtn_room_add_item_ex(mtn_room_type *t, const mtn_room_item *src) {
    if (t->item_count >= MTN_MAX_ROOM_ITEMS) return -1;
    mtn_room_item *it = &t->items[t->item_count];
    *it = *src;
    it->prefab[sizeof it->prefab - 1] = 0;
    if (it->scale <= 0.0f) it->scale = 1.0f;
    if (it->wall < MTN_WALL_NONE || it->wall > MTN_WALL_LEFT) it->wall = MTN_WALL_NONE;
    return t->item_count++;
}

int mtn_room_add_item(mtn_room_type *t, const char *prefab, float x, float z, float rot, float scale, int arg) {
    mtn_room_item it;
    mtn_room_item_defaults(&it);
    snprintf(it.prefab, sizeof it.prefab, "%s", prefab);
    it.x = x;
    it.z = z;
    it.rot = rot;
    it.scale = scale;
    it.arg = arg;
    return mtn_room_add_item_ex(t, &it);
}

const char *mtn_house_variant_name(int v) {
    static const char *n[MTN_HV_COUNT] = {"cottage", "hillside", "tree", "stilts"};
    return (v >= 0 && v < MTN_HV_COUNT) ? n[v] : "unknown";
}

/* ------------------------------------------------------------------ site */

static vec3 flat_dir(vec3 d, vec3 fallback) {
    vec3 r = v3_norm(v3(d.x, 0, d.z));
    return v3_len(r) > 0.5f ? r : fallback;
}

/* ------------------------------------------------------------------ stairs */

static int has_platform(int variant) { return variant == MTN_HV_TREE || variant == MTN_HV_STILTS; }

/* outward axis of a stair side in house-local coordinates */
static vec3 stair_axis(int side) {
    static const float ax[4][2] = {{0, 1}, {0, -1}, {1, 0}, {-1, 0}};
    return v3(ax[side & 3][0], 0, ax[side & 3][1]);
}

static float stair_start_at(int variant, int side) {
    if (!has_platform(variant)) return HOUSE_D * 0.5f + PORCH_DEPTH;
    return (side < 2 ? HOUSE_D * 0.5f : HOUSE_W * 0.5f) + DECK;
}

/* Walk down the stairs from the porch or platform edge until the steps meet the ground;
 * 1 when they end on dry ground within the longest allowed run. */
static int stairs_reach(const mtn_terrain *t, vec3 origin, vec3 right, vec3 front, float floor_y, int variant, int side,
                        float *len) {
    vec3 ax = stair_axis(side);
    float s0 = stair_start_at(variant, side);
    for (float d = 0.0f; d <= MTN_STAIR_MAX_RUN; d += 0.1f) {
        float u = s0 + d;
        vec3 w = v3_add(origin, v3_add(v3_scale(right, ax.x * u), v3_scale(front, ax.z * u)));
        float g = mtn_terrain_height(t, w.x, w.z);
        if (floor_y - d * STAIR_GRADE > g + MTN_STAIR_LAST_STEP) continue;
        float wl = mtn_terrain_water(t, w.x, w.z);
        if (wl != MTN_NO_WATER && wl - g > 0.2f) return 0; /* would end in the water */
        *len = d;
        return 1;
    }
    return 0;
}

int mtn_house_fit_stairs(const mtn_terrain *t, mtn_house_site *s) {
    vec3 front = flat_dir(s->front, v3(0, 0, 1));
    vec3 right = v3_cross(v3(0, 1, 0), front);
    int sides = has_platform(s->variant) ? 4 : 1;
    float best = 1e30f;
    int found = 0;
    for (int side = 0; side < sides; side++) {
        float len;
        if (!stairs_reach(t, s->pos, right, front, s->pos.y, s->variant, side, &len)) continue;
        float score = len + (side == 0 ? 0.0f : 2.0f); /* the front is nicest */
        if (score < best) {
            best = score;
            s->stair_side = side;
            s->stair_len = len;
            found = 1;
        }
    }
    return found;
}

static int evaluate_shape(const mtn_terrain *t, const mtn_flora *f, vec3 c, vec3 player, vec3 fwd, mtn_house_site *s) {
    memset(s, 0, sizeof *s);
    s->tree = -1;
    vec3 front = flat_dir(v3_sub(player, c), v3_scale(fwd, -1.0f));
    vec3 right = v3_cross(v3(0, 1, 0), front);
    if (!mtn_terrain_in_playable(t, c.x, c.z)) {
        snprintf(s->reason, sizeof s->reason, "the edge of the world is too close");
        return 0;
    }
    int tree = f ? mtn_flora_nearest_tree(f, c, 4.5f, 10.0f) : -1;
    if (tree >= 0) {
        const mtn_plant *p = &f->plants[tree];
        s->ok = 1;
        s->variant = MTN_HV_TREE;
        s->tree = tree;
        s->front = front;
        s->ground_min = s->ground_max = p->pos.y;
        s->pos = v3(p->pos.x, p->pos.y + 3.6f, p->pos.z);
        return 1;
    }
    float hmin = 1e30f, hmax = -1e30f;
    for (int iz = -2; iz <= 2; iz++)
        for (int ix = -2; ix <= 2; ix++) {
            vec3 q = v3_add(c, v3_add(v3_scale(right, (float)ix * 1.8f), v3_scale(front, (float)iz * 1.6f)));
            if (mtn_terrain_water(t, q.x, q.z) != MTN_NO_WATER) {
                snprintf(s->reason, sizeof s->reason, "the ground here is under water");
                return 0;
            }
            float hq = mtn_terrain_height(t, q.x, q.z);
            hmin = mtn_minf(hmin, hq);
            hmax = mtn_maxf(hmax, hq);
        }
    float hr = hmax - hmin;
    s->ground_min = hmin;
    s->ground_max = hmax;
    if (hr > 9.0f) {
        snprintf(s->reason, sizeof s->reason, "the slope is too steep");
        return 0;
    }
    s->ok = 1;
    s->front = front;
    if (hr > 2.4f) {
        vec3 n = mtn_terrain_normal(t, c.x, c.z);
        s->variant = MTN_HV_HILLSIDE;
        s->front = flat_dir(n, front);
        vec3 door = v3_add(c, v3_scale(s->front, 2.5f));
        s->pos = v3(c.x, mtn_terrain_height(t, door.x, door.z) + 0.2f, c.z);
        return 1;
    }
    for (int k = 0; k < 12; k++) {
        float a = (float)k * MTN_TAU / 12.0f;
        vec3 q = v3(c.x + cosf(a) * 11.0f, 0, c.z + sinf(a) * 11.0f);
        float w = mtn_terrain_water(t, q.x, q.z);
        if (w != MTN_NO_WATER) {
            s->variant = MTN_HV_STILTS;
            s->water_level = w;
            s->front = flat_dir(v3_sub(q, c), front);
            s->pos = v3(c.x, mtn_maxf(hmax, w) + 0.9f, c.z);
            return 1;
        }
    }
    s->variant = MTN_HV_COTTAGE;
    s->pos = v3(c.x, hmax + 0.25f, c.z);
    return 1;
}

static int evaluate_site(const mtn_terrain *t, const mtn_flora *f, vec3 c, vec3 player, vec3 fwd, mtn_house_site *s) {
    if (!evaluate_shape(t, f, c, player, fwd, s)) return 0;
    if (!mtn_house_fit_stairs(t, s)) {
        s->ok = 0;
        snprintf(s->reason, sizeof s->reason, "no way down to the ground from here");
        return 0;
    }
    return 1;
}

int mtn_house_find_site(const mtn_terrain *t, const mtn_flora *f, vec3 player, float yaw, mtn_house_site *out) {
    vec3 fwd = v3(sinf(yaw), 0, -cosf(yaw));
    vec3 target = v3_add(player, v3_scale(fwd, 8.0f));
    float best = 1e30f;
    int found = 0;
    mtn_house_site cand;
    memset(out, 0, sizeof *out);
    snprintf(out->reason, sizeof out->reason, "no place to settle here");
    static const float rings[4] = {0.0f, 3.0f, 6.0f, 9.0f};
    for (int ri = 0; ri < 4; ri++) {
        int n = ri == 0 ? 1 : 8;
        for (int k = 0; k < n; k++) {
            float a = (float)k * MTN_TAU / (float)n;
            vec3 c = v3(target.x + cosf(a) * rings[ri], 0, target.z + sinf(a) * rings[ri]);
            c.y = mtn_terrain_height(t, c.x, c.z);
            if (v3_dist_xz(c, player) < 5.5f) continue; /* never on top of the player */
            if (!evaluate_site(t, f, c, player, fwd, &cand)) {
                if (!found) memcpy(out->reason, cand.reason, sizeof out->reason);
                continue;
            }
            float score = v3_dist_xz(c, target) + (cand.ground_max - cand.ground_min) * 1.5f + cand.stair_len * 0.3f;
            if (cand.variant == MTN_HV_TREE) score -= 4.0f;
            if (score < best) {
                best = score;
                *out = cand;
                found = 1;
            }
        }
    }
    return found;
}

static void update_frame(mtn_house *h) {
    h->frame = m4_from_basis(h->origin, h->right, v3(0, 1, 0), h->front);
    h->inv_frame = m4_inverse(h->frame);
}

void mtn_house_summon(mtn_house *h, const mtn_house_site *s, uint32_t seed) {
    h->state = MTN_HOUSE_APPEARING;
    h->anim = 0.0f;
    h->variant = s->variant;
    h->seed = seed;
    h->origin = s->pos;
    h->floor_y = s->pos.y;
    h->front = flat_dir(s->front, v3(0, 0, 1));
    h->right = v3_cross(v3(0, 1, 0), h->front);
    h->ground_min = s->ground_min;
    h->water_level = s->water_level;
    h->tree = s->tree;
    h->stair_side = s->stair_side & 3;
    h->stair_len = s->stair_len;
    if (h->stair_len <= 0.0f && h->floor_y - h->ground_min > 0.3f) h->stair_len = (h->floor_y - h->ground_min) / STAIR_GRADE;
    if (!has_platform(h->variant)) h->stair_side = 0;
    h->door_angle = 0.0f;
    h->door_open = 0;
    h->inside = 0;
    h->room = 0;
    h->version++;
    update_frame(h);
    h->chimney_top = m4_mul_point(h->frame, v3(h->width * 0.3f, h->wall_h + 2.9f, -h->depth * 0.25f));
}

void mtn_house_unsummon(mtn_house *h) {
    if (h->state == MTN_HOUSE_PRESENT || h->state == MTN_HOUSE_APPEARING) {
        h->state = MTN_HOUSE_VANISHING;
        h->door_open = 0;
    }
}

int mtn_house_exists(const mtn_house *h) { return h->state != MTN_HOUSE_NONE; }

void mtn_house_update(mtn_house *h, float dt) {
    if (h->state == MTN_HOUSE_APPEARING) {
        h->anim += dt / 1.6f;
        if (h->anim >= 1.0f) {
            h->anim = 1.0f;
            h->state = MTN_HOUSE_PRESENT;
        }
    } else if (h->state == MTN_HOUSE_VANISHING) {
        h->anim -= dt / 1.2f;
        if (h->anim <= 0.0f) {
            h->anim = 0.0f;
            h->state = MTN_HOUSE_NONE;
            h->version++;
        }
    }
    float target = h->door_open ? 1.75f : 0.0f;
    float sp = 2.2f * dt;
    if (h->door_angle < target) h->door_angle = mtn_minf(target, h->door_angle + sp);
    else h->door_angle = mtn_maxf(target, h->door_angle - sp);
}

int mtn_house_can_unsummon(const mtn_house *h, vec3 player) {
    return h->state == MTN_HOUSE_PRESENT && !h->inside && v3_dist_xz(player, h->origin) <= MTN_HOUSE_UNSUMMON_DIST;
}

void mtn_house_toggle_door(mtn_house *h) {
    if (h->state == MTN_HOUSE_PRESENT) h->door_open = !h->door_open;
}

vec3 mtn_house_door_world(const mtn_house *h) {
    return m4_mul_point(h->frame, v3(0, 0, h->depth * 0.5f));
}

float mtn_house_door_distance(const mtn_house *h, vec3 p) {
    if (!mtn_house_exists(h)) return 1e30f;
    vec3 d = mtn_house_door_world(h);
    return sqrtf((p.x - d.x) * (p.x - d.x) + (p.z - d.z) * (p.z - d.z) + 0.25f * (p.y - d.y) * (p.y - d.y));
}

/* ------------------------------------------------------------------ surfaces & collision */

static float stair_start(const mtn_house *h) { return stair_start_at(h->variant, h->stair_side); }

/* along (u) and across (v) the stairs, in house-local coordinates */
static void stair_coords(const mtn_house *h, vec3 l, float *u, float *v) {
    vec3 ax = stair_axis(h->stair_side);
    *u = l.x * ax.x + l.z * ax.z;
    *v = l.x * ax.z - l.z * ax.x;
}

vec3 mtn_house_stair_foot(const mtn_house *h) {
    vec3 ax = stair_axis(h->stair_side);
    float u = stair_start(h) + h->stair_len;
    vec3 w = m4_mul_point(h->frame, v3(ax.x * u, 0, ax.z * u));
    w.y = h->floor_y - h->stair_len * STAIR_GRADE;
    return w;
}

int mtn_house_surface(const mtn_house *h, float x, float z, float *height) {
    if (h->state != MTN_HOUSE_PRESENT && h->state != MTN_HOUSE_APPEARING) return 0;
    vec3 l = m4_mul_point(h->inv_frame, v3(x, h->floor_y, z));
    float hw = h->width * 0.5f, hd = h->depth * 0.5f;
    float best = -1e30f;
    int any = 0;
    /* porch / platform / deck */
    float px0 = -PORCH_HALF, px1 = PORCH_HALF, pz0 = hd, pz1 = hd + PORCH_DEPTH;
    if (h->variant == MTN_HV_TREE || h->variant == MTN_HV_STILTS) {
        px0 = -hw - 1.2f; px1 = hw + 1.2f; pz0 = -hd - 1.2f; pz1 = hd + 1.2f;
    }
    if (l.x >= px0 && l.x <= px1 && l.z >= pz0 && l.z <= pz1) {
        best = h->floor_y;
        any = 1;
    }
    /* stairs down to the ground */
    float s0 = stair_start(h), u, v;
    stair_coords(h, l, &u, &v);
    if (h->stair_len > 0.0f && fabsf(v) <= 0.8f && u >= s0 && u <= s0 + h->stair_len) {
        float sh = h->floor_y - (u - s0) * STAIR_GRADE;
        if (sh > best) best = sh;
        any = 1;
    }
    if (any) *height = best;
    return any;
}

int mtn_house_collide(const mtn_house *h, vec3 *pos, float radius) {
    if (h->state != MTN_HOUSE_PRESENT && h->state != MTN_HOUSE_APPEARING) return 0;
    if (pos->y < h->floor_y - 1.6f || pos->y > h->floor_y + h->wall_h + 2.5f) return 0;
    vec3 l = m4_mul_point(h->inv_frame, *pos);
    float hw = h->width * 0.5f + radius, hd = h->depth * 0.5f + radius;
    if (l.x <= -hw || l.x >= hw || l.z <= -hd || l.z >= hd) return 0;
    if (h->door_open && h->door_angle > 0.8f && fabsf(l.x) < MTN_DOOR_WIDTH * 0.5f - 0.05f) return 0;
    float px = hw - fabsf(l.x), pz = hd - fabsf(l.z);
    if (px < pz) l.x = l.x < 0 ? -hw : hw;
    else l.z = l.z < 0 ? -hd : hd;
    vec3 w = m4_mul_point(h->frame, l);
    pos->x = w.x;
    pos->z = w.z;
    return 1;
}

int mtn_house_crossed_in(const mtn_house *h, vec3 prev, vec3 cur) {
    if (h->state != MTN_HOUSE_PRESENT || !h->door_open || h->door_angle < 0.8f) return 0;
    vec3 a = m4_mul_point(h->inv_frame, prev), b = m4_mul_point(h->inv_frame, cur);
    float plane = h->depth * 0.5f - PLANE_INSET;
    if (!(a.z >= plane && b.z < plane)) return 0;
    if (fabsf(b.x) > MTN_DOOR_WIDTH * 0.5f) return 0;
    return b.y > -1.0f && b.y < MTN_DOOR_HEIGHT;
}

/* ------------------------------------------------------------------ portals */

mat4 mtn_house_door_frame_ext(const mtn_house *h) {
    vec3 o = m4_mul_point(h->frame, v3(0, 0, h->depth * 0.5f - PLANE_INSET));
    return mtn_portal_frame(o, v3_scale(h->front, -1.0f));
}

mat4 mtn_house_door_frame_int(void) { return mtn_portal_frame(v3(0, 0, 0), v3(0, 0, -1)); }

mat4 mtn_house_ext_to_int(const mtn_house *h) {
    return mtn_portal_transform(mtn_house_door_frame_ext(h), mtn_house_door_frame_int());
}

mat4 mtn_house_window_to_world(const mtn_house *h, const mtn_terrain *t, const mtn_room *r) {
    /* the cabin's window is in the front wall beside the door: it maps like the door */
    if (r->view < 0 || !t || t->vp_count == 0) return m4_inverse(mtn_house_ext_to_int(h));
    const mtn_viewpoint *vp = &t->vps[r->view % t->vp_count];
    vec3 dir = v3(sinf(vp->yaw), 0, -cosf(vp->yaw));
    float lift = 0.2f + (r->balcony ? MTN_BALCONY_LIFT : 0.0f);
    mat4 target = mtn_portal_frame(v3_add(vp->pos, v3(0, lift, 0)), v3_scale(dir, -1.0f));
    return mtn_portal_transform(r->window_frame, target);
}

/* ------------------------------------------------------------------ exterior meshes */

typedef struct {
    mtn_mesh *m;
    mat4 xf;
} bctx;

static void box(bctx *b, vec3 mn, vec3 mx, uint32_t color, int tile, int flags, float uvs) {
    mtn_material mat = mtn_mat(color, tile, flags, uvs);
    mtn_mesh_box(b->m, b->xf, mn, mx, &mat, 63);
}

static void cyl(bctx *b, vec3 a, vec3 c, float ra, float rb, int sides, uint32_t color, int tile, float uvs) {
    mtn_material mat = mtn_mat(color, tile, 0, uvs);
    mtn_mesh_cylinder(b->m, b->xf, a, c, ra, rb, sides, &mat, 1);
}

static void quad(bctx *b, vec3 p0, vec3 p1, vec3 p2, vec3 p3, uint32_t color, int tile, int flags, vec2 uv1) {
    mtn_material mat = mtn_mat(color, tile, flags, 1.0f);
    mtn_mesh_quad(b->m, b->xf, p0, p1, p2, p3, &mat, v2(0, 0), uv1);
}

static uint32_t stone_color(const mtn_terrain *t, vec3 p) {
    mtn_biome bi = t ? mtn_terrain_biome(t, p.x, p.z) : MTN_BIOME_FOREST;
    if (bi == MTN_BIOME_ROCK || bi == MTN_BIOME_SCREE) return mtn_rgb(200, 196, 190);
    if (bi == MTN_BIOME_SNOW || bi == MTN_BIOME_ALPINE) return mtn_rgb(225, 225, 230);
    return mtn_rgb(235, 228, 215);
}

static void wall_with_hole(bctx *b, int axis_x, float fixed, float a0, float a1, float y0, float y1, float h0, float h1,
                           float v0, float v1, float thick, uint32_t col, int tile, float uvs) {
    /* builds a log wall as boxes around a hole [h0,h1] x [v0,v1] (door or window); axis_x: wall runs along x */
    float segs[4][4];
    int n = 0;
    float s1[4] = {a0, h0, y0, y1}, s2[4] = {h1, a1, y0, y1}, s3[4] = {h0, h1, v1, y1}, s4[4] = {h0, h1, y0, v0};
    memcpy(segs[n++], s1, sizeof s1);
    memcpy(segs[n++], s2, sizeof s2);
    if (v1 < y1) memcpy(segs[n++], s3, sizeof s3);
    if (v0 > y0) memcpy(segs[n++], s4, sizeof s4);
    for (int i = 0; i < n; i++) {
        if (axis_x) box(b, v3(segs[i][0], segs[i][2], fixed - thick), v3(segs[i][1], segs[i][3], fixed), col, tile, MTN_VF_SNOW, uvs);
        else box(b, v3(fixed - thick, segs[i][2], segs[i][0]), v3(fixed, segs[i][3], segs[i][1]), col, tile, MTN_VF_SNOW, uvs);
    }
}

void mtn_house_build_exterior(const mtn_house *h, const mtn_terrain *t, const mtn_flora *f, mtn_mesh *solid,
                              mtn_mesh *sprites, mtn_mesh *glow) {
    (void)f;
    if (!mtn_house_exists(h)) return;
    mtn_rng r;
    mtn_rng_seed(&r, h->seed, 5);
    bctx b = {solid, h->frame};
    bctx g = {glow, h->frame};
    bctx sp = {sprites, h->frame};
    float hw = h->width * 0.5f, hd = h->depth * 0.5f, wh = h->wall_h;
    float base = h->ground_min - h->floor_y - 0.6f; /* local y of the ground below */
    uint32_t wood = mtn_rgb(255, 255, 255);
    uint32_t stone = stone_color(t, h->origin);
    const float th = 0.3f;

    /* foundation / supports per variant */
    if (h->variant == MTN_HV_COTTAGE || h->variant == MTN_HV_HILLSIDE) {
        box(&b, v3(-hw - 0.1f, base, -hd - 0.1f), v3(hw + 0.1f, 0.0f, hd + 0.1f), stone, 5, MTN_VF_SNOW, 1.0f);
    } else {
        float py = h->variant == MTN_HV_TREE ? base : base - 1.0f;
        float pw = hw + DECK, pd = hd + DECK;
        box(&b, v3(-pw, -0.25f, -pd), v3(pw, 0.0f, pd), wood, 2, MTN_VF_SNOW, 0.8f);
        float xs[3] = {-pw + 0.2f, 0.0f, pw - 0.2f}, zs[2] = {-pd + 0.2f, pd - 0.2f};
        for (int i = 0; i < 3; i++)
            for (int k = 0; k < 2; k++) {
                if (h->variant == MTN_HV_TREE) {
                    /* diagonal braces towards the trunk */
                    cyl(&b, v3(xs[i] * 0.15f, base + 1.8f, zs[k] * 0.15f), v3(xs[i], -0.25f, zs[k]), 0.12f, 0.1f, 5,
                        mtn_rgb(150, 110, 75), NO_TEXTURE, 1.0f);
                } else {
                    cyl(&b, v3(xs[i], py, zs[k]), v3(xs[i], -0.25f, zs[k]), 0.16f, 0.14f, 6, mtn_rgb(120, 90, 62), NO_TEXTURE, 1.0f);
                }
            }
        /* railing around the platform, open at the stairs */
        uint32_t rc = mtn_rgb(140, 100, 66);
        const float ry = 0.95f;
        {
            /* the four edges: front, back, right, left; the stair side has a gap */
            vec3 ea[4] = {v3(-pw, ry, pd), v3(-pw, ry, -pd), v3(pw, ry, -pd), v3(-pw, ry, -pd)};
            vec3 eb[4] = {v3(pw, ry, pd), v3(pw, ry, -pd), v3(pw, ry, pd), v3(-pw, ry, pd)};
            for (int e = 0; e < 4; e++) {
                if (e == h->stair_side && h->stair_len > 0.0f) {
                    vec3 mid = v3_scale(v3_add(ea[e], eb[e]), 0.5f), dir = v3_norm(v3_sub(eb[e], ea[e]));
                    cyl(&b, ea[e], v3_sub(mid, v3_scale(dir, 0.9f)), 0.05f, 0.05f, 4, rc, NO_TEXTURE, 1.0f);
                    cyl(&b, v3_add(mid, v3_scale(dir, 0.9f)), eb[e], 0.05f, 0.05f, 4, rc, NO_TEXTURE, 1.0f);
                } else {
                    cyl(&b, ea[e], eb[e], 0.05f, 0.05f, 4, rc, NO_TEXTURE, 1.0f);
                }
            }
        }
        float px4[4] = {-pw, pw, -pw, pw}, pz4[4] = {-pd, -pd, pd, pd};
        for (int i = 0; i < 4; i++) cyl(&b, v3(px4[i], 0, pz4[i]), v3(px4[i], ry, pz4[i]), 0.06f, 0.06f, 4, rc, NO_TEXTURE, 1.0f);
    }
    if (h->variant == MTN_HV_COTTAGE || h->variant == MTN_HV_HILLSIDE) {
        /* porch deck */
        box(&b, v3(-PORCH_HALF, -0.2f, hd), v3(PORCH_HALF, 0.0f, hd + PORCH_DEPTH), wood, 2, MTN_VF_SNOW, 0.8f);
        box(&b, v3(-PORCH_HALF, base, hd), v3(PORCH_HALF, -0.2f, hd + PORCH_DEPTH), stone, 5, 0, 1.0f);
    }
    /* stairs, built along their own axis */
    if (h->stair_len > 0.0f) {
        vec3 ax = stair_axis(h->stair_side);
        bctx st = {solid, m4_mul(h->frame, m4_from_basis(v3(0, 0, 0), v3_cross(v3(0, 1, 0), ax), v3(0, 1, 0), ax))};
        float s0 = stair_start(h), len = h->stair_len, drop = len * STAIR_GRADE;
        int steps = (int)(drop / 0.22f) + 1;
        for (int i = 0; i < steps; i++) {
            float y = -drop * (float)(i + 1) / (float)steps;
            float z = s0 + len * (float)i / (float)steps;
            box(&st, v3(-0.8f, y - 0.08f, z), v3(0.8f, y + drop / (float)steps, z + len / (float)steps), wood, 2, MTN_VF_SNOW, 0.8f);
        }
        /* stringers down to the ground and a handrail */
        for (int sd = -1; sd <= 1; sd += 2) {
            float x = 0.82f * (float)sd;
            cyl(&st, v3(x, -0.1f, s0), v3(x, -drop - 0.1f, s0 + len), 0.06f, 0.06f, 4, mtn_rgb(120, 90, 62), NO_TEXTURE, 1.0f);
        }
        cyl(&st, v3(-0.85f, 0.9f, s0), v3(-0.85f, 0.9f - drop, s0 + len), 0.04f, 0.04f, 4, mtn_rgb(140, 100, 66), NO_TEXTURE, 1.0f);
        cyl(&st, v3(-0.85f, -drop, s0 + len), v3(-0.85f, 0.9f - drop, s0 + len), 0.05f, 0.05f, 4, mtn_rgb(140, 100, 66), NO_TEXTURE, 1.0f);
    }

    /* log walls (textured boxes) with door and window holes */
    float dw = MTN_DOOR_WIDTH * 0.5f;
    wall_with_hole(&b, 1, hd, -hw, hw, 0.0f, wh, -dw, dw, -1.0f, MTN_DOOR_HEIGHT, th, wood, 0, 3.3f);
    wall_with_hole(&b, 1, -hd + th, -hw, hw, 0.0f, wh, -0.7f, 0.7f, 1.0f, 2.0f, th, wood, 0, 3.3f);
    wall_with_hole(&b, 0, hw, -hd + th, hd - th, 0.0f, wh, -1.1f, 1.1f, 0.9f, 2.0f, th, wood, 0, 3.3f);
    wall_with_hole(&b, 0, -hw + th, -hd + th, hd - th, 0.0f, wh, -0.8f, 0.8f, 0.9f, 2.0f, th, wood, 0, 3.3f);
    /* protruding log ends at the corners */
    for (int i = 0; i < 9; i++) {
        float y = 0.15f + (float)i * 0.29f;
        if (y > wh) break;
        float cx[4] = {-hw, hw, -hw, hw}, cz[4] = {-hd, -hd, hd, hd};
        for (int k = 0; k < 4; k++) {
            vec3 c = v3(cx[k], y, cz[k]);
            if (i % 2) cyl(&b, v3_add(c, v3(-0.25f, 0, 0)), v3_add(c, v3(0.25f, 0, 0)), 0.15f, 0.15f, 6, mtn_rgb(150, 110, 75), 1, 1.0f);
            else cyl(&b, v3_add(c, v3(0, 0, -0.25f)), v3_add(c, v3(0, 0, 0.25f)), 0.15f, 0.15f, 6, mtn_rgb(150, 110, 75), 1, 1.0f);
        }
    }
    /* door frame */
    box(&b, v3(-dw - 0.12f, 0, hd), v3(-dw, MTN_DOOR_HEIGHT + 0.1f, hd + 0.06f), mtn_rgb(110, 80, 55), NO_TEXTURE, 0, 1.0f);
    box(&b, v3(dw, 0, hd), v3(dw + 0.12f, MTN_DOOR_HEIGHT + 0.1f, hd + 0.06f), mtn_rgb(110, 80, 55), NO_TEXTURE, 0, 1.0f);
    box(&b, v3(-dw - 0.12f, MTN_DOOR_HEIGHT, hd), v3(dw + 0.12f, MTN_DOOR_HEIGHT + 0.12f, hd + 0.08f), mtn_rgb(110, 80, 55), NO_TEXTURE, MTN_VF_SNOW, 1.0f);
    /* glowing windows (glass with mullions), drawn with the night glow */
    quad(&g, v3(hw + 0.01f, 0.9f, 1.1f), v3(hw + 0.01f, 0.9f, -1.1f), v3(hw + 0.01f, 2.0f, -1.1f), v3(hw + 0.01f, 2.0f, 1.1f),
         WHITE, 13, MTN_VF_EMISSIVE | MTN_VF_ABS_UV, v2(1, 1));
    quad(&g, v3(-hw - 0.01f, 0.9f, -0.8f), v3(-hw - 0.01f, 0.9f, 0.8f), v3(-hw - 0.01f, 2.0f, 0.8f), v3(-hw - 0.01f, 2.0f, -0.8f),
         WHITE, 13, MTN_VF_EMISSIVE | MTN_VF_ABS_UV, v2(1, 1));
    quad(&g, v3(0.7f, 1.0f, -hd - 0.01f), v3(-0.7f, 1.0f, -hd - 0.01f), v3(-0.7f, 2.0f, -hd - 0.01f), v3(0.7f, 2.0f, -hd - 0.01f),
         WHITE, 13, MTN_VF_EMISSIVE | MTN_VF_ABS_UV, v2(1, 1));
    { /* the front window beside the door, where the cabin's window looks out */
        vec3 wc = m4_mul_point(h->inv_frame, m4_mul_point(m4_inverse(mtn_house_ext_to_int(h)), v3(2.0f, 1.55f, 0.0f)));
        float x0 = wc.x - 0.7f, x1 = wc.x + 0.7f, z = hd + 0.01f;
        quad(&g, v3(x0, 0.9f, z), v3(x1, 0.9f, z), v3(x1, 2.2f, z), v3(x0, 2.2f, z), WHITE, 13, MTN_VF_EMISSIVE | MTN_VF_ABS_UV, v2(1, 1));
        box(&b, v3(x0 - 0.1f, 0.82f, hd), v3(x1 + 0.1f, 0.9f, hd + 0.18f), mtn_rgb(120, 85, 58), NO_TEXTURE, MTN_VF_SNOW, 1.0f);
        box(&b, v3(x0 - 0.1f, 2.2f, hd), v3(x1 + 0.1f, 2.3f, hd + 0.08f), mtn_rgb(120, 85, 58), NO_TEXTURE, MTN_VF_SNOW, 1.0f);
    }
    /* window sills and shutters */
    box(&b, v3(hw, 0.82f, -1.2f), v3(hw + 0.18f, 0.9f, 1.2f), mtn_rgb(120, 85, 58), NO_TEXTURE, MTN_VF_SNOW, 1.0f);
    box(&b, v3(-hw - 0.18f, 0.82f, -0.9f), v3(-hw, 0.9f, 0.9f), mtn_rgb(120, 85, 58), NO_TEXTURE, MTN_VF_SNOW, 1.0f);
    uint32_t shutter = mtn_rgb(70 + (int)(mtn_rng_u32(&r) % 60u), 95, 70 + (int)(mtn_rng_u32(&r) % 50u));
    box(&b, v3(hw, 0.9f, 1.1f), v3(hw + 0.06f, 2.0f, 1.65f), shutter, 14, 0, 1.0f);
    box(&b, v3(hw, 0.9f, -1.65f), v3(hw + 0.06f, 2.0f, -1.1f), shutter, 14, 0, 1.0f);

    /* gable roof, ridge along z */
    float rh = 1.9f, ov = 0.45f;
    uint32_t roof = mtn_rgb(255, 255, 255);
    vec3 eave_l0 = v3(-hw - ov, wh - 0.15f, -hd - ov), eave_l1 = v3(-hw - ov, wh - 0.15f, hd + ov);
    vec3 ridge0 = v3(0, wh + rh, -hd - ov), ridge1 = v3(0, wh + rh, hd + ov);
    vec3 eave_r0 = v3(hw + ov, wh - 0.15f, -hd - ov), eave_r1 = v3(hw + ov, wh - 0.15f, hd + ov);
    float slope_len = v3_len(v3_sub(ridge0, eave_l0));
    vec2 ruv = v2((h->depth + 2 * ov) * 1.6f, slope_len * 1.6f);
    quad(&b, eave_l0, eave_l1, ridge1, ridge0, roof, 4, MTN_VF_SNOW, ruv);
    quad(&b, eave_r1, eave_r0, ridge0, ridge1, roof, 4, MTN_VF_SNOW, ruv);
    /* roof undersides and thickness */
    vec3 dn = v3(0, -0.12f, 0);
    quad(&b, v3_add(eave_l1, dn), v3_add(eave_l0, dn), v3_add(ridge0, dn), v3_add(ridge1, dn), mtn_rgb(120, 90, 60), NO_TEXTURE, 0, v2(1, 1));
    quad(&b, v3_add(eave_r0, dn), v3_add(eave_r1, dn), v3_add(ridge1, dn), v3_add(ridge0, dn), mtn_rgb(120, 90, 60), NO_TEXTURE, 0, v2(1, 1));
    /* moss on the roof in damp places */
    float moist = t ? mtn_terrain_moisture(t, h->origin.x, h->origin.z) : 0.5f;
    mtn_biome hb = t ? mtn_terrain_biome(t, h->origin.x, h->origin.z) : MTN_BIOME_FOREST;
    int lichen = hb == MTN_BIOME_ALPINE || hb == MTN_BIOME_ROCK || hb == MTN_BIOME_SCREE || hb == MTN_BIOME_SNOW;
    /* moss in damp forests, pale yellow-grey lichen up in the alpine and rocky places */
    uint32_t growth = lichen ? mtn_rgb(215, 205, 150) : WHITE;
    if (moist > 0.4f || lichen) {
        for (int i = 0; i < 6; i++) {
            float zz = mtn_rng_range(&r, -hd, hd), u = mtn_rng_range(&r, 0.15f, 0.8f);
            int side = i % 2;
            vec3 e0 = side ? eave_r0 : eave_l0;
            vec3 p = v3_lerp(v3(e0.x, e0.y, zz), v3(0, wh + rh, zz), u);
            vec3 nn = side ? v3(0.7f, 0.7f, 0) : v3(-0.7f, 0.7f, 0);
            p = v3_add(p, v3_scale(nn, 0.03f));
            vec3 ax = v3(0, 0, 0.6f), ay = side ? v3(-0.45f, 0.45f, 0) : v3(0.45f, 0.45f, 0);
            if (!side) quad(&b, v3_sub(v3_sub(p, ax), ay), v3_sub(v3_add(p, ax), ay), v3_add(v3_add(p, ax), ay), v3_add(v3_sub(p, ax), ay), growth, 16, MTN_VF_SNOW, v2(1, 1));
            else quad(&b, v3_sub(v3_add(p, ax), ay), v3_sub(v3_sub(p, ax), ay), v3_add(v3_sub(p, ax), ay), v3_add(v3_add(p, ax), ay), growth, 16, MTN_VF_SNOW, v2(1, 1));
        }
    }
    /* gables */
    for (int s = 0; s < 2; s++) {
        float z = s ? hd : -hd;
        vec3 a = v3(-hw, wh, z), c = v3(hw, wh, z), top = v3(0, wh + rh - 0.1f, z);
        mtn_material gm = mtn_mat(WHITE, 3, 0, 1.0f);
        vec3 nrm = s ? v3(0, 0, 1) : v3(0, 0, -1);
        int base_i = solid->vcount;
        vec3 pts[3] = {a, c, top};
        vec2 uvs[3] = {v2(0, rh), v2(h->width, rh), v2(h->width * 0.5f, 0)};
        for (int k = 0; k < 3; k++) {
            mtn_vertex v;
            memset(&v, 0, sizeof v);
            vec3 wp = m4_mul_point(h->frame, pts[k]);
            vec3 wn = m4_mul_dir(h->frame, nrm);
            v.x = wp.x; v.y = wp.y; v.z = wp.z;
            v.nx = wn.x; v.ny = wn.y; v.nz = wn.z;
            v.u = uvs[k].x; v.v = uvs[k].y;
            v.r = v.g = v.b = v.a = 255;
            v.tile = gm.tile;
            v.ao = 255;
            mtn_mesh_vertex(solid, &v);
        }
        if (s) mtn_mesh_tri(solid, base_i, base_i + 1, base_i + 2);
        else mtn_mesh_tri(solid, base_i, base_i + 2, base_i + 1);
    }
    /* stone chimney made of voxels */
    {
        mtn_voxels vx;
        mtn_voxels_init(&vx, 5, 22, 5, 0.2f);
        vx.pal[1] = stone;
        vx.pal[2] = mtn_rgb_scale(stone, 0.8f);
        vx.pal[3] = mtn_rgb(60, 55, 52);
        for (int y = 0; y < 22; y++)
            for (int z = 0; z < 5; z++)
                for (int x = 0; x < 5; x++) {
                    int edge = x == 0 || z == 0 || x == 4 || z == 4;
                    uint8_t c = (uint8_t)(((x + y * 3 + z) % 4 == 0) ? 2 : 1);
                    if (y >= 20 && !edge) c = 3;
                    if (y < 21 || edge) mtn_voxels_set(&vx, x, y, z, c);
                }
        mat4 cx = m4_mul(h->frame, m4_translate(v3(h->width * 0.3f - 0.5f, h->wall_h - 1.6f, -h->depth * 0.25f - 0.5f)));
        mtn_voxels_mesh(&vx, solid, cx, NO_TEXTURE, MTN_VF_SNOW);
        mtn_voxels_free(&vx);
    }
    /* lantern beside the door */
    box(&b, v3(dw + 0.35f, 1.7f, hd), v3(dw + 0.45f, 1.75f, hd + 0.35f), mtn_rgb(40, 40, 40), NO_TEXTURE, 0, 1.0f);
    box(&g, v3(dw + 0.3f, 1.4f, hd + 0.2f), v3(dw + 0.5f, 1.68f, hd + 0.4f), WHITE, 23, MTN_VF_EMISSIVE, 4.0f);

    /* variant specific integration */
    if (h->variant == MTN_HV_HILLSIDE && t) {
        mtn_voxels vx;
        mtn_voxels_init(&vx, 44, 30, 22, 0.25f);
        vx.pal[1] = mtn_rgb(118, 112, 104);
        vx.pal[2] = mtn_rgb(92, 88, 82);
        vx.pal[3] = mtn_rgb(90, 112, 60);
        for (int i = 0; i < 9; i++) {
            float bx = mtn_rng_range(&r, 3, 41), by = mtn_rng_range(&r, 2, 16), bz = mtn_rng_range(&r, 2, 12);
            mtn_voxels_blob(&vx, bx, by, bz, mtn_rng_range(&r, 3.0f, 6.0f), (uint8_t)(1 + (i % 2)), h->seed + (uint32_t)i);
        }
        /* moss caps */
        for (int z = 0; z < 22; z++)
            for (int x = 0; x < 44; x++)
                for (int y = 29; y > 0; y--)
                    if (mtn_voxels_get(&vx, x, y, z)) {
                        if (mtn_hash_unit(mtn_hash3i(x, y, z, 3)) < 0.6f) mtn_voxels_set(&vx, x, y, z, 3);
                        break;
                    }
        /* embed the rocks in the uphill ground behind the house */
        vec3 back = m4_mul_point(h->frame, v3(0, 0, -hd - 1.5f));
        float gy = mtn_terrain_height(t, back.x, back.z) - h->floor_y;
        mat4 rx = m4_mul(h->frame, m4_translate(v3(-5.5f, mtn_minf(gy - 3.0f, h->wall_h - 2.0f), -hd - 3.2f)));
        mtn_voxels_mesh(&vx, solid, rx, NO_TEXTURE, MTN_VF_SNOW);
        mtn_voxels_free(&vx);
    }
    if (h->variant == MTN_HV_COTTAGE) {
        /* stepping stones and a flower bed */
        for (int i = 0; i < 4; i++) {
            float z = hd + PORCH_DEPTH + 0.6f + (float)i * 0.9f;
            float x = mtn_rng_range(&r, -0.3f, 0.3f);
            vec3 wp = m4_mul_point(h->frame, v3(x, 0, z));
            float gy = t ? mtn_terrain_height(t, wp.x, wp.z) - h->floor_y : base;
            box(&b, v3(x - 0.35f, gy - 0.1f, z - 0.3f), v3(x + 0.35f, gy + 0.06f, z + 0.3f), stone, 5, MTN_VF_SNOW, 1.0f);
        }
        for (int i = 0; i < 10; i++) {
            float x = (i < 5 ? -1 : 1) * mtn_rng_range(&r, PORCH_HALF + 0.3f, hw);
            float z = hd + mtn_rng_range(&r, 0.1f, 0.7f);
            vec3 wp = m4_mul_point(h->frame, v3(x, 0, z));
            float gy = t ? mtn_terrain_height(t, wp.x, wp.z) - h->floor_y : base;
            mtn_material fm = mtn_mat(WHITE, 16 + (int)(mtn_rng_u32(&r) % 8u), MTN_VF_WIND | MTN_VF_UPRIGHT, 1.0f);
            mtn_mesh_billboard(sp.m, m4_mul_point(h->frame, v3(x, gy, z)), 0.5f, 0.45f, 1.0f, &fm, 0.3f);
        }
    }
    /* ivy creeping up a corner, so it looks like it has always been here */
    for (int i = 0; i < 5; i++) {
        float y = (float)i * 0.45f;
        vec3 p = v3(-hw - 0.02f, y, -hd + 0.3f + mtn_rng_range(&r, 0, 0.6f));
        quad(&b, v3_add(p, v3(0, 0, -0.4f)), v3_add(p, v3(0, 0, 0.4f)), v3_add(p, v3(0, 0.6f, 0.4f)), v3_add(p, v3(0, 0.6f, -0.4f)),
             WHITE, 22, MTN_VF_WIND, v2(1, 1));
    }
}

void mtn_house_build_door(mtn_mesh *m) {
    /* hinge at the origin, the leaf extends along +x, outside face towards +z */
    mtn_material d = mtn_mat(WHITE, 14, 0, 1.0f);
    mtn_mesh_quad(m, m4_identity(), v3(0, 0, 0.03f), v3(MTN_DOOR_WIDTH, 0, 0.03f), v3(MTN_DOOR_WIDTH, MTN_DOOR_HEIGHT, 0.03f),
                  v3(0, MTN_DOOR_HEIGHT, 0.03f), &d, v2(0, 0), v2(1, 1));
    mtn_mesh_quad(m, m4_identity(), v3(MTN_DOOR_WIDTH, 0, -0.03f), v3(0, 0, -0.03f), v3(0, MTN_DOOR_HEIGHT, -0.03f),
                  v3(MTN_DOOR_WIDTH, MTN_DOOR_HEIGHT, -0.03f), &d, v2(0, 0), v2(1, 1));
    mtn_material e = mtn_mat(mtn_rgb(90, 64, 44), NO_TEXTURE, 0, 1.0f);
    mtn_mesh_box(m, m4_identity(), v3(0, 0, -0.03f), v3(MTN_DOOR_WIDTH, 0.02f, 0.03f), &e, 63);
    mtn_mesh_box(m, m4_identity(), v3(MTN_DOOR_WIDTH - 0.02f, 0, -0.03f), v3(MTN_DOOR_WIDTH, MTN_DOOR_HEIGHT, 0.03f), &e, 63);
    mtn_material k = mtn_mat(mtn_rgb(200, 160, 70), NO_TEXTURE, 0, 1.0f);
    mtn_mesh_box(m, m4_identity(), v3(MTN_DOOR_WIDTH - 0.2f, 1.0f, -0.08f), v3(MTN_DOOR_WIDTH - 0.12f, 1.08f, 0.08f), &k, 63);
}

mat4 mtn_house_door_model(const mtn_house *h, int interior) {
    float dw = MTN_DOOR_WIDTH * 0.5f;
    if (interior) {
        /* interior front wall at z=0 facing +z; the leaf swings out to -z */
        /* the portal mirrors x: the hinge is on +x inside, the leaf points to -x */
        return m4_mul(m4_translate(v3(dw, 0, 0)), m4_rotate_y(MTN_PI - h->door_angle));
    }
    mat4 local = m4_mul(m4_translate(v3(-dw, 0, h->depth * 0.5f)), m4_rotate_y(-h->door_angle));
    return m4_mul(h->frame, local);
}
