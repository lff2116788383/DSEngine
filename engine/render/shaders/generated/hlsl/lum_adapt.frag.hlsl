cbuffer LumAdaptParams
{
    float _34_u_dt : packoffset(c0);
    float _34_u_speed_up : packoffset(c0.y);
    float _34_u_speed_down : packoffset(c0.z);
    float _34_u_min_exposure : packoffset(c0.w);
    float _34_u_max_exposure : packoffset(c1);
    float _34_u_compensation : packoffset(c1.y);
};

Texture2D<float4> screenTexture : register(t0);
SamplerState _screenTexture_sampler : register(s0);
Texture2D<float4> prevAdaptedTex : register(t1);
SamplerState _prevAdaptedTex_sampler : register(s1);

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
    float avgLogLum = screenTexture.Sample(_screenTexture_sampler, 0.5f.xx).x;
    float avgLum = exp(avgLogLum);
    float targetExposure = 0.180000007152557373046875f / max(avgLum, 0.001000000047497451305389404296875f);
    targetExposure = clamp(targetExposure * exp2(_34_u_compensation), _34_u_min_exposure, _34_u_max_exposure);
    float prevExposure = prevAdaptedTex.Sample(_prevAdaptedTex_sampler, 0.5f.xx).x;
    if (prevExposure <= 0.0f)
    {
        prevExposure = targetExposure;
    }
    float _65;
    if (targetExposure > prevExposure)
    {
        _65 = _34_u_speed_up;
    }
    else
    {
        _65 = _34_u_speed_down;
    }
    float speed = _65;
    float adapted = prevExposure + ((targetExposure - prevExposure) * (1.0f - exp((-_34_u_dt) * speed)));
    FragColor = float4(adapted, 0.0f, 0.0f, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
