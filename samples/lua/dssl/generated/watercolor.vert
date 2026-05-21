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

layout(std140, set = 0, binding = 0) uniform PerFrame {
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
};

// Set 1: PerScene (for TIME etc.)
layout(std140, set = 1, binding = 0) uniform PerScene {
    vec4 light_dir_and_enabled;
    vec4 light_color_and_ambient;
    vec4 light_params;
    vec4 cascade_splits;
    mat4 light_space_matrices[3];
};

layout(push_constant) uniform PushConstants {
    mat4 u_model;
    int u_skinned;
    int u_morph_enabled;
} pc;

const int MAX_BONES = 255;
layout(std140, set = 2, binding = 8) uniform BoneMatrices {
    mat4 u_bone_matrices[MAX_BONES];
};

const int MAX_MORPH_TARGETS = 4;
layout(std140, set = 2, binding = 9) uniform MorphWeights {
    float u_morph_weights[MAX_MORPH_TARGETS];
};

// DSSL PerMaterial UBO (mirrored in vertex shader)
layout(std140, set = 2, binding = 0) uniform PerMaterial {
    vec4 _mat_albedo_color;
    float _mat_paper_strength;
    float _mat_edge_darkening;
    float _mat_color_bleed;
    float _mat_pigment_density;
};
#define albedo_color _mat_albedo_color
#define paper_strength _mat_paper_strength
#define edge_darkening _mat_edge_darkening
#define color_bleed _mat_color_bleed
#define pigment_density _mat_pigment_density

#define MODEL_MATRIX pc.u_model
#define VIEW_MATRIX view
#define PROJECTION_MATRIX vp
#define TIME light_params.w

void main() {
    mat4 boneTransform = mat4(1.0);
    if (pc.u_skinned != 0) {
        boneTransform = u_bone_matrices[int(aBoneIndices[0])] * aBoneWeights[0] +
                        u_bone_matrices[int(aBoneIndices[1])] * aBoneWeights[1] +
                        u_bone_matrices[int(aBoneIndices[2])] * aBoneWeights[2] +
                        u_bone_matrices[int(aBoneIndices[3])] * aBoneWeights[3];
    }

    vec3 VERTEX = aPos;
    vec3 NORMAL = aNormal;
    vec2 UV = aTexCoord;
    vec4 COLOR = aColor;

    if (pc.u_morph_enabled != 0) {
        VERTEX += vec3(0.01) * u_morph_weights[0];
    }

    vec4 localPos = boneTransform * vec4(VERTEX, 1.0);
    vec4 worldPos = pc.u_model * localPos;
    gl_Position = vp * worldPos;

    vFragPos = worldPos.xyz;
    vFragPosViewSpace = (view * worldPos).xyz;
    vColor = COLOR;
    vTexCoord = UV;

    mat3 normalMatrix = transpose(inverse(mat3(pc.u_model * boneTransform)));
    vec3 T = normalize(normalMatrix * aTangent);
    vec3 N = normalize(normalMatrix * NORMAL);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    vTBN = mat3(T, B, N);
    vNormal = N;
}
