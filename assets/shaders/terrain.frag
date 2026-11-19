// Mountain - terrain shader (fragment): seasonal grass, rock, trail dirt and snow. MIT License.
in vec3 v_world;
in vec3 v_nrm;
in vec4 v_base;
in vec4 v_info;
in vec3 v_light;
in float v_fog;

uniform sampler2D u_detail;     // 4 tiles: grass, rock, dirt, snow
uniform vec3 u_grass_color;
uniform vec3 u_dry_grass;
uniform float u_snow_height;
uniform float u_fresh_snow;
uniform vec3 u_fog_color;
uniform vec3 u_snow_light;

out vec4 o_color;

vec3 detail(int t, vec2 uv) {
    return texture(u_detail, vec2((float(t) + fract(uv.x) * 0.996 + 0.002) * 0.25, fract(uv.y))).rgb * 2.0;
}

void main() {
    vec3 n = normalize(v_nrm);
    float steep = 1.0 - smoothstep(0.55, 0.8, n.y);
    vec2 flat_uv = v_world.xz * 0.35;
    vec2 cliff_uv = vec2(v_world.x + v_world.z, v_world.y) * 0.3;
    float moist = v_info.y;
    vec3 grass = mix(u_dry_grass, u_grass_color, smoothstep(0.15, 0.7, moist)) * detail(0, flat_uv);
    vec3 rock = v_base.rgb * detail(1, mix(flat_uv, cliff_uv, steep));
    float veg = v_base.a * (1.0 - steep);
    vec3 c = mix(rock, grass, veg);
    float trail = v_info.x;
    c = mix(c, vec3(0.42, 0.34, 0.25) * detail(2, flat_uv), smoothstep(0.2, 0.8, trail));
    float noise = (detail(3, v_world.xz * 0.05).r - 1.0) * 25.0;
    float snow = smoothstep(u_snow_height - 8.0, u_snow_height + 8.0, v_world.y + noise) * smoothstep(0.45, 0.7, n.y);
    snow = max(snow, u_fresh_snow * smoothstep(0.35, 0.6, n.y));
    c = mix(c, vec3(0.93, 0.95, 1.0) * detail(3, flat_uv), snow);
    c *= v_light;
    c = mix(c, u_fog_color, v_fog);
    o_color = vec4(c, 1.0);
}
