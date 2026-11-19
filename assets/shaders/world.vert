// Mountain - generic world shader: meshes, voxels and camera-facing sprites. MIT License.
// The engine prepends the #version line (330 core on desktop, 300 es on the web).
in vec3 a_pos;
in vec3 a_nrm;
in vec2 a_uv;
in vec4 a_col;
in vec4 a_info; // tile, flags, extra (species), ao

uniform mat4 u_viewproj;
uniform mat4 u_model;
uniform vec3 u_cam_pos;
uniform vec3 u_cam_right;
uniform vec3 u_cam_up;
uniform float u_time;
uniform vec3 u_wind;            // dir.x, dir.z, strength
uniform vec3 u_light_dir;
uniform vec3 u_light_color;
uniform vec3 u_ambient;
uniform float u_fog_density;
uniform float u_snow_height;    // metres above which upward faces are snowy
uniform float u_emissive;
uniform vec4 u_species[48];     // rgb leaf colour, a = leaf amount / bloom
uniform float u_blossom[48];
uniform int u_interior;
uniform int u_light_count;
uniform vec4 u_lights_pos[9];   // xyz, radius
uniform vec4 u_lights_col[9];   // rgb, flicker

out vec2 v_uv;
out vec4 v_color;
out float v_fog;
out float v_snow;
flat out int v_tile;
flat out int v_flags;

const int F_EMISSIVE = 1;
const int F_WIND = 2;
const int F_SNOW = 4;
const int F_LEAF = 8;
const int F_BLOSSOM = 16;
const int F_BILLBOARD = 32;
const int F_UPRIGHT = 64;

void main() {
    int tile = int(a_info.x + 0.5);
    int flags = int(a_info.y + 0.5);
    int sp = int(a_info.z + 0.5);
    float ao = a_info.w / 255.0;
    vec3 pos = (u_model * vec4(a_pos, 1.0)).xyz;
    vec3 nrm = normalize(mat3(u_model) * a_nrm);
    vec4 col = a_col;
    float sway_w = 0.4;
    if ((flags & F_BILLBOARD) != 0) {
        vec2 corner = a_nrm.xy;
        sway_w = a_nrm.z;
        float amount = 1.0;
        if ((flags & F_LEAF) != 0) amount = u_species[sp].a;
        if ((flags & F_BLOSSOM) != 0) amount = u_blossom[sp];
        corner *= amount;
        vec3 right = u_cam_right;
        vec3 up = u_cam_up;
        if ((flags & F_UPRIGHT) != 0) {
            right = normalize(vec3(u_cam_right.x, 0.0, u_cam_right.z) + vec3(1e-4, 0.0, 0.0));
            up = vec3(0.0, 1.0, 0.0);
        }
        pos += right * corner.x + up * corner.y;
        vec3 to_cam = normalize(u_cam_pos - pos);
        nrm = normalize(vec3(0.0, 1.0, 0.0) * 0.55 + to_cam * 0.45 + vec3(0.0, corner.y, 0.0) * 0.15);
        if (amount <= 0.01) pos = vec3(0.0, -1e6, 0.0);
    }
    if ((flags & F_WIND) != 0) {
        float s = sin(u_time * 1.7 + pos.x * 0.31 + pos.z * 0.23) + 0.4 * sin(u_time * 4.1 + pos.x * 1.3);
        pos.xz += u_wind.xy * s * u_wind.z * sway_w * 0.35;
    }
    if ((flags & F_LEAF) != 0) col.rgb *= u_species[sp].rgb;

    vec3 light;
    if ((flags & F_EMISSIVE) != 0) {
        light = vec3(u_emissive);
    } else {
        float ndl = max(dot(nrm, u_light_dir), 0.0);
        float sky = 0.6 + 0.4 * nrm.y;
        light = u_ambient * sky * ao + u_light_color * ndl * mix(1.0, ao, 0.5);
        if (u_interior == 1) {
            light *= 0.35;
            for (int i = 0; i < 9; i++) {
                if (i >= u_light_count) break;
                vec3 d = u_lights_pos[i].xyz - pos;
                float dist = length(d);
                float att = clamp(1.0 - dist / u_lights_pos[i].w, 0.0, 1.0);
                att *= att;
                float fl = 1.0 - u_lights_col[i].a * 0.25 * (0.5 + 0.5 * sin(u_time * 13.0 + float(i) * 1.7) * sin(u_time * 7.3));
                float lam = max(dot(nrm, d / max(dist, 1e-3)), 0.0) * 0.75 + 0.25;
                light += u_lights_col[i].rgb * att * lam * fl * ao;
            }
        }
    }
    v_snow = 0.0;
    if ((flags & F_SNOW) != 0 && u_interior == 0) {
        float up = (flags & F_BILLBOARD) != 0 ? step(0.0, a_nrm.y) * 0.8 : smoothstep(0.35, 0.8, nrm.y);
        v_snow = up * smoothstep(u_snow_height - 15.0, u_snow_height + 5.0, pos.y);
    }
    v_color = vec4(col.rgb * light, col.a);
    float dist = length(pos - u_cam_pos);
    v_fog = u_interior == 1 ? 0.0 : 1.0 - exp(-dist * u_fog_density);
    v_uv = a_uv;
    v_tile = tile;
    v_flags = flags;
    gl_Position = u_viewproj * vec4(pos, 1.0);
}
