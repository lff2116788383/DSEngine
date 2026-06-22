#version 450
#extension GL_ARB_separate_shader_objects : enable
// @SSBO_LOW_REGISTERS
// 阶段4-M1：蒙皮 + 硬件实例化组合顶点着色器（配 forward_shaded.frag）。
//
// 与 pbr.vert 的 u_skinned==2 路径同算：骨骼矩阵为「绑定→局部空间」（未在 CPU 预乘 model），
// 每实例独立的 model 矩阵与 bone_offset 经实例 SSBO 提供；VS 先做骨骼混合（仍在局部空间），
// 再施每实例 model 到世界空间，最后 vp。骨骼调色板可在 CPU 侧去重（多实例共享一份调色板），
// bone_offset 表达该实例调色板在密排骨骼 SSBO 中的起始下标。
//
// varying 接口（location 0..4）与 forward_shaded.frag 逐位匹配（同 forward_shaded_skinned.vert /
// forward_shaded_instanced.vert）。
//
// 描述符 set 安排：forward_shaded.frag 已占满 set0..set6（各 binding0）+ set7.binding1（聚光灯 UBO）。
//   本 VS 需两个 SSBO（实例 + 骨骼），故置于独立的 set8.binding0/1，既与 frag 的 set7.binding1
//   无 Vulkan 同 set+binding 冲突，又因 @SSBO_LOW_REGISTERS 使两 SSBO 落连续低位：
//     GL   spirv-cross 剥 set、保留 binding=0/1 → glBindBufferBase(0)/(1)
//     VK   通用原语按 (set,binding) 升序映射，两 SSBO 即第 0/1 个 → slot 0/1
//     DX11 经 @SSBO_LOW_REGISTERS → register(t0)/(t1)（VS-only 绑定，不撞 PS 纹理）
//   三后端通用原语 BindStorageBuffer(0)=实例 / BindStorageBuffer(1)=骨骼 对齐。

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

// 每实例 model + bone_offset（与 DX11 执行器 SkinnedInstGPU 同布局：mat4 + int + 3 pad，80 字节）。
struct MeshSkinnedInst {
    mat4 model;       // 世界空间每实例 model
    int  bone_offset; // 该实例骨骼调色板在 u_bones 中的起始下标
    int  _pad0;
    int  _pad1;
    int  _pad2;
};
layout(std430, set = 8, binding = 0) readonly buffer SkinnedInstBuf {
    MeshSkinnedInst u_instances[];
};
layout(std430, set = 8, binding = 1) readonly buffer BoneMatrices {
    mat4 u_bones[];   // 绑定→局部空间骨骼矩阵（未预乘 model），多实例调色板密排
};

void main() {
    MeshSkinnedInst inst = u_instances[gl_InstanceIndex];
    int bo = inst.bone_offset;
    mat4 skin =
        u_bones[bo + int(aBoneIndices[0])] * aBoneWeights[0] +
        u_bones[bo + int(aBoneIndices[1])] * aBoneWeights[1] +
        u_bones[bo + int(aBoneIndices[2])] * aBoneWeights[2] +
        u_bones[bo + int(aBoneIndices[3])] * aBoneWeights[3];

    mat4 model = inst.model;
    vec4 localPos = skin * vec4(aPos, 1.0);
    vec4 worldPos = model * localPos;
    // 法线矩阵：model*skin 通常含 rotation+translation（近似正交），下游 normalize() 修正均匀缩放。
    mat3 nrmMat = mat3(model * skin);

    vWorldPos = worldPos.xyz;
    vNormal   = normalize(nrmMat * aNormal);
    vTangent  = nrmMat * aTangent;
    vColor    = aColor;
    vTexCoord = aTexCoord;
    gl_Position = vp * worldPos;
}
