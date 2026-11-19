// Mountain - terrain shader. MIT License.
in vec3 a_pos;
in vec3 a_nrm;
in vec2 a_uv;
in vec4 a_col;  // base colour (rgb), vegetation amount (a)
in vec4 a_info; // trail, moisture, biome, ambient occlusion

uniform mat4 u_viewproj;
uniform vec3 u_cam_pos;
uniform vec3 u_light_dir;
uniform vec3 u_light_color;
uniform vec3 u_ambient;
uniform float u_fog_density;

out vec3 v_world;
out vec3 v_nrm;
out vec4 v_base;
out vec4 v_info;
out vec3 v_light;
out float v_fog;

void main() {
    vec3 n = normalize(a_nrm);
    float ao = a_info.w / 255.0;
    float ndl = max(dot(n, u_light_dir), 0.0);
    v_light = u_ambient * (0.55 + 0.45 * n.y) * ao + u_light_color * ndl * (0.6 + 0.4 * ao);
    v_world = a_pos;
    v_nrm = n;
    v_base = a_col;
    v_info = a_info / 255.0;
    float dist = length(a_pos - u_cam_pos);
    v_fog = 1.0 - exp(-dist * u_fog_density);
    gl_Position = u_viewproj * vec4(a_pos, 1.0);
}
