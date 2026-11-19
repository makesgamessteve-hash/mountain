// Mountain - rain and snow particles wrapped in a box around the camera. MIT License.
in vec3 a_pos;
in vec3 a_nrm;
in vec4 a_info;
uniform mat4 u_viewproj;
uniform vec3 u_cam_pos;
uniform vec3 u_cam_right;
uniform float u_time;
uniform float u_amount;
uniform int u_snow;
uniform vec3 u_wind;
uniform vec3 u_light;
out vec2 v_uv;
out float v_alpha;
out vec3 v_light;
void main() {
    float box = 40.0;
    float fall = u_snow == 1 ? 1.3 : 11.0;
    vec3 p = a_pos * box;
    p.y -= u_time * fall * (0.8 + 0.4 * fract(a_pos.x * 7.0));
    p.xz += u_wind.xy * u_wind.z * u_time * (u_snow == 1 ? 1.8 : 3.0);
    if (u_snow == 1) p.xz += vec2(sin(u_time * 1.3 + a_pos.z * 40.0), cos(u_time * 1.1 + a_pos.x * 40.0)) * 0.4;
    p = mod(p - u_cam_pos, box) + u_cam_pos - box * 0.5;
    float idx = a_info.z * 256.0 + a_info.w;
    float on = step(idx / 65535.0, u_amount);
    vec2 corner = a_nrm.xy;
    vec3 right = normalize(vec3(u_cam_right.x, 0.0, u_cam_right.z) + 1e-4);
    vec3 world = p + right * corner.x * (u_snow == 1 ? 0.07 : 0.025) + vec3(0.0, corner.y, 0.0) * (u_snow == 1 ? 0.07 : 0.45);
    v_uv = a_nrm.xy * 0.5 + 0.5;
    v_uv.y = 1.0 - v_uv.y;
    float dist = length(p - u_cam_pos);
    v_alpha = on * (1.0 - smoothstep(12.0, 20.0, dist));
    v_light = u_light;
    gl_Position = u_viewproj * vec4(world, 1.0);
    if (on < 0.5) gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
}
