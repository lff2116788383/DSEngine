#version 450
#extension GL_ARB_separate_shader_objects : enable
// @VARIANTS: GPU_DRIVEN

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aNormal;
layout(location = 4) in vec3 aTangent;
layout(location = 5) in vec4 aBoneWeights;
layout(location = 6) in vec4 aBoneIndices;

// Set 0: PerFrame
layout(std140, set = 0, binding = 0) uniform PerFrame {
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
};

#ifdef GPU_DRIVEN
// GPU-Driven 路径：model 矩阵来自 SSBO，compute 着色器已完成蒙皮
struct DSEGPUInst { mat4 model; uint mat_id; uint cmd_id; uint pad0; uint pad1; };
layout(std430, set = 4, binding = 0) readonly buffer DSEInstBuf {
    DSEGPUInst dse_inst[];
};
#else
// Push Constant: 逐对象数据
layout(push_constant) uniform PushConstants {
    mat4 u_model;
    int u_skinned;       // 0=no skin, 1=single draw, 2=hardware instanced
    int u_morph_enabled;
    int u_bone_offset;
} pc;

// Hardware Instancing SSBO: per-instance model + bone_offset (skinned=2 时使用)
struct DSESkinnedInst { mat4 model; int bone_offset; int _pad0; int _pad1; int _pad2; };
layout(std430, set = 2, binding = 10) readonly buffer SkinnedInstBuf {
    DSESkinnedInst skinned_instances[];
};

// Bone SSBO: 所有实例的骨骼矩阵紧密排列
layout(std430, set = 2, binding = 8) readonly buffer BoneMatricesSSBO {
    mat4 u_bone_matrices[];
};

// Compute Skinning Output SSBO: 预蒙皮顶点数据
struct ComputeSkinVertex { vec4 pos; vec4 normal; vec4 tangent; };
layout(std430, set = 2, binding = 20) readonly buffer ComputeSkinBuf {
    ComputeSkinVertex compute_skin_verts[];
};
#endif

void main() {
#ifdef GPU_DRIVEN
    vec4 localPos = vec4(aPos, 1.0);
    vec4 worldPos = dse_inst[gl_InstanceIndex].model * localPos;
#else
    mat4 boneTransform = mat4(1.0);
    if ((pc.u_skinned == 1) || (pc.u_skinned == 2)) {
        int bo = (pc.u_skinned == 2)
            ? skinned_instances[gl_InstanceIndex].bone_offset
            : pc.u_bone_offset;
        boneTransform = u_bone_matrices[bo + int(aBoneIndices[0])] * aBoneWeights[0] +
                        u_bone_matrices[bo + int(aBoneIndices[1])] * aBoneWeights[1] +
                        u_bone_matrices[bo + int(aBoneIndices[2])] * aBoneWeights[2] +
                        u_bone_matrices[bo + int(aBoneIndices[3])] * aBoneWeights[3];
    }

    vec4 localPos = boneTransform * vec4(aPos, 1.0);

    // Compute pre-skinned path: 读取 compute shader 输出的已蒙皮顶点
    if (pc.u_skinned == 3) {
        uint skin_idx = uint(pc.u_bone_offset) + uint(gl_VertexIndex);
        localPos = compute_skin_verts[skin_idx].pos;
    }

    mat4 inst_model = ((pc.u_skinned == 2) || (pc.u_skinned == 3))
        ? skinned_instances[gl_InstanceIndex].model : pc.u_model;
    vec4 worldPos = inst_model * localPos;
#endif
    gl_Position = vp * worldPos;
}
