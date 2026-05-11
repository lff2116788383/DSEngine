cbuffer PerFrame : register(b0)
{
    row_major float4x4 _14_vp : packoffset(c0);
    row_major float4x4 _14_view : packoffset(c4);
    float4 _14_camera_pos : packoffset(c8);
};


static float4 gl_Position;
static float3 iPos;
static float3 aPos;
static float iSize;
static float4 vParticleColor;
static float4 iColor;
static float2 vTexCoord;
static float2 aTexCoord;

struct SPIRV_Cross_Input
{
    float3 aPos : TEXCOORD0;
    float2 aTexCoord : TEXCOORD1;
    float3 iPos : TEXCOORD2;
    float4 iColor : TEXCOORD3;
    float iSize : TEXCOORD4;
};

struct SPIRV_Cross_Output
{
    float4 vParticleColor : TEXCOORD0;
    float2 vTexCoord : TEXCOORD1;
    float4 gl_Position : SV_Position;
};

void vert_main()
{
    float3 camera_right = float3(_14_view[0].x, _14_view[1].x, _14_view[2].x);
    float3 camera_up = float3(_14_view[0].y, _14_view[1].y, _14_view[2].y);
    float3 vertexPosition_worldspace = (iPos + ((camera_right * aPos.x) * iSize)) + ((camera_up * aPos.y) * iSize);
    gl_Position = mul(float4(vertexPosition_worldspace, 1.0f), _14_vp);
    vParticleColor = iColor;
    vTexCoord = aTexCoord;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    iPos = stage_input.iPos;
    aPos = stage_input.aPos;
    iSize = stage_input.iSize;
    iColor = stage_input.iColor;
    aTexCoord = stage_input.aTexCoord;
    vert_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gl_Position = gl_Position;
    stage_output.vParticleColor = vParticleColor;
    stage_output.vTexCoord = vTexCoord;
    return stage_output;
}
