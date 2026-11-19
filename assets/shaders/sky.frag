// Mountain - sky: gradient, sun, moons with phases, stars, milky way, clouds. MIT License.
in vec2 v_ndc;
uniform mat4 u_inv_viewproj;   // rotation-only view
uniform vec3 u_zenith;
uniform vec3 u_horizon;
uniform vec3 u_fog_color;
uniform vec3 u_sun_dir;
uniform vec3 u_sun_color;
uniform float u_sun_size;
uniform float u_sun_vis;
uniform float u_sunset;
uniform int u_moon_count;
uniform vec4 u_moon_dir[4];    // xyz dir, w angular size
uniform vec4 u_moon_col[4];
uniform sampler2D u_moon_tex;
uniform float u_stars;
uniform mat4 u_star_rot;
uniform float u_cloud_cover;
uniform vec3 u_cloud_color;
uniform float u_time;
uniform vec2 u_wind;
out vec4 o_color;

float hash3(vec3 p) {
    p = fract(p * 0.3183099 + vec3(0.71, 0.113, 0.419));
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}
float hash2(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash2(i), hash2(i + vec2(1, 0)), f.x), mix(hash2(i + vec2(0, 1)), hash2(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) { s += a * vnoise(p); p = p * 2.03 + vec2(1.7, 9.2); a *= 0.5; }
    return s;
}

void main() {
    vec4 wp = u_inv_viewproj * vec4(v_ndc, 1.0, 1.0);
    vec3 d = normalize(wp.xyz / wp.w);
    float up = d.y;
    vec3 col = mix(u_horizon, u_zenith, pow(clamp(up, 0.0, 1.0), 0.55));
    // warm glow around the sun near sunrise/sunset
    float cs = dot(d, u_sun_dir);
    col += vec3(1.0, 0.55, 0.3) * pow(max(cs, 0.0), 6.0) * u_sunset * 0.45 * smoothstep(-0.2, 0.3, up + 0.2);
    // stars and milky way
    if (u_stars > 0.01 && up > -0.05) {
        vec3 sd = (u_star_rot * vec4(d, 0.0)).xyz;
        float band = exp(-pow(dot(sd, normalize(vec3(0.3, 0.2, 0.93))) / 0.22, 2.0));
        float h = hash3(floor(sd * 220.0));
        float thresh = 0.9965 - band * 0.004;
        float star = smoothstep(thresh, 1.0, h) * 3.0;
        star *= 0.7 + 0.3 * sin(u_time * 3.0 + h * 100.0);
        float mw = band * (0.25 + 0.75 * fbm(vec2(atan(sd.z, sd.x), sd.y) * 6.0)) * 0.10;
        col += (vec3(star) + vec3(0.75, 0.8, 1.0) * mw) * u_stars * smoothstep(-0.05, 0.15, up);
    }
    // sun disc and halo
    float disc = smoothstep(cos(u_sun_size * 1.15), cos(u_sun_size * 0.85), cs);
    col += u_sun_color * (disc * 2.5 + pow(max(cs, 0.0), 300.0) * 0.6 + pow(max(cs, 0.0), 20.0) * 0.12) * u_sun_vis;
    // moons, lit from the real sun direction so phases are correct
    for (int i = 0; i < 4; i++) {
        if (i >= u_moon_count) break;
        vec3 md = u_moon_dir[i].xyz;
        float cm = dot(d, md);
        if (cm <= 0.0) continue;
        vec3 right = normalize(cross(md, vec3(0.0, 1.0, 0.0001)));
        vec3 mup = cross(right, md);
        vec3 q = d / cm - md;
        float tsz = tan(u_moon_dir[i].w);
        vec2 uv = vec2(dot(q, right), dot(q, mup)) / tsz;
        float r2 = dot(uv, uv);
        if (r2 < 1.0) {
            vec3 n = right * uv.x + mup * uv.y - md * sqrt(1.0 - r2);
            float lit = smoothstep(-0.08, 0.12, dot(n, u_sun_dir));
            vec4 tex = texture(u_moon_tex, uv * vec2(0.484, -0.484) + 0.5);
            vec3 mc = tex.rgb * u_moon_col[i].rgb * (0.04 + 1.1 * lit);
            float vis = smoothstep(-0.02, 0.06, md.y) * tex.a;
            col = mix(col, col * 0.4 + mc, vis);
        }
    }
    // cloud layer
    if (up > 0.0 && u_cloud_cover > 0.02) {
        vec2 cuv = d.xz / (up + 0.08) * 0.9 + u_wind * u_time * 0.01;
        float n = fbm(cuv);
        float cov = smoothstep(1.0 - u_cloud_cover * 0.95 - 0.05, 1.0 - u_cloud_cover * 0.95 + 0.35, n);
        float shade = 0.75 + 0.25 * smoothstep(0.3, 0.8, fbm(cuv * 2.0 + 3.0)) + 0.3 * pow(max(cs, 0.0), 4.0) * u_sun_vis;
        cov *= smoothstep(0.0, 0.18, up);
        col = mix(col, u_cloud_color * shade, cov * 0.95);
    }
    // below the horizon fade into the fog colour (distant haze)
    col = mix(col, u_fog_color, smoothstep(0.02, -0.12, up));
    o_color = vec4(col, 1.0);
}
