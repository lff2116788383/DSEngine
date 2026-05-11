cbuffer BloomParams : register(b2)
{
    float _11_filterRadius : packoffset(c0);
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
    float x = _11_filterRadius;
    float y = _11_filterRadius;
    float3 a = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x - x, vTexCoords.y + y)).xyz;
    float3 b = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x, vTexCoords.y + y)).xyz;
    float3 c = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x + x, vTexCoords.y + y)).xyz;
    float3 d = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x - x, vTexCoords.y)).xyz;
    float3 e = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x, vTexCoords.y)).xyz;
    float3 f = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x + x, vTexCoords.y)).xyz;
    float3 g = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x - x, vTexCoords.y - y)).xyz;
    float3 h = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x, vTexCoords.y - y)).xyz;
    float3 i = screenTexture.Sample(_screenTexture_sampler, float2(vTexCoords.x + x, vTexCoords.y - y)).xyz;
    float3 upsample = e * 4.0f;
    upsample += ((((b + d) + f) + h) * 2.0f);
    upsample += (((a + c) + g) + i);
    upsample *= 0.0625f;
    FragColor = float4(upsample, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
