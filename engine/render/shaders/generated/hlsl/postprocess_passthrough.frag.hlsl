Texture2D<float4> screenTexture : register(t1);
SamplerState _screenTexture_sampler : register(s1);

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
    FragColor = screenTexture.Sample(_screenTexture_sampler, vTexCoords);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
