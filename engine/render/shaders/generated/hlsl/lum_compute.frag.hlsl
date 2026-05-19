Texture2D<float4> screenTexture : register(t0);
SamplerState _screenTexture_sampler : register(s0);

static float4 FragColor;
static float2 vTexCoords;

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
    float logSum = 0.0f;
    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            float2 uv = (float2(float(i), float(j)) + 0.5f.xx) / 8.0f.xx;
            float3 c = screenTexture.Sample(_screenTexture_sampler, uv).xyz;
            float lum = dot(c, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
            logSum += log(max(lum, 9.9999997473787516355514526367188e-05f));
        }
    }
    float avgLogLum = logSum / 64.0f;
    FragColor = float4(avgLogLum, 0.0f, 0.0f, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
