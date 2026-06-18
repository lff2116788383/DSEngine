#version 450
#extension GL_ARB_separate_shader_objects : enable
// @SSBO_LOW_REGISTERS
// B2b-2: 蒙皮 forward PBR 顶点着色器（复用 forward_pbr.frag）。
//
// 顶点为局部/绑定空间；骨骼矩阵已在 CPU 侧预乘 model，故 VS 只做骨骼混合 + vp。
// 骨骼矩阵走 SSBO，按 bone index/weight 混合（最多 4 骨骼影响）。
//
// 骨骼 SSBO 置于 set3.binding0，使一次通用原语 BindStorageBuffer(0) 在三后端命中：
//   GL   spirv-cross 保留源 binding=0 → glBindBufferBase(0)
//   VK   通用原语按位置映射，单 SSBO 即第 0 个 → slot 0
//   DX11 经 @SSBO_LOW_REGISTERS → register(t0)（VS-only 绑定，不撞 PS 纹理 t0..t4）

layout(location = 0) in vec3 aPos;        // local/bind-space position
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aNormal;     // local/bind-space normal
layout(location = 4) in vec3 aTangent;    // local/bind-space tangent
layout(location = 5) in vec4 aBoneIndices; // 4 骨骼索引（float 承载）
layout(location = 6) in vec4 aBoneWeights; // 4 骨骼权重（和应为 1）

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

layout(std430, set = 3, binding = 0) readonly buffer BoneMatrices {
    mat4 u_bones[];   // 已预乘 model 的骨骼矩阵（世界空间）
};

void main() {
    mat4 skin =
        u_bones[int(aBoneIndices[0])] * aBoneWeights[0] +
        u_bones[int(aBoneIndices[1])] * aBoneWeights[1] +
        u_bones[int(aBoneIndices[2])] * aBoneWeights[2] +
        u_bones[int(aBoneIndices[3])] * aBoneWeights[3];

    vec4 worldPos = skin * vec4(aPos, 1.0);
    mat3 skin3 = mat3(skin);

    vWorldPos = worldPos.xyz;
    vNormal   = normalize(skin3 * aNormal);
    vTangent  = skin3 * aTangent;
    vColor    = aColor;
    vTexCoord = aTexCoord;
    gl_Position = vp * worldPos;
}
