cbuffer PushConstants
{
    row_major float4x4 pc_u_vp : packoffset(c0);
};


static float4 gl_Position;
static float3 vTexCoords;
static float3 aPos;

struct SPIRV_Cross_Input
{
    float3 aPos : TEXCOORD0;
};

struct SPIRV_Cross_Output
{
    float3 vTexCoords : TEXCOORD0;
    float4 gl_Position : SV_Position;
};

void vert_main()
{
    vTexCoords = aPos;
    float4 pos = mul(float4(aPos * 10000.0f, 1.0f), pc_u_vp);
    gl_Position = pos.xyww;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    aPos = stage_input.aPos;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.vTexCoords = vTexCoords;
    return stage_output;
}
