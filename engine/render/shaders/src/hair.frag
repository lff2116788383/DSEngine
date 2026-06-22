#version 450
#extension GL_ARB_separate_shader_objects : enable

// Hair strand fragment shader — Kajiya-Kay lighting model.
//
// 与 hair.vert 共享同一组合 UBO（set0.binding0，全字段声明须逐字一致以保证 std140 偏移对齐）；
// 本阶段仅读取 camera/light/color/material 字段，model/view/projection 不用但须保留占位。

layout(location = 0) in vec3 v_world_pos;
layout(location = 1) in vec3 v_tangent;
layout(location = 2) in float v_t;

layout(std140, set = 0, binding = 0) uniform HairUniforms {
    mat4 u_model;
    mat4 u_view;
    mat4 u_projection;
    vec4 u_camera_pos;     // xyz = camera world pos
    vec4 u_light_dir;      // xyz = light direction
    vec4 u_light_color;    // xyz = light color
    vec4 u_root_color;     // rgba
    vec4 u_tip_color;      // rgba
    vec4 u_spec_color;     // xyz = specular color
    vec4 u_params0;        // x=light_intensity y=ambient_intensity z=opacity w=spec_primary
    vec4 u_params1;        // x=spec_secondary y=spec_strength1 z=spec_strength2 w=pad
};

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
    vec3 L = normalize(-u_light_dir.xyz);
    vec3 V = normalize(u_camera_pos.xyz - v_world_pos);

    float light_intensity   = u_params0.x;
    float ambient_intensity = u_params0.y;
    float opacity           = u_params0.z;
    float spec_primary      = u_params0.w;
    float spec_secondary    = u_params1.x;
    float spec_strength1    = u_params1.y;
    float spec_strength2    = u_params1.z;

    // Interpolate color from root to tip
    float t = clamp(v_t, 0.0, 1.0);
    vec4 hair_color = mix(u_root_color, u_tip_color, t);

    // Kajiya-Kay lighting
    float diffuse = KajiyaDiffuse(T, L);
    float spec1 = KajiyaSpecular(T, L, V, spec_primary) * spec_strength1;
    float spec2 = KajiyaSpecular(T, L, V, spec_secondary) * spec_strength2;

    vec3 lit = hair_color.rgb * (ambient_intensity + diffuse * light_intensity) * u_light_color.xyz
             + (spec1 + spec2) * u_spec_color.xyz * u_light_color.xyz * light_intensity;

    FragColor = vec4(lit, hair_color.a * opacity);
}
