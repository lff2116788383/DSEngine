cbuffer SsrParams
{
    float _28_max_distance : packoffset(c0);
    float _28_thickness : packoffset(c0.y);
    float _28_step_size : packoffset(c0.z);
    int _28_max_steps : packoffset(c0.w);
    float _28_near_plane : packoffset(c1);
    float _28_far_plane : packoffset(c1.y);
    float _28_screen_w : packoffset(c1.z);
    float _28_screen_h : packoffset(c1.w);
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
    return ((2.0f * _28_near_plane) * _28_far_plane) / ((_28_far_plane + _28_near_plane) - (z * (_28_far_plane - _28_near_plane)));
}

float3 reconstructNormal(float2 uv)
{
    float2 texel = 1.0f.xx / float2(_28_screen_w, _28_screen_h);
    float param = screenTexture.Sample(_screenTexture_sampler, uv).x;
    float dc = linearizeDepth(param);
    float param_1 = screenTexture.Sample(_screenTexture_sampler, uv - float2(texel.x, 0.0f)).x;
    float dl = linearizeDepth(param_1);
    float param_2 = screenTexture.Sample(_screenTexture_sampler, uv + float2(texel.x, 0.0f)).x;
    float dr = linearizeDepth(param_2);
    float param_3 = screenTexture.Sample(_screenTexture_sampler, uv - float2(0.0f, texel.y)).x;
    float db = linearizeDepth(param_3);
    float param_4 = screenTexture.Sample(_screenTexture_sampler, uv + float2(0.0f, texel.y)).x;
    float dt = linearizeDepth(param_4);
    return normalize(float3(dl - dr, db - dt, (2.0f * texel.x) * dc));
}

void frag_main()
{
    float depth = screenTexture.Sample(_screenTexture_sampler, vTexCoords).x;
    if (depth >= 1.0f)
    {
        FragColor = 0.0f.xxxx;
        return;
    }
    float param = depth;
    float lin_depth = linearizeDepth(param);
    float2 param_1 = vTexCoords;
    float3 normal = reconstructNormal(param_1);
    float3 view_dir = float3((vTexCoords * 2.0f) - 1.0f.xx, 1.0f);
    view_dir = normalize(view_dir);
    float3 reflect_dir = reflect(view_dir, normal);
    float2 texel = 1.0f.xx / float2(_28_screen_w, _28_screen_h);
    float2 ray_uv = vTexCoords;
    float ray_depth = lin_depth;
    for (int i = 0; i < _28_max_steps; i++)
    {
        ray_uv += ((reflect_dir.xy * texel) * _28_step_size);
        bool _216 = ray_uv.x < 0.0f;
        bool _223;
        if (!_216)
        {
            _223 = ray_uv.x > 1.0f;
        }
        else
        {
            _223 = _216;
        }
        bool _230;
        if (!_223)
        {
            _230 = ray_uv.y < 0.0f;
        }
        else
        {
            _230 = _223;
        }
        bool _237;
        if (!_230)
        {
            _237 = ray_uv.y > 1.0f;
        }
        else
        {
            _237 = _230;
        }
        if (_237)
        {
            break;
        }
        float param_2 = screenTexture.SampleLevel(_screenTexture_sampler, ray_uv, 0.0f).x;
        float sample_depth = linearizeDepth(param_2);
        ray_depth += (reflect_dir.z * _28_step_size);
        float depth_diff = ray_depth - sample_depth;
        bool _261 = depth_diff > 0.0f;
        bool _269;
        if (_261)
        {
            _269 = depth_diff < _28_thickness;
        }
        else
        {
            _269 = _261;
        }
        if (_269)
        {
            float fade = 1.0f - (float(i) / float(_28_max_steps));
            float3 hit_color = u_color_texture.SampleLevel(_u_color_texture_sampler, ray_uv, 0.0f).xyz;
            FragColor = float4(hit_color * fade, fade);
            return;
        }
    }
    FragColor = 0.0f.xxxx;
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
