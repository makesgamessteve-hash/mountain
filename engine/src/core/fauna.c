/* Mountain engine - calm wildlife: species data, spawning, behaviour, meshes. MIT License. */
#include "mtn/fauna.h"

#include <math.h>
#include <string.h>

#define NO_TEXTURE 255

void mtn_fauna_init(mtn_fauna *f, uint32_t seed) {
    int sc = f->species_count;
    mtn_animal_species keep[MTN_MAX_ANIMAL_SPECIES];
    memcpy(keep, f->species, sizeof keep);
    memset(f, 0, sizeof *f);
    memcpy(f->species, keep, sizeof keep);
    f->species_count = sc;
    mtn_rng_seed(&f->rng, seed, 909);
}

void mtn_animal_defaults(mtn_animal_species *s) {
    memset(s, 0, sizeof *s);
    strcpy(s->name, "animal");
    s->kind = MTN_ANIMAL_QUADRUPED;
    s->size = 1.4f;
    s->leg_len = 0.8f;
    s->neck_len = 0.5f;
    s->head_size = 0.25f;
    s->color = v3(0.55f, 0.38f, 0.22f);
    s->color2 = v3(0.85f, 0.78f, 0.65f);
    s->tail = 0.15f;
    s->walk_speed = 1.2f;
    s->run_speed = 5.0f;
    s->comfort_dist = 18.0f;
    s->alt_min = 0.0f;
    s->alt_max = 1.0f;
    s->biome_mask = 0xFE;
    s->hour_start = 0.0f;
    s->hour_end = 24.0f;
    s->season_mask = 15;
    s->group_min = 1;
    s->group_max = 3;
    s->max_groups = 2;
    s->spawn_chance = 0.5f;
    s->sprite_tile = 32;
}

int mtn_fauna_add_species(mtn_fauna *f, const mtn_animal_species *s) {
    if (f->species_count >= MTN_MAX_ANIMAL_SPECIES) return -1;
    f->species[f->species_count] = *s;
    return f->species_count++;
}

void mtn_fauna_clear(mtn_fauna *f) {
    memset(f->animals, 0, sizeof f->animals);
    f->call_count = 0;
}

static int hour_in(float h, float a, float b) { return a <= b ? (h >= a && h <= b) : (h >= a || h <= b); }

int mtn_fauna_active(const mtn_animal_species *s, float hour, mtn_season_blend season) {
    if (!hour_in(hour, s->hour_start, s->hour_end)) return 0;
    float w = 0.0f;
    for (int k = 0; k < MTN_SEASON_COUNT; k++)
        if (s->season_mask & (1 << k)) w += season.w[k];
    return w >= 0.5f;
}

int mtn_fauna_count(const mtn_fauna *f, int species) {
    int n = 0;
    for (int i = 0; i < MTN_MAX_ANIMALS; i++)
        if (f->animals[i].alive && (species < 0 || f->animals[i].species == species)) n++;
    return n;
}

static int groups_of(const mtn_fauna *f, int species) {
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

static int habitat_ok(const mtn_animal_species *s, const mtn_terrain *t, float x, float z) {
    if (!mtn_terrain_in_playable(t, x, z)) return 0;
    if (mtn_terrain_water(t, x, z) != MTN_NO_WATER) return 0;
    if (!(s->biome_mask & (1 << mtn_terrain_biome(t, x, z)))) return 0;
    float a = mtn_terrain_altitude_frac(t, mtn_terrain_height(t, x, z));
    return a >= s->alt_min && a <= s->alt_max;
}

static float base_height(const mtn_animal_species *s, mtn_rng *r) {
    switch (s->kind) {
    case MTN_ANIMAL_BIRD: return mtn_rng_range(r, 14.0f, 32.0f);
    case MTN_ANIMAL_SOARING: return mtn_rng_range(r, 90.0f, 140.0f);
    case MTN_ANIMAL_BUTTERFLY: return mtn_rng_range(r, 0.5f, 1.6f);
    case MTN_ANIMAL_FIREFLY: return mtn_rng_range(r, 0.4f, 2.5f);
    default: return 0.0f;
    }
}

int mtn_fauna_spawn_group(mtn_fauna *f, const mtn_terrain *t, int si, vec3 at) {
    if (si < 0 || si >= f->species_count) return 0;
    const mtn_animal_species *s = &f->species[si];
    if (!habitat_ok(s, t, at.x, at.z)) return 0;
    int n = mtn_rng_int(&f->rng, s->group_min, s->group_max);
    int group = ++f->next_group;
    int made = 0;
    for (int i = 0; i < MTN_MAX_ANIMALS && made < n; i++) {
        mtn_animal *a = &f->animals[i];
        if (a->alive) continue;
        memset(a, 0, sizeof *a);
        a->alive = 1;
        a->species = si;
        a->group = group;
        float spread = s->kind == MTN_ANIMAL_QUADRUPED ? 6.0f : 3.0f;
        a->pos = v3(at.x + mtn_rng_range(&f->rng, -spread, spread), 0, at.z + mtn_rng_range(&f->rng, -spread, spread));
        a->home = at;
        a->home.y = base_height(s, &f->rng);
        a->pos.y = mtn_terrain_height(t, a->pos.x, a->pos.z) + a->home.y;
        a->yaw = mtn_rng_range(&f->rng, 0, MTN_TAU);
        a->phase = mtn_rng_range(&f->rng, 0, MTN_TAU);
        a->state = s->kind == MTN_ANIMAL_QUADRUPED ? MTN_AS_GRAZE : MTN_AS_FLY;
        a->timer = mtn_rng_range(&f->rng, 2.0f, 8.0f);
        a->fade = 0.0f;
        made++;
    }
    return made;
}

static void spawn_distance(const mtn_animal_species *s, float *mn, float *mx) {
    switch (s->kind) {
    case MTN_ANIMAL_BUTTERFLY: *mn = 6.0f; *mx = 30.0f; break;
    case MTN_ANIMAL_FIREFLY: *mn = 4.0f; *mx = 25.0f; break;
    case MTN_ANIMAL_BIRD: *mn = 60.0f; *mx = 180.0f; break;
    case MTN_ANIMAL_SOARING: *mn = 120.0f; *mx = 250.0f; break;
    default: *mn = 45.0f; *mx = 140.0f; break;
    }
}

static void try_spawn(mtn_fauna *f, const mtn_terrain *t, vec3 player, float hour, mtn_season_blend season,
                      float precipitation) {
    if (f->species_count == 0) return;
    int si = mtn_rng_int(&f->rng, 0, f->species_count - 1);
    const mtn_animal_species *s = &f->species[si];
    if (!mtn_fauna_active(s, hour, season)) return;
    if ((s->kind == MTN_ANIMAL_BUTTERFLY || s->kind == MTN_ANIMAL_FIREFLY) && precipitation > 0.3f) return;
    if (groups_of(f, si) >= s->max_groups) return;
    if (mtn_rng_float(&f->rng) >= s->spawn_chance) return;
    float mn, mx;
    spawn_distance(s, &mn, &mx);
    float ang = mtn_rng_range(&f->rng, 0, MTN_TAU), d = mtn_rng_range(&f->rng, mn, mx);
    vec3 at = v3(player.x + cosf(ang) * d, 0, player.z + sinf(ang) * d);
    mtn_fauna_spawn_group(f, t, si, at);
}

static void push_call(mtn_fauna *f, const char *name) {
    if (!name[0] || f->call_count >= 4) return;
    strncpy(f->call_events[f->call_count], name, 23);
    f->call_events[f->call_count][23] = 0;
    f->call_count++;
}

const char *mtn_fauna_pop_call(mtn_fauna *f) {
    static char out[24];
    if (f->call_count == 0) return "";
    memcpy(out, f->call_events[0], sizeof out);
    memmove(f->call_events[0], f->call_events[1], sizeof(f->call_events[0]) * 3);
    f->call_count--;
    return out;
}

static void update_quadruped(mtn_fauna *f, const mtn_terrain *t, mtn_animal *a, const mtn_animal_species *s,
                             vec3 player, float dt) {
    float pd = v3_dist_xz(a->pos, player);
    float speed = 0.0f;
    a->timer -= dt;
    if (pd < s->comfort_dist && a->state != MTN_AS_FLEE) {
        a->state = MTN_AS_FLEE;
        a->timer = mtn_rng_range(&f->rng, 3.0f, 6.0f);
        push_call(f, s->sound);
    }
    if (a->state == MTN_AS_FLEE) {
        vec3 away = v3_norm(v3(a->pos.x - player.x, 0, a->pos.z - player.z));
        a->vel = v3_scale(away, s->run_speed * 0.6f); /* a calm trot, never panicked */
        speed = s->run_speed * 0.6f;
        if (a->timer <= 0.0f && pd > s->comfort_dist * 1.6f) {
            a->state = MTN_AS_IDLE;
            a->timer = mtn_rng_range(&f->rng, 2.0f, 5.0f);
            a->home = a->pos;
        }
    } else if (a->state == MTN_AS_WALK) {
        vec3 to = v3(a->home.x - a->pos.x, 0, a->home.z - a->pos.z);
        float d = v3_len(to);
        if (d < 1.0f || a->timer <= 0.0f) {
            a->state = MTN_AS_GRAZE;
            a->timer = mtn_rng_range(&f->rng, 3.0f, 9.0f);
            a->vel = v3(0, 0, 0);
        } else {
            a->vel = v3_scale(v3_norm(to), s->walk_speed);
            speed = s->walk_speed;
        }
    } else if (a->timer <= 0.0f) {
        float r = mtn_rng_float(&f->rng);
        if (r < 0.5f) {
            float ang = mtn_rng_range(&f->rng, 0, MTN_TAU), d = mtn_rng_range(&f->rng, 5.0f, 25.0f);
            vec3 target = v3(a->pos.x + cosf(ang) * d, 0, a->pos.z + sinf(ang) * d);
            if (habitat_ok(s, t, target.x, target.z)) {
                a->home = target;
                a->state = MTN_AS_WALK;
                a->timer = d / s->walk_speed + 3.0f;
            }
        } else {
            a->state = r < 0.8f ? MTN_AS_GRAZE : MTN_AS_IDLE;
            a->timer = mtn_rng_range(&f->rng, 2.0f, 8.0f);
        }
        a->vel = v3(0, 0, 0);
        if (mtn_rng_float(&f->rng) < 0.15f) push_call(f, s->sound);
    }
    vec3 np = v3_add(a->pos, v3_scale(a->vel, dt));
    if (mtn_terrain_water(t, np.x, np.z) == MTN_NO_WATER && mtn_terrain_slope(t, np.x, np.z) < 0.6f) {
        a->pos.x = np.x;
        a->pos.z = np.z;
    } else if (a->state != MTN_AS_FLEE) {
        a->state = MTN_AS_IDLE;
        a->timer = 1.0f;
    } else {
        a->vel = v3_rotate_y(a->vel, 0.8f); /* turn along the obstacle */
    }
    a->pos.y = mtn_terrain_height(t, a->pos.x, a->pos.z);
    if (speed > 0.01f) {
        float target = atan2f(a->vel.x, -a->vel.z);
        a->yaw += mtn_wrap_angle(target - a->yaw) * mtn_minf(1.0f, dt * 6.0f);
    }
    a->anim += dt * (speed > 0 ? speed * 3.0f : 1.0f);
}

void mtn_fauna_update(mtn_fauna *f, const mtn_terrain *t, vec3 player, float hour, mtn_season_blend season,
                      float precipitation, float dt) {
    f->spawn_timer -= dt;
    if (f->spawn_timer <= 0.0f) {
        f->spawn_timer = 1.5f;
        try_spawn(f, t, player, hour, season, precipitation);
    }
    for (int i = 0; i < MTN_MAX_ANIMALS; i++) {
        mtn_animal *a = &f->animals[i];
        if (!a->alive) continue;
        const mtn_animal_species *s = &f->species[a->species];
        int active = mtn_fauna_active(s, hour, season);
        if ((s->kind == MTN_ANIMAL_BUTTERFLY || s->kind == MTN_ANIMAL_FIREFLY) && precipitation > 0.3f) active = 0;
        if (!active || v3_dist_xz(a->pos, player) > 300.0f) a->leaving = 1;
        a->fade = mtn_clampf(a->fade + (a->leaving ? -dt : dt) * 0.8f, 0.0f, 1.0f);
        if (a->leaving && a->fade <= 0.0f) {
            a->alive = 0;
            continue;
        }
        float ground = mtn_terrain_height(t, a->pos.x, a->pos.z);
        switch (s->kind) {
        case MTN_ANIMAL_QUADRUPED: update_quadruped(f, t, a, s, player, dt); break;
        case MTN_ANIMAL_BIRD:
        case MTN_ANIMAL_SOARING: {
            float radius = s->kind == MTN_ANIMAL_SOARING ? 70.0f : 18.0f;
            float speed = s->kind == MTN_ANIMAL_SOARING ? 9.0f : 7.0f;
            a->phase += speed / radius * dt;
            a->home.x += f->rng.state & 1 ? dt * 0.3f : -dt * 0.3f;
            vec3 prev = a->pos;
            float hg = mtn_terrain_height(t, a->home.x, a->home.z);
            a->pos = v3(a->home.x + cosf(a->phase) * radius, hg + a->home.y + sinf(a->phase * 2.0f) * 2.0f,
                        a->home.z + sinf(a->phase) * radius);
            if (a->pos.y < ground + 5.0f) a->pos.y = ground + 5.0f;
            a->vel = v3_scale(v3_sub(a->pos, prev), dt > 0 ? 1.0f / dt : 0.0f);
            a->yaw = atan2f(a->vel.x, -a->vel.z);
            a->anim += dt * (s->kind == MTN_ANIMAL_SOARING ? 1.5f : 14.0f);
            break;
        }
        case MTN_ANIMAL_BUTTERFLY:
        case MTN_ANIMAL_FIREFLY: {
            float jitter = s->kind == MTN_ANIMAL_BUTTERFLY ? 6.0f : 1.5f;
            a->vel.x += mtn_rng_range(&f->rng, -1, 1) * jitter * dt;
            a->vel.z += mtn_rng_range(&f->rng, -1, 1) * jitter * dt;
            vec3 pull = v3(a->home.x - a->pos.x, 0, a->home.z - a->pos.z);
            a->vel = v3_add(a->vel, v3_scale(pull, 0.15f * dt));
            float sp = v3_len(a->vel), maxs = s->kind == MTN_ANIMAL_BUTTERFLY ? 1.6f : 0.4f;
            if (sp > maxs) a->vel = v3_scale(a->vel, maxs / sp);
            a->pos.x += a->vel.x * dt;
            a->pos.z += a->vel.z * dt;
            a->pos.y = mtn_terrain_height(t, a->pos.x, a->pos.z) + a->home.y + 0.3f * sinf(a->anim * 1.7f);
            a->anim += dt * (s->kind == MTN_ANIMAL_BUTTERFLY ? 1.0f : 0.5f);
            break;
        }
        }
    }
}

/* ------------------------------------------------------------------ meshes */

static uint32_t col(vec3 c, float s) {
    return mtn_rgb((int)(c.x * 255.0f * s), (int)(c.y * 255.0f * s), (int)(c.z * 255.0f * s));
}

static void build_quadruped(const mtn_animal_species *s, const mtn_animal *a, mtn_mesh *m) {
    mat4 xf = m4_mul(m4_translate(a->pos), m4_rotate_y(-a->yaw));
    float L = s->size, leg = s->leg_len, r = L * 0.2f;
    mtn_material body = mtn_mat(col(s->color, 1.0f), NO_TEXTURE, 0, 1.0f);
    mtn_material belly = mtn_mat(col(s->color2, 1.0f), NO_TEXTURE, 0, 1.0f);
    mtn_material dark = mtn_mat(col(s->color, 0.55f), NO_TEXTURE, 0, 1.0f);
    float y = leg + r * 0.6f;
    /* torso: chest and hindquarters */
    mtn_mesh_cylinder(m, xf, v3(0, y, L * 0.45f), v3(0, y + r * 0.1f, -L * 0.05f), r * 0.9f, r, 6, &body, 1);
    mtn_mesh_cylinder(m, xf, v3(0, y + r * 0.1f, -L * 0.05f), v3(0, y + r * 0.15f, -L * 0.42f), r, r * 0.85f, 6, &body, 1);
    mtn_mesh_cylinder(m, xf, v3(0, y - r * 0.55f, L * 0.3f), v3(0, y - r * 0.55f, -L * 0.3f), r * 0.45f, r * 0.45f, 4,
                      &belly, 0);
    /* neck and head: lowered when grazing */
    float graze = a->state == MTN_AS_GRAZE ? 0.5f + 0.5f * sinf(a->anim * 0.7f) : 0.0f;
    vec3 n0 = v3(0, y + r * 0.4f, -L * 0.42f);
    vec3 n1 = v3(0, mtn_lerpf(y + s->neck_len, r * 0.6f, graze), -L * 0.42f - s->neck_len * (0.55f + 0.35f * graze));
    mtn_mesh_cylinder(m, xf, n0, n1, r * 0.45f, r * 0.35f, 5, &body, 0);
    float hs = s->head_size;
    vec3 h1 = v3_add(n1, v3(0, -hs * 0.3f, -hs * 1.4f));
    mtn_mesh_cylinder(m, xf, n1, h1, hs * 0.55f, hs * 0.3f, 5, &body, 1);
    if (s->horns == 1) { /* antlers */
        for (int sd = -1; sd <= 1; sd += 2) {
            vec3 b0 = v3_add(n1, v3((float)sd * hs * 0.3f, hs * 0.4f, 0));
            vec3 b1 = v3_add(b0, v3((float)sd * hs * 0.9f, hs * 1.8f, hs * 0.3f));
            mtn_mesh_cylinder(m, xf, b0, b1, hs * 0.08f, hs * 0.05f, 3, &dark, 0);
            mtn_mesh_cylinder(m, xf, v3_lerp(b0, b1, 0.5f), v3_add(v3_lerp(b0, b1, 0.5f), v3((float)sd * hs * 0.5f, hs * 0.6f, -hs * 0.4f)),
                              hs * 0.06f, hs * 0.04f, 3, &dark, 0);
        }
    } else if (s->horns == 2) { /* curved horns */
        for (int sd = -1; sd <= 1; sd += 2) {
            vec3 b0 = v3_add(n1, v3((float)sd * hs * 0.25f, hs * 0.35f, -hs * 0.1f));
            vec3 b1 = v3_add(b0, v3((float)sd * hs * 0.2f, hs * 0.9f, hs * 0.6f));
            vec3 b2 = v3_add(b1, v3((float)sd * hs * 0.1f, hs * 0.1f, hs * 0.7f));
            mtn_mesh_cylinder(m, xf, b0, b1, hs * 0.14f, hs * 0.09f, 4, &dark, 0);
            mtn_mesh_cylinder(m, xf, b1, b2, hs * 0.09f, hs * 0.03f, 4, &dark, 0);
        }
    } else if (s->horns == 3) { /* long ears */
        for (int sd = -1; sd <= 1; sd += 2) {
            vec3 b0 = v3_add(n1, v3((float)sd * hs * 0.25f, hs * 0.3f, 0));
            mtn_mesh_cylinder(m, xf, b0, v3_add(b0, v3((float)sd * hs * 0.2f, hs * 1.6f, hs * 0.4f)), hs * 0.15f,
                              hs * 0.08f, 4, &body, 0);
        }
    }
    /* legs with a walk cycle */
    float moving = (a->state == MTN_AS_WALK || a->state == MTN_AS_FLEE) ? 1.0f : 0.0f;
    const float lx[4] = {-1, 1, -1, 1}, lz[4] = {-0.32f, -0.32f, 0.32f, 0.32f}, ph[4] = {0, MTN_PI, MTN_PI, 0};
    for (int i = 0; i < 4; i++) {
        float sw = sinf(a->anim + ph[i]) * 0.45f * moving;
        vec3 hip = v3(lx[i] * r * 0.55f, y - r * 0.3f, lz[i] * L);
        vec3 knee = v3_add(hip, v3(0, -leg * 0.5f, -sinf(sw) * leg * 0.5f));
        vec3 foot = v3_add(knee, v3(0, -leg * 0.5f + 0.02f, -sinf(sw * 1.4f) * leg * 0.45f));
        mtn_mesh_cylinder(m, xf, hip, knee, r * 0.22f, r * 0.15f, 4, &body, 0);
        mtn_mesh_cylinder(m, xf, knee, foot, r * 0.13f, r * 0.1f, 4, &dark, 0);
    }
    if (s->tail > 0.0f) {
        vec3 t0 = v3(0, y + r * 0.3f, L * 0.48f);
        mtn_mesh_cylinder(m, xf, t0, v3_add(t0, v3(0, -s->tail * 0.6f, s->tail)), r * 0.2f, r * 0.08f, 4, &body, 0);
    }
}

static void build_bird(const mtn_animal_species *s, const mtn_animal *a, mtn_mesh *m) {
    mat4 xf = m4_mul(m4_translate(a->pos), m4_rotate_y(-a->yaw));
    float span = s->size;
    float flap = s->kind == MTN_ANIMAL_SOARING ? 0.15f * sinf(a->anim) : 0.8f * sinf(a->anim);
    mtn_material b = mtn_mat(col(s->color, 1.0f), NO_TEXTURE, 0, 1.0f);
    mtn_mesh_cylinder(m, xf, v3(0, 0, span * 0.25f), v3(0, 0, -span * 0.25f), span * 0.06f, span * 0.04f, 4, &b, 1);
    for (int sd = -1; sd <= 1; sd += 2) {
        float tipy = sinf(flap) * span * 0.5f;
        vec3 root0 = v3(0, 0, -span * 0.08f), root1 = v3(0, 0, span * 0.1f);
        vec3 tip0 = v3((float)sd * span * 0.5f, tipy, span * 0.02f), tip1 = v3((float)sd * span * 0.5f, tipy, span * 0.1f);
        if (sd > 0) {
            mtn_mesh_quad(m, xf, root0, tip0, tip1, root1, &b, v2(0, 0), v2(1, 1));
            mtn_mesh_quad(m, xf, root1, tip1, tip0, root0, &b, v2(0, 0), v2(1, 1));
        } else {
            mtn_mesh_quad(m, xf, root1, tip1, tip0, root0, &b, v2(0, 0), v2(1, 1));
            mtn_mesh_quad(m, xf, root0, tip0, tip1, root1, &b, v2(0, 0), v2(1, 1));
        }
    }
}

void mtn_fauna_build(const mtn_fauna *f, vec3 cam, float max_dist, mtn_mesh *solid, mtn_mesh *sprites) {
    for (int i = 0; i < MTN_MAX_ANIMALS; i++) {
        const mtn_animal *a = &f->animals[i];
        if (!a->alive || v3_dist(a->pos, cam) > max_dist) continue;
        const mtn_animal_species *s = &f->species[a->species];
        switch (s->kind) {
        case MTN_ANIMAL_QUADRUPED: build_quadruped(s, a, solid); break;
        case MTN_ANIMAL_BIRD:
        case MTN_ANIMAL_SOARING: build_bird(s, a, solid); break;
        case MTN_ANIMAL_BUTTERFLY: {
            int frame = ((int)(a->anim * 10.0f)) & 1;
            int alpha = (int)(a->fade * 255.0f);
            mtn_material mt = mtn_mat(mtn_rgb(255, 255, 255) & (0x00FFFFFFu | ((uint32_t)alpha << 24)),
                                      s->sprite_tile + frame, 0, 1.0f);
            mtn_mesh_billboard(sprites, a->pos, s->size, s->size, 0.0f, &mt, 0.0f);
            break;
        }
        case MTN_ANIMAL_FIREFLY: {
            float blink = mtn_clampf(sinf(a->anim * 3.0f + a->phase) * 1.5f, 0.0f, 1.0f) * a->fade;
            int c = (int)(blink * 255.0f);
            mtn_material mt = mtn_mat(mtn_rgb(c, c, c), s->sprite_tile, MTN_VF_EMISSIVE, 1.0f);
            mtn_mesh_billboard(sprites, a->pos, s->size, s->size, 0.0f, &mt, 0.0f);
            break;
        }
        }
    }
}
