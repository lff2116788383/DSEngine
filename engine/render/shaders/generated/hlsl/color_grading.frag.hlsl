cbuffer ColorGradingParams
{
    float _40_u_lut_intensity : packoffset(c0);
};

Texture2D<float4> screenTexture : register(t0);
SamplerState _screenTexture_sampler : register(s0);
Texture3D<float4> u_lut : register(t1);
SamplerState _u_lut_sampler : register(s1);

static float4 gl_FragCoord;
static float2 vTexCoords;
static float4 FragColor;

struct SPIRV_Cross_Input
{
    float2 vTexCoords : TEXCOORD0;
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 FragColor : SV_Target0;
};

void frag_main()
{
    float3 color = screenTexture.Sample(_screenTexture_sampler, vTexCoords).xyz;
    float3 lutColor = u_lut.Sample(_u_lut_sampler, clamp(color, 0.0f.xxx, 1.0f.xxx)).xyz;
    color = lerp(color, lutColor, _40_u_lut_intensity.xxx);
    float ign = frac(52.98291778564453125f * frac((0.067110560834407806396484375f * gl_FragCoord.x) + (0.005837149918079376220703125f * gl_FragCoord.y)));
    color += ((ign - 0.5f) / 255.0f).xxx;
    FragColor = float4(color, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
