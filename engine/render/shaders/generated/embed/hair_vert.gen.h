// Hand-written gen.h for hair shader — SSBO prevents GLSL 330 cross-compile.
// Canonical source: engine/render/shaders/src/hair.vert (GLSL 450 + explicit locations)
// This runtime version uses GLSL 430 without layout locations — uniform locations
// are queried via glGetUniformLocation at init time in gl_draw_executor_fx.cpp.
#pragma once

namespace dse {
namespace render {
namespace generated_shaders {

static const char* const khair_vert_glsl430 = R"(
#version 430 core

layout(std430, binding = 0) readonly buffer PositionBuf { vec4 positions[]; };
layout(std430, binding = 3) readonly buffer TangentBuf  { vec4 tangents[]; };

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

out vec3 v_world_pos;
out vec3 v_tangent;
out float v_t; // 0=root, 1=tip

void main() {
    vec4 pos = positions[gl_VertexID];
    vec4 tan = tangents[gl_VertexID];

    vec4 world_pos = u_model * vec4(pos.xyz, 1.0);
    v_world_pos = world_pos.xyz;
    v_tangent = normalize(mat3(u_model) * tan.xyz);
    v_t = 1.0 - tan.w; // tangent.w = thickness: 1 at root, 0 at tip

    gl_Position = u_projection * u_view * world_pos;
}
)";

// Vulkan GLSL 450 — UBO + SSBO（push constant 超过 128B 限制，改用 UBO）
static const char* const khair_vert_glsl450 = R"(
#version 450

layout(std430, set = 1, binding = 0) readonly buffer PositionBuf { vec4 positions[]; };
layout(std430, set = 1, binding = 1) readonly buffer TangentBuf  { vec4 tangents[]; };

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

layout(location = 0) out vec3 v_world_pos;
layout(location = 1) out vec3 v_tangent;
layout(location = 2) out float v_t;

void main() {
    vec4 pos = positions[gl_VertexIndex];
    vec4 tan = tangents[gl_VertexIndex];

    vec4 world_pos = model * vec4(pos.xyz, 1.0);
    v_world_pos = world_pos.xyz;
    v_tangent = normalize(mat3(model) * tan.xyz);
    v_t = 1.0 - tan.w;

    gl_Position = projection * view * world_pos;
}
)";

// HLSL — ByteAddressBuffer for SSBO, cbuffer for uniforms
static const char* const khair_vert_hlsl = R"(
cbuffer HairVSCB : register(b0) {
    float4x4 g_model;
    float4x4 g_view;
    float4x4 g_projection;
    float3   g_camera_pos;
    float    _pad0;
};

ByteAddressBuffer PositionBuf : register(t0);
ByteAddressBuffer TangentBuf  : register(t1);

struct VSOutput {
    float4 pos       : SV_POSITION;
    float3 world_pos : TEXCOORD0;
    float3 tangent   : TEXCOORD1;
    float  t_param   : TEXCOORD2;
};

VSOutput main(uint vid : SV_VertexID) {
    float4 p = asfloat(PositionBuf.Load4(vid * 16));
    float4 tn = asfloat(TangentBuf.Load4(vid * 16));

    float4 world_pos = mul(g_model, float4(p.xyz, 1.0));

    VSOutput o;
    o.world_pos = world_pos.xyz;
    o.tangent = normalize(mul((float3x3)g_model, tn.xyz));
    o.t_param = 1.0 - tn.w;
    o.pos = mul(g_projection, mul(g_view, world_pos));
    return o;
}
)";

} // namespace generated_shaders
} // namespace render
} // namespace dse
