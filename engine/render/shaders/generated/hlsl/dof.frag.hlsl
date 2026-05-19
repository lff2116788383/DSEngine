cbuffer DofParams
{
    float _20_focus_distance : packoffset(c0);
    float _20_focus_range : packoffset(c0.y);
    float _20_bokeh_radius : packoffset(c0.z);
    float _20_near_plane : packoffset(c0.w);
    float _20_far_plane : packoffset(c1);
    float _20_screen_w : packoffset(c1.y);
    float _20_screen_h : packoffset(c1.z);
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

float linearizeDepth(float d)
{
    float z = (d * 2.0f) - 1.0f;
    return ((2.0f * _20_near_plane) * _20_far_plane) / ((_20_far_plane + _20_near_plane) - (z * (_20_far_plane - _20_near_plane)));
}

void frag_main()
{
    float depth = screenTexture.Sample(_screenTexture_sampler, vTexCoords).x;
    float param = depth;
    float lin_depth = linearizeDepth(param);
    float coc = clamp(abs(lin_depth - _20_focus_distance) / _20_focus_range, 0.0f, 1.0f);
    float2 texel = 1.0f.xx / float2(_20_screen_w, _20_screen_h);
    float radius = coc * _20_bokeh_radius;
    float3 color = 0.0f.xxx;
    float total_weight = 0.0f;
    for (int i = 0; i < 16; i++)
    {
        float r = sqrt(float(i) / 16.0f) * radius;
        float theta = float(i) * 2.3999631404876708984375f;
        float2 offset = (float2(cos(theta), sin(theta)) * r) * texel;
        float param_1 = screenTexture.Sample(_screenTexture_sampler, vTexCoords + offset).x;
        float sample_depth = linearizeDepth(param_1);
        float sample_coc = clamp(abs(sample_depth - _20_focus_distance) / _20_focus_range, 0.0f, 1.0f);
        float w = max(sample_coc, coc);
        color += (u_color_texture.Sample(_u_color_texture_sampler, vTexCoords + offset).xyz * w);
        total_weight += w;
    }
    if (total_weight > 0.0f)
    {
        color /= total_weight.xxx;
    }
    else
    {
        color = u_color_texture.Sample(_u_color_texture_sampler, vTexCoords).xyz;
    }
    FragColor = float4(color, 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
