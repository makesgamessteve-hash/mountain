// Mountain - 2D interface quads. MIT License.
in vec3 a_pos;
in vec2 a_uv;
in vec4 a_col;
uniform vec2 u_screen;
out vec2 v_uv;
out vec4 v_col;
void main() {
    v_uv = a_uv;
    v_col = a_col;
    vec2 p = a_pos.xy / u_screen * 2.0 - 1.0;
    gl_Position = vec4(p.x, -p.y, 0.0, 1.0);
}
