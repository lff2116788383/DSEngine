// Hand-written gen.h for hair shader — SSBO prevents GLSL 330 cross-compile.
// Canonical source: engine/render/shaders/src/hair.frag (GLSL 450 + explicit locations)
// This runtime version uses GLSL 430 without layout locations — uniform locations
// are queried via glGetUniformLocation at init time in gl_draw_executor_fx.cpp.
#pragma once

namespace dse {
namespace render {
namespace generated_shaders {

static const char* const khair_frag_glsl430 = R"(
#version 430 core

in vec3 v_world_pos;
in vec3 v_tangent;
in float v_t;

uniform vec3 u_camera_pos;
uniform vec3 u_light_dir;
uniform vec3 u_light_color;
uniform float u_light_intensity;
uniform float u_ambient_intensity;
uniform vec4 u_root_color;
uniform vec4 u_tip_color;
uniform float u_opacity;
uniform float u_spec_primary;
uniform float u_spec_secondary;
uniform float u_spec_strength1;
uniform float u_spec_strength2;
uniform vec3 u_spec_color;

out vec4 FragColor;

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
)";

} // namespace generated_shaders
} // namespace render
} // namespace dse
