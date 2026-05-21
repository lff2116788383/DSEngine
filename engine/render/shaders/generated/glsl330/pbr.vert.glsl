#version 430

layout(binding = 8, std140) uniform BoneMatrices
{
    mat4 u_bone_matrices[255];
} _36;

layout(binding = 9, std140) uniform MorphWeights
{
    float u_morph_weights[4];
} _138;

layout(binding = 0, std140) uniform PerFrame
{
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
} _166;

uniform mat4 u_model;
uniform int u_skinned;
uniform int u_morph_enabled;
layout(location = 6) in vec4 aBoneIndices;
layout(location = 5) in vec4 aBoneWeights;
layout(location = 0) in vec3 aPos;
layout(location = 3) in vec3 aNormal;
layout(location = 2) out vec3 vFragPos;
layout(location = 7) out vec3 vFragPosViewSpace;
layout(location = 0) out vec4 vColor;
layout(location = 1) in vec4 aColor;
layout(location = 1) out vec2 vTexCoord;
layout(location = 2) in vec2 aTexCoord;
layout(location = 4) in vec3 aTangent;
layout(location = 4) out mat3 vTBN;
layout(location = 3) out vec3 vNormal;

void main()
{
    mat4 boneTransform = mat4(vec4(1.0, 0.0, 0.0, 0.0), vec4(0.0, 1.0, 0.0, 0.0), vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0));
    if (u_skinned != 0)
    {
        mat4 _50 = _36.u_bone_matrices[int(aBoneIndices.x)] * aBoneWeights.x;
        mat4 _59 = _36.u_bone_matrices[int(aBoneIndices.y)] * aBoneWeights.y;
        mat4 _72 = mat4(_50[0] + _59[0], _50[1] + _59[1], _50[2] + _59[2], _50[3] + _59[3]);
        mat4 _81 = _36.u_bone_matrices[int(aBoneIndices.z)] * aBoneWeights.z;
        mat4 _94 = mat4(_72[0] + _81[0], _72[1] + _81[1], _72[2] + _81[2], _72[3] + _81[3]);
        mat4 _103 = _36.u_bone_matrices[int(aBoneIndices.w)] * aBoneWeights.w;
        boneTransform = mat4(_94[0] + _103[0], _94[1] + _103[1], _94[2] + _103[2], _94[3] + _103[3]);
    }
    vec3 morphedPos = aPos;
    vec3 morphedNormal = aNormal;
    if (u_morph_enabled != 0)
    {
        morphedPos += (vec3(0.00999999977648258209228515625) * _138.u_morph_weights[0]);
    }
    vec4 localPos = boneTransform * vec4(morphedPos, 1.0);
    vec4 worldPos = u_model * localPos;
    gl_Position = _166.vp * worldPos;
    vFragPos = worldPos.xyz;
    vFragPosViewSpace = (_166.view * worldPos).xyz;
    vColor = aColor;
    vTexCoord = aTexCoord;
    mat4 _198 = u_model * boneTransform;
    mat3 normalMatrix = transpose(inverse(mat3(_198[0].xyz, _198[1].xyz, _198[2].xyz)));
    vec3 T = normalize(normalMatrix * aTangent);
    vec3 N = normalize(normalMatrix * morphedNormal);
    T = normalize(T - (N * dot(T, N)));
    vec3 B = cross(N, T);
    vTBN = mat3(vec3(T), vec3(B), vec3(N));
    vNormal = N;
}

