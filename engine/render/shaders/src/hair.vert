#version 450

// Hair strand vertex shader — uses SSBO for position/tangent data.
// NOTE: Requires GLSL 430+ (SSBO). Cannot cross-compile to GLSL 330.

layout(std430, binding = 0) readonly buffer PositionBuf { vec4 positions[]; };
layout(std430, binding = 3) readonly buffer TangentBuf  { vec4 tangents[]; };

layout(std140, binding = 4) uniform TransformUniforms {
    mat4 u_model;
    mat4 u_view;
    mat4 u_projection;
};

layout(location = 0) out vec3 v_world_pos;
layout(location = 1) out vec3 v_tangent;
layout(location = 2) out float v_t; // 0=root, 1=tip

void main() {
    vec4 pos = positions[gl_VertexID];
    vec4 tan = tangents[gl_VertexID];

    vec4 world_pos = u_model * vec4(pos.xyz, 1.0);
    v_world_pos = world_pos.xyz;
    v_tangent = normalize(mat3(u_model) * tan.xyz);
    v_t = 1.0 - tan.w; // tangent.w = thickness: 1 at root, 0 at tip

    gl_Position = u_projection * u_view * world_pos;
}
