Texture2D<float4> u_texture : register(t1);
SamplerState _u_texture_sampler : register(s1);

static float2 vTexCoord;
static float4 FragColor;
static float4 vParticleColor;

struct SPIRV_Cross_Input
{
    float4 vParticleColor : TEXCOORD0;
    float2 vTexCoord : TEXCOORD1;
};

struct SPIRV_Cross_Output
{
    float4 FragColor : SV_Target0;
};

void frag_main()
{
    float4 texColor = u_texture.Sample(_u_texture_sampler, vTexCoord);
    FragColor = texColor * vParticleColor;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoord = stage_input.vTexCoord;
    vParticleColor = stage_input.vParticleColor;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
