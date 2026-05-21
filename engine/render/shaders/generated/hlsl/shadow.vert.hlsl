cbuffer BoneMatrices : register(b2)
{
    row_major float4x4 _36_u_bone_matrices[255] : packoffset(c0);
};

cbuffer PerFrame : register(b1)
{
    row_major float4x4 _141_vp : packoffset(c0);
    row_major float4x4 _141_view : packoffset(c4);
    float4 _141_camera_pos : packoffset(c8);
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
static float4 aColor;
static float2 aTexCoord;
static float3 aNormal;
static float3 aTangent;

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
    float4 gl_Position : SV_Position;
};

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
    float4 localPos = mul(float4(aPos, 1.0f), boneTransform);
    float4 worldPos = mul(localPos, pc_u_model);
    gl_Position = mul(worldPos, _141_vp);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    aBoneIndices = stage_input.aBoneIndices;
    aBoneWeights = stage_input.aBoneWeights;
    aPos = stage_input.aPos;
    aColor = stage_input.aColor;
    aTexCoord = stage_input.aTexCoord;
    aNormal = stage_input.aNormal;
    aTangent = stage_input.aTangent;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    return stage_output;
}
