#version 450
#extension GL_ARB_separate_shader_objects : enable

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

// Push Constant: 逐对象数据
layout(push_constant) uniform PushConstants {
    mat4 u_model;
    int u_skinned;
    int u_morph_enabled;
} pc;

const int MAX_BONES = 100;
layout(std140, set = 2, binding = 8) uniform BoneMatrices {
    mat4 u_bone_matrices[MAX_BONES];
};

const int MAX_MORPH_TARGETS = 4;
layout(std140, set = 2, binding = 9) uniform MorphWeights {
    float u_morph_weights[MAX_MORPH_TARGETS];
};

void main() {
    mat4 boneTransform = mat4(1.0);
    if (pc.u_skinned != 0) {
        boneTransform = u_bone_matrices[int(aBoneIndices[0])] * aBoneWeights[0] +
                        u_bone_matrices[int(aBoneIndices[1])] * aBoneWeights[1] +
                        u_bone_matrices[int(aBoneIndices[2])] * aBoneWeights[2] +
                        u_bone_matrices[int(aBoneIndices[3])] * aBoneWeights[3];
    }

    vec3 morphedPos = aPos;
    vec3 morphedNormal = aNormal;
    if (pc.u_morph_enabled != 0) {
        morphedPos += vec3(0.01) * u_morph_weights[0];
    }

    vec4 localPos = boneTransform * vec4(morphedPos, 1.0);
    vec4 worldPos = pc.u_model * localPos;
    gl_Position = vp * worldPos;

    vFragPos = worldPos.xyz;
    vFragPosViewSpace = (view * worldPos).xyz;
    vColor = aColor;
    vTexCoord = aTexCoord;

    mat3 normalMatrix = transpose(inverse(mat3(pc.u_model * boneTransform)));
    vec3 T = normalize(normalMatrix * aTangent);
    vec3 N = normalize(normalMatrix * morphedNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    vTBN = mat3(T, B, N);
    vNormal = N;
}
