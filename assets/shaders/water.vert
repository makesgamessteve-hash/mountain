// Mountain - lake water. MIT License.
in vec3 a_pos;
uniform mat4 u_viewproj;
uniform vec3 u_cam_pos;
uniform float u_fog_density;
out vec3 v_world;
out float v_fog;
void main() {
    v_world = a_pos;
    v_fog = 1.0 - exp(-length(a_pos - u_cam_pos) * u_fog_density);
    gl_Position = u_viewproj * vec4(a_pos, 1.0);
}
