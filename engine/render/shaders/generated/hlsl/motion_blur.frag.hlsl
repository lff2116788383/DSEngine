cbuffer MotionBlurParams
{
    float _23_intensity : packoffset(c0);
    float _23_num_samples : packoffset(c0.y);
    float _23_screen_w : packoffset(c0.z);
    float _23_screen_h : packoffset(c0.w);
};

Texture2D<float4> screenTexture : register(t0);
SamplerState _screenTexture_sampler : register(s0);
Texture2D<float4> u_color_texture : register(t1);
SamplerState _u_color_texture_sampler : register(s1);

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
    float2 velocity = screenTexture.Sample(_screenTexture_sampler, vTexCoords).xy * _23_intensity;
    int samples = max(int(_23_num_samples), 1);
    float3 color = u_color_texture.Sample(_u_color_texture_sampler, vTexCoords).xyz;
    float total = 1.0f;
    for (int i = 1; i < samples; i++)
    {
        float t = float(i) / float(samples);
        float2 sample_uv = vTexCoords + (velocity * t);
        bool _75 = sample_uv.x >= 0.0f;
        bool _81;
        if (_75)
        {
            _81 = sample_uv.x <= 1.0f;
        }
        else
        {
            _81 = _75;
        }
        bool _88;
        if (_81)
        {
            _88 = sample_uv.y >= 0.0f;
        }
        else
        {
            _88 = _81;
        }
        bool _94;
        if (_88)
        {
            _94 = sample_uv.y <= 1.0f;
        }
        else
        {
            _94 = _88;
        }
        if (_94)
        {
            color += u_color_texture.Sample(_u_color_texture_sampler, sample_uv).xyz;
            total += 1.0f;
        }
    }
    FragColor = float4(color / total.xxx, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
