cbuffer BloomParams : register(b3)
{
    float _73_exposure : packoffset(c0);
    float _73_bloomIntensity : packoffset(c0.y);
};

Texture2D<float4> screenTexture : register(t1);
SamplerState _screenTexture_sampler : register(s1);
Texture2D<float4> bloomBlur : register(t2);
SamplerState _bloomBlur_sampler : register(s2);

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

float3 AcesFilmic(float3 x)
{
    float a = 2.5099999904632568359375f;
    float b = 0.02999999932944774627685546875f;
    float c = 2.4300000667572021484375f;
    float d = 0.589999973773956298828125f;
    float e = 0.14000000059604644775390625f;
    return clamp((x * ((x * a) + b.xxx)) / ((x * ((x * c) + d.xxx)) + e.xxx), 0.0f.xxx, 1.0f.xxx);
}

void frag_main()
{
    float3 color = screenTexture.Sample(_screenTexture_sampler, vTexCoords).xyz;
    float3 bloomColor = bloomBlur.Sample(_bloomBlur_sampler, vTexCoords).xyz;
    color += (bloomColor * _73_bloomIntensity);
    float3 param = color * _73_exposure;
    color = AcesFilmic(param);
    color = pow(color, 0.4545454680919647216796875f.xxx);
    FragColor = float4(color, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
