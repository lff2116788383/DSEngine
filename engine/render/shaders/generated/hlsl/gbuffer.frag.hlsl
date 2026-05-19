cbuffer PerFrame : register(b0)
{
    row_major float4x4 _63_vp : packoffset(c0);
    row_major float4x4 _63_view : packoffset(c4);
    float4 _63_camera_pos : packoffset(c8);
};

cbuffer PerScene : register(b1)
{
    float4 _66_gbuf_dummy : packoffset(c0);
};

Texture2D<float4> u_texture : register(t0);
SamplerState _u_texture_sampler : register(s0);

static float2 vTexCoord;
static float4 vColor;
static float4 gAlbedo;
static float4 gNormal;
static float3 vNormal;
static float4 gPosition;
static float3 vFragPos;

struct SPIRV_Cross_Input
{
    float4 vColor : TEXCOORD0;
    float2 vTexCoord : TEXCOORD1;
    float3 vFragPos : TEXCOORD2;
    float3 vNormal : TEXCOORD3;
};

struct SPIRV_Cross_Output
{
    float4 gAlbedo : SV_Target0;
    float4 gNormal : SV_Target1;
    float4 gPosition : SV_Target2;
};

void frag_main()
{
    float4 albedo = u_texture.Sample(_u_texture_sampler, vTexCoord) * vColor;
    if (albedo.w < 0.00999999977648258209228515625f)
    {
        discard;
    }
    gAlbedo = albedo;
    gNormal = float4((normalize(vNormal) * 0.5f) + 0.5f.xxx, 1.0f);
    gPosition = float4(vFragPos, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoord = stage_input.vTexCoord;
    vColor = stage_input.vColor;
    vNormal = stage_input.vNormal;
    vFragPos = stage_input.vFragPos;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.gAlbedo = gAlbedo;
    stage_output.gNormal = gNormal;
    stage_output.gPosition = gPosition;
    return stage_output;
}
