// Mountain - retro post process: reduced colour depth with 4x4 ordered dithering,
// photo filters, viewfinder overlay and shutter flash. Nearest upscaling. MIT License.
in vec2 v_uv;
uniform sampler2D u_scene;
uniform vec2 u_lowres;
uniform int u_filter;
uniform float u_flash;
uniform float u_viewfinder;
uniform float u_levels;
out vec4 o_color;

float bayer(vec2 p) {
    int x = int(mod(p.x, 4.0)), y = int(mod(p.y, 4.0));
    int i = x + y * 4;
    int m[16] = int[16](0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);
    return (float(m[i]) + 0.5) / 16.0;
}

vec3 apply_filter(vec3 c, int f) {
    float l = dot(c, vec3(0.299, 0.587, 0.114));
    if (f == 1) return vec3(l * 1.07 + 0.08, l * 0.88 + 0.03, l * 0.66);
    if (f == 2) { float k = (l - 0.5) * 1.15 + 0.5; return vec3(k); }
    if (f == 3) return vec3(l) + (c - vec3(l)) * 1.45;
    if (f == 4) return vec3(c.r * 1.08 + 0.055, c.g * 0.92 + 0.008, c.b * 0.86 + 0.12 * (1.0 - l));
    return c;
}

void main() {
    vec2 px = floor(v_uv * u_lowres);
    vec3 c = texture(u_scene, (px + 0.5) / u_lowres).rgb;
    c = clamp(apply_filter(c, u_filter), 0.0, 1.0);
    float b = bayer(px);
    c = floor(c * u_levels + b) / u_levels;
    if (u_viewfinder > 0.0) {
        vec2 q = v_uv * 2.0 - 1.0;
        c *= 1.0 - u_viewfinder * 0.35 * smoothstep(0.6, 1.4, length(q * vec2(1.0, 0.9)));
        vec2 t = abs(fract(v_uv * 3.0) - 0.5);
        float line = step(0.5 - 0.5 / u_lowres.x * 3.0, t.x) + step(0.5 - 0.5 / u_lowres.y * 3.0, t.y);
        c = mix(c, vec3(1.0), clamp(line, 0.0, 1.0) * 0.12 * u_viewfinder);
    }
    c = mix(c, vec3(1.0), u_flash);
    o_color = vec4(c, 1.0);
}
