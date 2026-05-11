cbuffer BloomParams : register(b2)
{
    float _33_threshold : packoffset(c0);
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
    float3 color = screenTexture.Sample(_screenTexture_sampler, vTexCoords).xyz;
    float brightness = dot(color, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
    if (brightness > _33_threshold)
    {
        FragColor = float4(color, 1.0f);
    }
    else
    {
        FragColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
    }
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
