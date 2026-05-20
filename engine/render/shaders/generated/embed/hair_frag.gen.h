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

// Vulkan GLSL 450 — 从 VS 共享的 HairUBO 读取参数
static const char* const khair_frag_glsl450 = R"(
#version 450

layout(location = 0) in vec3 v_world_pos;
layout(location = 1) in vec3 v_tangent;
layout(location = 2) in float v_t;

layout(std140, set = 0, binding = 0) uniform HairUBO {
    mat4 model;
    mat4 view;
    mat4 projection;
    vec4 camera_pos;      // xyz=pos, w=unused
    vec4 light_dir_int;   // xyz=dir, w=intensity
    vec4 light_color_amb; // xyz=color, w=ambient
    vec4 root_color;
    vec4 tip_color;
    vec4 spec_params;     // x=primary, y=secondary, z=strength1, w=strength2
    vec4 spec_color_opa;  // xyz=spec_color, w=opacity
};

layout(location = 0) out vec4 FragColor;

float KajiyaDiffuse(vec3 T, vec3 L) {
    float TdotL = dot(T, L);
    return sqrt(max(0.0, 1.0 - TdotL * TdotL));
}

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
    vec3 L = normalize(-light_dir_int.xyz);
    vec3 V = normalize(camera_pos.xyz - v_world_pos);

    float t = clamp(v_t, 0.0, 1.0);
    vec4 hair_color = mix(root_color, tip_color, t);

    float diffuse = KajiyaDiffuse(T, L);
    float spec1 = KajiyaSpecular(T, L, V, spec_params.x) * spec_params.z;
    float spec2 = KajiyaSpecular(T, L, V, spec_params.y) * spec_params.w;

    vec3 lit = hair_color.rgb * (light_color_amb.w + diffuse * light_dir_int.w) * light_color_amb.xyz
             + (spec1 + spec2) * spec_color_opa.xyz * light_color_amb.xyz * light_dir_int.w;

    FragColor = vec4(lit, hair_color.a * spec_color_opa.w);
}
)";

// HLSL 片元着色器
static const char* const khair_frag_hlsl = R"(
cbuffer HairPSCB : register(b1) {
    float3 g_light_dir;
    float  g_light_intensity;
    float3 g_light_color;
    float  g_ambient_intensity;
    float4 g_root_color;
    float4 g_tip_color;
    float  g_opacity;
    float  g_spec_primary;
    float  g_spec_secondary;
    float  g_spec_strength1;
    float  g_spec_strength2;
    float3 g_spec_color;
};

cbuffer HairVSCB : register(b0) {
    float4x4 g_model;
    float4x4 g_view;
    float4x4 g_projection;
    float3   g_camera_pos;
    float    _pad0;
};

struct PSInput {
    float4 pos       : SV_POSITION;
    float3 world_pos : TEXCOORD0;
    float3 tangent   : TEXCOORD1;
    float  t_param   : TEXCOORD2;
};

float KajiyaDiffuse(float3 T, float3 L) {
    float TdotL = dot(T, L);
    return sqrt(max(0.0, 1.0 - TdotL * TdotL));
}

float KajiyaSpecular(float3 T, float3 L, float3 V, float power) {
    float TdotL = dot(T, L);
    float TdotV = dot(T, V);
    float sinTL = sqrt(max(0.0, 1.0 - TdotL * TdotL));
    float sinTV = sqrt(max(0.0, 1.0 - TdotV * TdotV));
    float cosAngle = sinTL * sinTV - TdotL * TdotV;
    return pow(max(0.0, cosAngle), power);
}

float4 main(PSInput input) : SV_TARGET {
    float3 T = normalize(input.tangent);
    float3 L = normalize(-g_light_dir);
    float3 V = normalize(g_camera_pos - input.world_pos);

    float t = saturate(input.t_param);
    float4 hair_color = lerp(g_root_color, g_tip_color, t);

    float diffuse = KajiyaDiffuse(T, L);
    float spec1 = KajiyaSpecular(T, L, V, g_spec_primary) * g_spec_strength1;
    float spec2 = KajiyaSpecular(T, L, V, g_spec_secondary) * g_spec_strength2;

    float3 lit = hair_color.rgb * (g_ambient_intensity + diffuse * g_light_intensity) * g_light_color
               + (spec1 + spec2) * g_spec_color * g_light_color * g_light_intensity;

    return float4(lit, hair_color.a * g_opacity);
}
)";

} // namespace generated_shaders
} // namespace render
} // namespace dse
