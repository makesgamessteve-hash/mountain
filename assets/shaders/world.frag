// Mountain - generic world shader (fragment). MIT License.
in vec2 v_uv;
in vec4 v_color;
in float v_fog;
in float v_snow;
flat in int v_tile;
flat in int v_flags;

uniform sampler2D u_atlas;
uniform int u_single;          // 1: u_atlas is a single image (photos on the wall)
uniform float u_alpha_test;
uniform vec3 u_fog_color;
uniform vec3 u_snow_light;

out vec4 o_color;

void main() {
    vec4 tex = vec4(1.0);
    if (u_single == 1) {
        tex = texture(u_atlas, clamp(v_uv, 0.0, 1.0));
    } else if (v_tile != 255) {
        vec2 uv = (v_flags & 128) != 0 ? clamp(v_uv, 0.0, 1.0) : fract(v_uv);
        vec2 cell = vec2(float(v_tile % 8), float(v_tile / 8));
        tex = texture(u_atlas, (cell + uv * 0.996 + 0.002) / 8.0);
    }
    if (tex.a < u_alpha_test) discard;
    vec3 c = tex.rgb * v_color.rgb;
    c = mix(c, u_snow_light * (0.85 + 0.15 * tex.r), v_snow);
    c = mix(c, u_fog_color, v_fog);
    o_color = vec4(c, tex.a * v_color.a);
}
