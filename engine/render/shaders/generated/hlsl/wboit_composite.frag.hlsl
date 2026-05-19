cbuffer WboitParams
{
    float _61_u_reveal_handle : packoffset(c0);
};

Texture2D<float4> screenTexture : register(t0);
SamplerState _screenTexture_sampler : register(s0);
Texture2D<float4> u_reveal_tex : register(t1);
SamplerState _u_reveal_tex_sampler : register(s1);

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
    float4 accum = screenTexture.Sample(_screenTexture_sampler, vTexCoords);
    float revealage = u_reveal_tex.Sample(_u_reveal_tex_sampler, vTexCoords).x;
    if (accum.w < 9.9999997473787516355514526367188e-05f)
    {
        discard;
    }
    float3 avg_color = accum.xyz / max(accum.w, 9.9999997473787516355514526367188e-06f).xxx;
    FragColor = float4(avg_color, 1.0f - revealage);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
