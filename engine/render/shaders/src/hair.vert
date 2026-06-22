#version 450
#extension GL_ARB_separate_shader_objects : enable
// @SSBO_LOW_REGISTERS
// Hair strand vertex shader — SSBO position/tangent + combined HairUniforms UBO.
//
// 单组合 UBO（set0.binding0）跨 VS/FS 共享（取代旧的 TransformUniforms\@b4 + HairUniforms\@b12
// 分离 UBO：分离 UBO 在 DX11 per-stage 各自落 b0，无法被通用原语统一 slot 寻址）。
// position/tangent SSBO 落 set7.binding0/1 低位 t-register（@SSBO_LOW_REGISTERS），
// 与通用原语 BindStorageBuffer(0/1) 在三后端对齐。顶点无属性，按 gl_VertexIndex 取 SSBO。
// 配 HairRenderer（LINE_STRIP PSO + 逐 strand Draw）。

layout(std430, set = 7, binding = 0) readonly buffer PositionBuf { vec4 positions[]; };
layout(std430, set = 7, binding = 1) readonly buffer TangentBuf  { vec4 tangents[]; };

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

layout(location = 0) out vec3 v_world_pos;
layout(location = 1) out vec3 v_tangent;
layout(location = 2) out float v_t; // 0=root, 1=tip

void main() {
    vec4 pos = positions[gl_VertexIndex];
    vec4 tan = tangents[gl_VertexIndex];

    vec4 world_pos = u_model * vec4(pos.xyz, 1.0);
    v_world_pos = world_pos.xyz;
    v_tangent = normalize(mat3(u_model) * tan.xyz);
    v_t = 1.0 - tan.w; // tangent.w = thickness: 1 at root, 0 at tip

    gl_Position = u_projection * u_view * world_pos;
}
