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

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vTexCoord;
layout(location = 2) out vec3 vFragPos;
layout(location = 3) out vec3 vNormal;
layout(location = 4) out mat3 vTBN;
layout(location = 7) out vec3 vFragPosViewSpace;

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
layout(location = 8) flat out uint v_material_id;
#define MODEL_MATRIX dse_inst[gl_InstanceIndex].model
#else
// 标准路径：逐对象 push constant + VS 蒙皮
layout(push_constant) uniform PushConstants {
    mat4 u_model;
    int u_skinned;       // 0=no skin, 1=single draw, 2=hardware instanced
    int u_morph_enabled;
    int u_bone_offset;   // 该实例在 bone SSBO 中的起始偏移 (skinned=1 时使用)
} pc;

// Hardware Instancing SSBO: per-instance model + bone_offset (skinned=2 时使用)
struct DSESkinnedInst { mat4 model; int bone_offset; int _pad0; int _pad1; int _pad2; };
layout(std430, set = 2, binding = 10) readonly buffer SkinnedInstBuf {
    DSESkinnedInst skinned_instances[];
};

#define MODEL_MATRIX ((pc.u_skinned == 2) ? skinned_instances[gl_InstanceIndex].model : pc.u_model)

// Bone SSBO: 所有实例的骨骼矩阵紧密排列，per-draw 通过 u_bone_offset 索引
layout(std430, set = 2, binding = 8) readonly buffer BoneMatricesSSBO {
    mat4 u_bone_matrices[];
};

const int MAX_MORPH_TARGETS = 4;
layout(std140, set = 2, binding = 9) uniform MorphWeights {
    float u_morph_weights[MAX_MORPH_TARGETS];
};
#endif

void main() {
#ifdef GPU_DRIVEN
    // Compute skinning 已将蒙皮结果写入 mega VBO；顶点数据已是世界/局部空间
    vec4 localPos    = vec4(aPos, 1.0);
    vec3 finalNormal  = aNormal;
    vec3 finalTangent = aTangent;
#else
    mat4 boneTransform = mat4(1.0);
    if (pc.u_skinned != 0) {
        int bo = (pc.u_skinned == 2)
            ? skinned_instances[gl_InstanceIndex].bone_offset
            : pc.u_bone_offset;
        boneTransform = u_bone_matrices[bo + int(aBoneIndices[0])] * aBoneWeights[0] +
                        u_bone_matrices[bo + int(aBoneIndices[1])] * aBoneWeights[1] +
                        u_bone_matrices[bo + int(aBoneIndices[2])] * aBoneWeights[2] +
                        u_bone_matrices[bo + int(aBoneIndices[3])] * aBoneWeights[3];
    }

    vec3 morphedPos = aPos;
    vec3 finalNormal  = aNormal;
    vec3 finalTangent = aTangent;
    if (pc.u_morph_enabled != 0) {
        morphedPos += vec3(0.01) * u_morph_weights[0];
        finalNormal = aNormal;
    }
    vec4 localPos = boneTransform * vec4(morphedPos, 1.0);
#endif

    vec4 worldPos = MODEL_MATRIX * localPos;
    gl_Position = vp * worldPos;

    vFragPos = worldPos.xyz;
    vFragPosViewSpace = (view * worldPos).xyz;
    vColor = aColor;
    vTexCoord = aTexCoord;
#ifdef GPU_DRIVEN
    v_material_id = dse_inst[gl_InstanceIndex].mat_id;
#endif

#ifdef GPU_DRIVEN
    mat3 normalMatrix = transpose(inverse(mat3(MODEL_MATRIX)));
#else
    // 蒙皮: bone 矩阵通常为 rotation+translation (正交), inverse ≈ transpose,
    // 下游 normalize() 修正均匀缩放, 避免昂贵的 per-vertex inverse()
    mat3 normalMatrix = (pc.u_skinned != 0)
        ? mat3(MODEL_MATRIX * boneTransform)
        : transpose(inverse(mat3(MODEL_MATRIX)));
#endif
    vec3 T = normalize(normalMatrix * finalTangent);
    vec3 N = normalize(normalMatrix * finalNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    vTBN = mat3(T, B, N);
    vNormal = N;
}
