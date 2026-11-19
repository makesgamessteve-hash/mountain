// Mountain - lake water (fragment): fresnel sky reflection, ripples, sun glint. MIT License.
in vec3 v_world;
in float v_fog;
uniform vec3 u_cam_pos;
uniform float u_time;
uniform vec3 u_zenith;
uniform vec3 u_horizon;
uniform vec3 u_sun_dir;
uniform vec3 u_light_color;
uniform vec3 u_ambient;
uniform vec3 u_fog_color;
uniform float u_snow;   // frozen in deep winter
out vec4 o_color;
void main() {
    vec2 p = v_world.xz;
    float r = sin(p.x * 0.9 + u_time * 1.3) * 0.5 + sin(p.y * 1.1 - u_time * 1.1) * 0.5 + sin((p.x + p.y) * 2.3 + u_time * 2.0) * 0.25;
    vec3 n = normalize(vec3(r * 0.06, 1.0, r * 0.05));
    vec3 v = normalize(u_cam_pos - v_world);
    float fres = pow(1.0 - max(dot(n, v), 0.0), 3.0);
    vec3 refl = reflect(-v, n);
    vec3 sky = mix(u_horizon, u_zenith, clamp(refl.y * 2.0, 0.0, 1.0));
    vec3 deep = vec3(0.05, 0.12, 0.14) * (u_ambient + u_light_color * 0.6);
    vec3 c = mix(deep, sky, 0.25 + 0.65 * fres);
    float glint = pow(max(dot(refl, u_sun_dir), 0.0), 120.0);
    c += u_light_color * glint * 1.5;
    c = mix(c, vec3(0.8, 0.85, 0.9) * (u_ambient + u_light_color * 0.7), u_snow);
    c = mix(c, u_fog_color, v_fog);
    o_color = vec4(c, mix(0.82, 1.0, u_snow));
}
