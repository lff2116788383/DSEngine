cbuffer LightShaftParams
{
    float _25_u_depth_handle : packoffset(c0);
    float _25_u_sun_x : packoffset(c0.y);
    float _25_u_sun_y : packoffset(c0.z);
    float _25_u_light_r : packoffset(c0.w);
    float _25_u_light_g : packoffset(c1);
    float _25_u_light_b : packoffset(c1.y);
    float _25_u_density : packoffset(c1.z);
    float _25_u_weight : packoffset(c1.w);
    float _25_u_decay : packoffset(c2);
    float _25_u_exposure : packoffset(c2.y);
    float _25_u_num_samples : packoffset(c2.z);
    float _25_u_intensity : packoffset(c2.w);
    float _25_pad0 : packoffset(c3);
    float _25_pad1 : packoffset(c3.y);
    float _25_pad2 : packoffset(c3.z);
    float _25_pad3 : packoffset(c3.w);
};

Texture2D<float4> screenTexture : register(t0);
SamplerState _screenTexture_sampler : register(s0);
Texture2D<float4> u_depth_tex : register(t1);
SamplerState _u_depth_tex_sampler : register(s1);

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
    float4 scene = screenTexture.Sample(_screenTexture_sampler, vTexCoords);
    int samples = int(_25_u_num_samples);
    float2 delta_uv = ((float2(_25_u_sun_x, _25_u_sun_y) - vTexCoords) * _25_u_density) / float(samples).xx;
    float2 uv = vTexCoords;
    float illum_decay = 1.0f;
    float3 accumulated = 0.0f.xxx;
    for (int i = 0; i < samples; i++)
    {
        uv += delta_uv;
        float2 suv = clamp(uv, 0.001000000047497451305389404296875f.xx, 0.999000012874603271484375f.xx);
        float d = u_depth_tex.Sample(_u_depth_tex_sampler, suv).x;
        float3 s = screenTexture.Sample(_screenTexture_sampler, suv).xyz;
        float sky = step(0.99989998340606689453125f, d);
        float lum = dot(s, float3(0.2125999927520751953125f, 0.715200006961822509765625f, 0.072200000286102294921875f));
        float bright = smoothstep(0.800000011920928955078125f, 1.2000000476837158203125f, lum);
        float mask = max(sky, bright);
        accumulated += (((s * mask) * illum_decay) * _25_u_weight);
        illum_decay *= _25_u_decay;
        if (illum_decay < 0.0030000000260770320892333984375f)
        {
            break;
        }
    }
    float3 result = scene.xyz + (((accumulated * _25_u_exposure) * float3(_25_u_light_r, _25_u_light_g, _25_u_light_b)) * _25_u_intensity);
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
