// Mountain - portal stencil quads (house door and room windows). MIT License.
in vec3 a_pos;
uniform mat4 u_viewproj;
uniform int u_far;
void main() {
    gl_Position = u_viewproj * vec4(a_pos, 1.0);
    if (u_far == 1) gl_Position.z = gl_Position.w * 0.99999;
}
