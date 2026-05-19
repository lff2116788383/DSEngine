Texture2D<float4> screenTexture : register(t0);
SamplerState _screenTexture_sampler : register(s0);

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

uint2 spvTextureSize(Texture2D<float4> Tex, uint Level, out uint Param)
{
    uint2 ret;
    Tex.GetDimensions(Level, ret.x, ret.y, Param);
    return ret;
}

void frag_main()
{
    uint _20_dummy_parameter;
    float2 texelSize = 1.0f.xx / float2(int2(spvTextureSize(screenTexture, uint(0), _20_dummy_parameter)));
    float result = 0.0f;
    for (int x = -2; x <= 2; x++)
    {
        for (int y = -2; y <= 2; y++)
        {
            float2 offset = float2(float(x), float(y)) * texelSize;
            result += screenTexture.Sample(_screenTexture_sampler, vTexCoords + offset).x;
        }
    }
    FragColor = float4((result / 25.0f).xxx, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
