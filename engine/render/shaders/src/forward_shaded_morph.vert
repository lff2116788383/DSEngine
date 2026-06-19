#version 450
#extension GL_ARB_separate_shader_objects : enable
// @SSBO_LOW_REGISTERS
// Final-Feat-5: Morph target（形变目标）+ 高级 shading 组合顶点着色器（配 forward_shaded.frag）。
//
// 顶点基位/法线已在 CPU 侧预变换到世界空间（同 forward_pbr.vert 约定）。每个 morph target
// 的「权重×世界空间增量」已在 CPU 侧按权重线性叠加为单顶点合并增量（morph 为线性混合：
// final = base + Σ_t w_t·delta_t），与本路径既有「顶点 CPU 预变换」设计一致。VS 仅取本顶点
// 合并增量相加 + vp。
//
// 合并增量经 SSBO 提供，每顶点一条，按 gl_VertexIndex 取（索引绘制时等于索引缓冲值，与顶点
// 缓冲并行；三后端语义一致）。
//
// 描述符 set 安排：forward_shaded.frag 已占 set0..set6（各 binding0）+ set7.binding1（聚光灯 UBO）。
//   合并增量 SSBO 置 set7.binding0（与 skinned/instanced 单 SSBO 完全同构 → 通用原语
//   BindStorageBuffer(0) 在三后端命中同一 slot：binding=rank=0，无歧义）：
//     GL   spirv-cross 剥 set、保留 binding=0 → glBindBufferBase(0)
//     VK   通用原语按 (set,binding) 升序映射，单 SSBO 即第 0 个 → slot 0
//     DX11 经 @SSBO_LOW_REGISTERS → register(t0)（VS-only 绑定，不撞 PS 纹理）
//   不引入 VS 侧额外 UBO：DX11 generic 路 cbuffer 寄存器按 stage 内 (set,binding) 顺序紧排
//   （PerFrame→b0），若再加 set7 的权重 UBO 会落到 b1 而非 slot8，与 VSSetConstantBuffers(8)
//   错位（GL/VK 反射驱动则正确）；故权重在 CPU 合并，规避三后端绑定语义分歧。

layout(location = 0) in vec3 aPos;        // world-space base position
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aNormal;     // world-space base normal
layout(location = 4) in vec3 aTangent;    // world-space base tangent

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vTexCoord;
layout(location = 2) out vec3 vWorldPos;
layout(location = 3) out vec3 vNormal;
layout(location = 4) out vec3 vTangent;

layout(std140, set = 0, binding = 0) uniform PerFrame {
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
    vec4 foliage_wind;
    vec4 foliage_push;
};

struct MorphDelta {
    vec4 dpos;   // xyz: 世界空间合并位置增量（已按权重叠加）
    vec4 dnrm;   // xyz: 世界空间合并法线增量
};

layout(std430, set = 7, binding = 0) readonly buffer MorphDeltas {
    MorphDelta u_morph_deltas[];   // 每顶点一条，按 gl_VertexIndex 取
};

void main() {
    MorphDelta d = u_morph_deltas[gl_VertexIndex];
    vec3 pos = aPos + d.dpos.xyz;
    vec3 nrm = aNormal + d.dnrm.xyz;

    vWorldPos = pos;
    vNormal   = normalize(nrm);
    vTangent  = aTangent;
    vColor    = aColor;
    vTexCoord = aTexCoord;
    gl_Position = vp * vec4(pos, 1.0);
}
