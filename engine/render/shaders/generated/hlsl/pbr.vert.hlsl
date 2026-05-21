cbuffer BoneMatrices : register(b2)
{
    row_major float4x4 _36_u_bone_matrices[255] : packoffset(c0);
};

cbuffer MorphWeights : register(b3)
{
    float _138_u_morph_weights[4] : packoffset(c0);
};

cbuffer PerFrame : register(b1)
{
    row_major float4x4 _166_vp : packoffset(c0);
    row_major float4x4 _166_view : packoffset(c4);
    float4 _166_camera_pos : packoffset(c8);
};

cbuffer PushConstants
{
    row_major float4x4 pc_u_model : packoffset(c0);
    int pc_u_skinned : packoffset(c4);
    int pc_u_morph_enabled : packoffset(c4.y);
};


static float4 gl_Position;
static float4 aBoneIndices;
static float4 aBoneWeights;
static float3 aPos;
static float3 aNormal;
static float3 vFragPos;
static float3 vFragPosViewSpace;
static float4 vColor;
static float4 aColor;
static float2 vTexCoord;
static float2 aTexCoord;
static float3 aTangent;
static float3x3 vTBN;
static float3 vNormal;

struct SPIRV_Cross_Input
{
    float3 aPos : TEXCOORD0;
    float4 aColor : TEXCOORD1;
    float2 aTexCoord : TEXCOORD2;
    float3 aNormal : TEXCOORD3;
    float3 aTangent : TEXCOORD4;
    float4 aBoneWeights : TEXCOORD5;
    float4 aBoneIndices : TEXCOORD6;
};

struct SPIRV_Cross_Output
{
    float4 vColor : TEXCOORD0;
    float2 vTexCoord : TEXCOORD1;
    float3 vFragPos : TEXCOORD2;
    float3 vNormal : TEXCOORD3;
    float3x3 vTBN : TEXCOORD4;
    float3 vFragPosViewSpace : TEXCOORD7;
    float4 gl_Position : SV_Position;
};

// Returns the determinant of a 2x2 matrix.
float spvDet2x2(float a1, float a2, float b1, float b2)
{
    return a1 * b2 - b1 * a2;
}

// Returns the inverse of a matrix, by using the algorithm of calculating the classical
// adjoint and dividing by the determinant. The contents of the matrix are changed.
float3x3 spvInverse(float3x3 m)
{
    float3x3 adj;	// The adjoint matrix (inverse after dividing by determinant)

    // Create the transpose of the cofactors, as the classical adjoint of the matrix.
    adj[0][0] =  spvDet2x2(m[1][1], m[1][2], m[2][1], m[2][2]);
    adj[0][1] = -spvDet2x2(m[0][1], m[0][2], m[2][1], m[2][2]);
    adj[0][2] =  spvDet2x2(m[0][1], m[0][2], m[1][1], m[1][2]);

    adj[1][0] = -spvDet2x2(m[1][0], m[1][2], m[2][0], m[2][2]);
    adj[1][1] =  spvDet2x2(m[0][0], m[0][2], m[2][0], m[2][2]);
    adj[1][2] = -spvDet2x2(m[0][0], m[0][2], m[1][0], m[1][2]);

    adj[2][0] =  spvDet2x2(m[1][0], m[1][1], m[2][0], m[2][1]);
    adj[2][1] = -spvDet2x2(m[0][0], m[0][1], m[2][0], m[2][1]);
    adj[2][2] =  spvDet2x2(m[0][0], m[0][1], m[1][0], m[1][1]);

    // Calculate the determinant as a combination of the cofactors of the first row.
    float det = (adj[0][0] * m[0][0]) + (adj[0][1] * m[1][0]) + (adj[0][2] * m[2][0]);

    // Divide the classical adjoint matrix by the determinant.
    // If determinant is zero, matrix is not invertable, so leave it unchanged.
    return (det != 0.0f) ? (adj * (1.0f / det)) : m;
}

void vert_main()
{
    float4x4 boneTransform = float4x4(float4(1.0f, 0.0f, 0.0f, 0.0f), float4(0.0f, 1.0f, 0.0f, 0.0f), float4(0.0f, 0.0f, 1.0f, 0.0f), float4(0.0f, 0.0f, 0.0f, 1.0f));
    if (pc_u_skinned != 0)
    {
        float4x4 _50 = _36_u_bone_matrices[int(aBoneIndices.x)] * aBoneWeights.x;
        float4x4 _59 = _36_u_bone_matrices[int(aBoneIndices.y)] * aBoneWeights.y;
        float4x4 _72 = float4x4(_50[0] + _59[0], _50[1] + _59[1], _50[2] + _59[2], _50[3] + _59[3]);
        float4x4 _81 = _36_u_bone_matrices[int(aBoneIndices.z)] * aBoneWeights.z;
        float4x4 _94 = float4x4(_72[0] + _81[0], _72[1] + _81[1], _72[2] + _81[2], _72[3] + _81[3]);
        float4x4 _103 = _36_u_bone_matrices[int(aBoneIndices.w)] * aBoneWeights.w;
        boneTransform = float4x4(_94[0] + _103[0], _94[1] + _103[1], _94[2] + _103[2], _94[3] + _103[3]);
    }
    float3 morphedPos = aPos;
    float3 morphedNormal = aNormal;
    if (pc_u_morph_enabled != 0)
    {
        morphedPos += (0.00999999977648258209228515625f.xxx * _138_u_morph_weights[0]);
    }
    float4 localPos = mul(float4(morphedPos, 1.0f), boneTransform);
    float4 worldPos = mul(localPos, pc_u_model);
    gl_Position = mul(worldPos, _166_vp);
    vFragPos = worldPos.xyz;
    vFragPosViewSpace = mul(worldPos, _166_view).xyz;
    vColor = aColor;
    vTexCoord = aTexCoord;
    float4x4 _198 = mul(boneTransform, pc_u_model);
    float3x3 normalMatrix = transpose(spvInverse(float3x3(_198[0].xyz, _198[1].xyz, _198[2].xyz)));
    float3 T = normalize(mul(aTangent, normalMatrix));
    float3 N = normalize(mul(morphedNormal, normalMatrix));
    T = normalize(T - (N * dot(T, N)));
    float3 B = cross(N, T);
    vTBN = float3x3(float3(T), float3(B), float3(N));
    vNormal = N;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    aBoneIndices = stage_input.aBoneIndices;
    aBoneWeights = stage_input.aBoneWeights;
    aPos = stage_input.aPos;
    aNormal = stage_input.aNormal;
    aColor = stage_input.aColor;
    aTexCoord = stage_input.aTexCoord;
    aTangent = stage_input.aTangent;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.vFragPos = vFragPos;
    stage_output.vFragPosViewSpace = vFragPosViewSpace;
    stage_output.vColor = vColor;
    stage_output.vTexCoord = vTexCoord;
    stage_output.vTBN = vTBN;
    stage_output.vNormal = vNormal;
    return stage_output;
}
