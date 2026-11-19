// Mountain - 2D interface (fragment). MIT License.
in vec2 v_uv;
in vec4 v_col;
uniform sampler2D u_tex;
out vec4 o_color;
void main() {
    vec4 t = texture(u_tex, v_uv);
    o_color = t * v_col;
    if (o_color.a < 0.01) discard;
}
