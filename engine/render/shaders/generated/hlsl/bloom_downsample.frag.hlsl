cbuffer BloomParams : register(b2)
{
    float2 _13_srcResolution : packoffset(c0);
};

Texture2D<float4> screenTexture : register(t1);
SamplerState _screenTexture_sampler : register(s1);

static float2 vTexCoords;
static float4 FragColor;

struct SPIRV_Cross_Input
{
    float2 vTexCoords : TEXCOORD0;
};

struct SPIRV_Cross_Output
{
    float4 FragColor : SV_Target0;
};

void frag_main()
{
    float2 srcTexelSize = 1.0f.xx / _13_srcResolution;
    float x = srcTexelSize.x;
    float y = srcTexelSize.y;
    float3 a = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x - (2.0f * x), vTexCoords.y + (2.0f * y))).xyz;
    float3 b = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x, vTexCoords.y + (2.0f * y))).xyz;
    float3 c = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x + (2.0f * x), vTexCoords.y + (2.0f * y))).xyz;
    float3 d = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x - (2.0f * x), vTexCoords.y)).xyz;
    float3 e = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x, vTexCoords.y)).xyz;
    float3 f = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x + (2.0f * x), vTexCoords.y)).xyz;
    float3 g = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x - (2.0f * x), vTexCoords.y - (2.0f * y))).xyz;
    float3 h = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x, vTexCoords.y - (2.0f * y))).xyz;
    float3 i = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x + (2.0f * x), vTexCoords.y - (2.0f * y))).xyz;
    float3 j = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x - x, vTexCoords.y + y)).xyz;
    float3 k = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x + x, vTexCoords.y + y)).xyz;
    float3 l = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x - x, vTexCoords.y - y)).xyz;
    float3 m = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x + x, vTexCoords.y - y)).xyz;
    float3 downsample = e * 0.125f;
    downsample += ((((a + c) + g) + i) * 0.03125f);
    downsample += ((((b + d) + f) + h) * 0.0625f);
    downsample += ((((j + k) + l) + m) * 0.125f);
    FragColor = float4(downsample, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
