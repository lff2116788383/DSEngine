static const float _69[5] = { 0.227026998996734619140625f, 0.19459460675716400146484375f, 0.121621601283550262451171875f, 0.054053999483585357666015625f, 0.01621600054204463958740234375f };

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

uint2 spvTextureSize(Texture2D<float4> Tex, uint Level, out uint Param)
{
    uint2 ret;
    Tex.GetDimensions(Level, ret.x, ret.y, Param);
    return ret;
}

void frag_main()
{
    uint _20_dummy_parameter;
    float2 tex_offset = 1.0f.xx / float2(int2(spvTextureSize(screenTexture, uint(0), _20_dummy_parameter)));
    float3 result = screenTexture.Sample(_screenTexture_sampler, vTexCoords).xyz * 0.227026998996734619140625f;
    for (int i = 1; i < 5; i++)
    {
        result += (screenTexture.Sample(_screenTexture_sampler, vTexCoords + float2(tex_offset.x * float(i), 0.0f)).xyz * _69[i]);
        result += (screenTexture.Sample(_screenTexture_sampler, vTexCoords - float2(tex_offset.x * float(i), 0.0f)).xyz * _69[i]);
    }
    FragColor = float4(result, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
