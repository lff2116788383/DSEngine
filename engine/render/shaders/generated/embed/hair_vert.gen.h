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

} // namespace generated_shaders
} // namespace render
} // namespace dse
