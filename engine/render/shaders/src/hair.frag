#version 450

// Hair strand fragment shader — Kajiya-Kay lighting model.

layout(location = 0) in vec3 v_world_pos;
layout(location = 1) in vec3 v_tangent;
layout(location = 2) in float v_t;

layout(location = 12) uniform vec3 u_camera_pos;
layout(location = 13) uniform vec3 u_light_dir;
layout(location = 14) uniform vec3 u_light_color;
layout(location = 15) uniform float u_light_intensity;
layout(location = 16) uniform float u_ambient_intensity;
layout(location = 17) uniform vec4 u_root_color;
layout(location = 18) uniform vec4 u_tip_color;
layout(location = 19) uniform float u_opacity;
layout(location = 20) uniform float u_spec_primary;
layout(location = 21) uniform float u_spec_secondary;
layout(location = 22) uniform float u_spec_strength1;
layout(location = 23) uniform float u_spec_strength2;
layout(location = 24) uniform vec3 u_spec_color;

layout(location = 0) out vec4 FragColor;

// Kajiya-Kay diffuse: sin(T, L)
float KajiyaDiffuse(vec3 T, vec3 L) {
    float TdotL = dot(T, L);
    return sqrt(max(0.0, 1.0 - TdotL * TdotL));
}

// Kajiya-Kay specular: sin(T, L) * sin(T, V) - cos(T, L) * cos(T, V)
float KajiyaSpecular(vec3 T, vec3 L, vec3 V, float power) {
    float TdotL = dot(T, L);
    float TdotV = dot(T, V);
    float sinTL = sqrt(max(0.0, 1.0 - TdotL * TdotL));
    float sinTV = sqrt(max(0.0, 1.0 - TdotV * TdotV));
    float cosAngle = sinTL * sinTV - TdotL * TdotV;
    return pow(max(0.0, cosAngle), power);
}

void main() {
    vec3 T = normalize(v_tangent);
    vec3 L = normalize(-u_light_dir);
    vec3 V = normalize(u_camera_pos - v_world_pos);

    // Interpolate color from root to tip
    float t = clamp(v_t, 0.0, 1.0);
    vec4 hair_color = mix(u_root_color, u_tip_color, t);

    // Kajiya-Kay lighting
    float diffuse = KajiyaDiffuse(T, L);
    float spec1 = KajiyaSpecular(T, L, V, u_spec_primary) * u_spec_strength1;
    float spec2 = KajiyaSpecular(T, L, V, u_spec_secondary) * u_spec_strength2;

    vec3 lit = hair_color.rgb * (u_ambient_intensity + diffuse * u_light_intensity) * u_light_color
             + (spec1 + spec2) * u_spec_color * u_light_color * u_light_intensity;

    FragColor = vec4(lit, hair_color.a * u_opacity);
}
