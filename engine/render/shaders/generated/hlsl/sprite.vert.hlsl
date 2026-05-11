cbuffer PushConstants
{
    row_major float4x4 pc_u_model : packoffset(c0);
    row_major float4x4 pc_u_vp : packoffset(c4);
};


static float4 gl_Position;
static float2 aPos;
static float4 vColor;
static float4 aColor;
static float2 vTexCoord;
static float2 aTexCoord;

struct SPIRV_Cross_Input
{
    float2 aPos : TEXCOORD0;
    float2 aTexCoord : TEXCOORD1;
    float4 aColor : TEXCOORD2;
};

struct SPIRV_Cross_Output
{
    float4 vColor : TEXCOORD0;
    float2 vTexCoord : TEXCOORD1;
    float4 gl_Position : SV_Position;
};

void vert_main()
{
    gl_Position = mul(float4(aPos, 0.0f, 1.0f), mul(pc_u_model, pc_u_vp));
    vColor = aColor;
    vTexCoord = aTexCoord;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    aPos = stage_input.aPos;
    aColor = stage_input.aColor;
    aTexCoord = stage_input.aTexCoord;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.vColor = vColor;
    stage_output.vTexCoord = vTexCoord;
    return stage_output;
}
