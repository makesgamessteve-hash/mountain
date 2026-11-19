/* Mountain engine - vector/matrix math. MIT License. */
#ifndef MTN_MATH_H
#define MTN_MATH_H

#include <stdint.h>

#define MTN_PI 3.14159265358979323846f
#define MTN_TAU 6.28318530717958647692f
#define MTN_DEG2RAD (MTN_PI / 180.0f)

typedef struct { float x, y; } vec2;
typedef struct { float x, y, z; } vec3;
typedef struct { float x, y, z, w; } vec4;
/* Column-major 4x4 matrix (OpenGL convention): m[col*4 + row]. */
typedef struct { float m[16]; } mat4;

float mtn_clampf(float v, float lo, float hi);
float mtn_lerpf(float a, float b, float t);
float mtn_smoothstep(float e0, float e1, float x);
float mtn_fract(float x);
float mtn_wrap_angle(float a); /* wrap to [-pi, pi) */
float mtn_minf(float a, float b);
float mtn_maxf(float a, float b);
int mtn_clampi(int v, int lo, int hi);

vec2 v2(float x, float y);
vec3 v3(float x, float y, float z);
vec4 v4(float x, float y, float z, float w);
vec3 v3_add(vec3 a, vec3 b);
vec3 v3_sub(vec3 a, vec3 b);
vec3 v3_scale(vec3 a, float s);
vec3 v3_mul(vec3 a, vec3 b);
float v3_dot(vec3 a, vec3 b);
vec3 v3_cross(vec3 a, vec3 b);
float v3_len(vec3 a);
vec3 v3_norm(vec3 a);
vec3 v3_lerp(vec3 a, vec3 b, float t);
float v3_dist(vec3 a, vec3 b);
float v3_dist_xz(vec3 a, vec3 b);
vec3 v3_rotate_y(vec3 v, float angle);

mat4 m4_identity(void);
mat4 m4_mul(mat4 a, mat4 b);
mat4 m4_translate(vec3 t);
mat4 m4_scale(vec3 s);
mat4 m4_rotate_x(float a);
mat4 m4_rotate_y(float a);
mat4 m4_rotate_z(float a);
mat4 m4_perspective(float fovy_rad, float aspect, float znear, float zfar);
mat4 m4_ortho(float l, float r, float b, float t, float n, float f);
mat4 m4_look_at(vec3 eye, vec3 target, vec3 up);
/* Basis from origin + axes (columns). */
mat4 m4_from_basis(vec3 origin, vec3 x, vec3 y, vec3 z);
mat4 m4_inverse(mat4 m);
mat4 m4_transpose(mat4 m);
vec3 m4_mul_point(mat4 m, vec3 p);
vec3 m4_mul_dir(mat4 m, vec3 d);
vec4 m4_mul_v4(mat4 m, vec4 v);
/* Lengyel oblique near-plane clipping: plane given in view space (ax+by+cz+d=0),
 * the visible side is where the plane equation is positive. */
mat4 m4_oblique_near(mat4 proj, vec4 plane_view);

/* Frustum culling. Planes point inward. */
typedef struct { vec4 p[6]; } mtn_frustum;
mtn_frustum mtn_frustum_from(mat4 viewproj);
int mtn_frustum_sphere(const mtn_frustum *f, vec3 c, float r);
int mtn_frustum_aabb(const mtn_frustum *f, vec3 mn, vec3 mx);

/* Portals: a frame's +z ("forward") points from the viewer's side through the
 * opening. Returns the transform that maps space A into space B so that looking
 * through portal A shows what lies behind portal B. */
mat4 mtn_portal_frame(vec3 origin, vec3 forward);
mat4 mtn_portal_transform(mat4 frame_a, mat4 frame_b);

uint32_t mtn_pack_rgba(float r, float g, float b, float a);

#endif
