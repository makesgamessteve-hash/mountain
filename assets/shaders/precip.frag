// Mountain - rain and snow (fragment). MIT License.
in vec2 v_uv;
in float v_alpha;
in vec3 v_light;
uniform sampler2D u_atlas;
uniform int u_snow;
out vec4 o_color;
void main() {
    float tile = u_snow == 1 ? 41.0 : 40.0;
    vec2 cell = vec2(mod(tile, 8.0), floor(tile / 8.0));
    vec4 t = texture(u_atlas, (cell + clamp(v_uv, 0.02, 0.98)) / 8.0);
    o_color = vec4(t.rgb * v_light, t.a * v_alpha * (u_snow == 1 ? 1.0 : 0.6));
    if (o_color.a < 0.02) discard;
}
