cbuffer BloomCompositeAeParams
{
    float _90_exposure : packoffset(c0);
    float _90_bloomIntensity : packoffset(c0.y);
    int _90_bloomEnabled : packoffset(c0.z);
    int _90_ssaoEnabled : packoffset(c0.w);
    int _90_autoExposureEnabled : packoffset(c1);
    int _90_lutEnabled : packoffset(c1.y);
    float _90_lutIntensity : packoffset(c1.z);
    int _90_csEnabled : packoffset(c1.w);
    float _90_csStrength : packoffset(c2);
    int _90_vignetteEnabled : packoffset(c2.y);
    float _90_vignetteIntensity : packoffset(c2.z);
    float _90_vignetteRadius : packoffset(c2.w);
    float _90_vignetteSoftness : packoffset(c3);
    int _90_filmGrainEnabled : packoffset(c3.y);
    float _90_filmGrainIntensity : packoffset(c3.z);
    float _90_filmGrainTime : packoffset(c3.w);
};

Texture2D<float4> screenTexture : register(t0);
SamplerState _screenTexture_sampler : register(s0);
Texture2D<float4> ssaoTexture : register(t2);
SamplerState _ssaoTexture_sampler : register(s2);
Texture2D<float4> bloomBlur : register(t1);
SamplerState _bloomBlur_sampler : register(s1);
Texture2D<float4> contactShadowTex : register(t5);
SamplerState _contactShadowTex_sampler : register(s5);
Texture2D<float4> autoExposureTex : register(t3);
SamplerState _autoExposureTex_sampler : register(s3);
Texture3D<float4> u_lut : register(t4);
SamplerState _u_lut_sampler : register(s4);

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

float3 AcesFilmic(float3 x)
{
    float a = 2.5099999904632568359375f;
    float b = 0.02999999932944774627685546875f;
    float c = 2.4300000667572021484375f;
    float d = 0.589999973773956298828125f;
    float e = 0.14000000059604644775390625f;
    return clamp((x * ((x * a) + b.xxx)) / ((x * ((x * c) + d.xxx)) + e.xxx), 0.0f.xxx, 1.0f.xxx);
}

float GrainNoise(float2 uv, float time_seed)
{
    return frac(sin(dot(uv + float2(time_seed, time_seed * 0.37000000476837158203125f), float2(12.98980045318603515625f, 78.233001708984375f))) * 43758.546875f);
}

void frag_main()
{
    float3 color = screenTexture.Sample(_screenTexture_sampler, vTexCoords).xyz;
    if (_90_ssaoEnabled != 0)
    {
        float ao = ssaoTexture.Sample(_ssaoTexture_sampler, vTexCoords).x;
        color *= ao;
    }
    if (_90_bloomEnabled != 0)
    {
        float3 bloomColor = bloomBlur.Sample(_bloomBlur_sampler, vTexCoords).xyz;
        color += (bloomColor * _90_bloomIntensity);
    }
    if (_90_csEnabled != 0)
    {
        float cs = contactShadowTex.Sample(_contactShadowTex_sampler, vTexCoords).x;
        color *= (1.0f - ((1.0f - cs) * _90_csStrength));
    }
    float finalExposure = _90_exposure;
    if (_90_autoExposureEnabled != 0)
    {
        finalExposure = autoExposureTex.Sample(_autoExposureTex_sampler, 0.5f.xx).x;
    }
    float3 param = color * finalExposure;
    color = AcesFilmic(param);
    color = pow(color, 0.4545454680919647216796875f.xxx);
    if (_90_lutEnabled != 0)
    {
        float3 lutColor = u_lut.Sample(_u_lut_sampler, clamp(color, 0.0f.xxx, 1.0f.xxx)).xyz;
        color = lerp(color, lutColor, _90_lutIntensity.xxx);
    }
    if (_90_vignetteEnabled != 0)
    {
        float dist = length(vTexCoords - 0.5f.xx);
        float radius = clamp(_90_vignetteRadius, 0.001000000047497451305389404296875f, 1.5f);
        float softness = max(_90_vignetteSoftness, 9.9999997473787516355514526367188e-05f);
        float vignette = 1.0f - smoothstep(radius, radius + softness, dist);
        color *= lerp(1.0f, vignette, clamp(_90_vignetteIntensity, 0.0f, 1.0f));
    }
    if (_90_filmGrainEnabled != 0)
    {
        float2 param_1 = vTexCoords * float2(1280.0f, 720.0f);
        float param_2 = _90_filmGrainTime;
        float grain = GrainNoise(param_1, param_2) - 0.5f;
        color = clamp(color + (grain * _90_filmGrainIntensity).xxx, 0.0f.xxx, 1.0f.xxx);
    }
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
