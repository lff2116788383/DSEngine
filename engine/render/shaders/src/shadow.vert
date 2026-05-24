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
    int u_skinned;
    int u_morph_enabled;
} pc;

const int MAX_BONES = 255;
layout(std140, set = 2, binding = 8) uniform BoneMatrices {
    mat4 u_bone_matrices[MAX_BONES];
};
#endif

void main() {
#ifdef GPU_DRIVEN
    vec4 localPos = vec4(aPos, 1.0);
    vec4 worldPos = dse_inst[gl_InstanceIndex].model * localPos;
#else
    mat4 boneTransform = mat4(1.0);
    if (pc.u_skinned != 0) {
        boneTransform = u_bone_matrices[int(aBoneIndices[0])] * aBoneWeights[0] +
                        u_bone_matrices[int(aBoneIndices[1])] * aBoneWeights[1] +
                        u_bone_matrices[int(aBoneIndices[2])] * aBoneWeights[2] +
                        u_bone_matrices[int(aBoneIndices[3])] * aBoneWeights[3];
    }

    vec4 localPos = boneTransform * vec4(aPos, 1.0);
    vec4 worldPos = pc.u_model * localPos;
#endif
    gl_Position = vp * worldPos;
}
