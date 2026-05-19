cbuffer TaaParams
{
    float _36_u_blend_factor : packoffset(c0);
    float _36_u_jitter_x : packoffset(c0.y);
    float _36_u_jitter_y : packoffset(c0.z);
    int _36_u_frame_index : packoffset(c0.w);
    float _36_u_screen_w : packoffset(c1);
    float _36_u_screen_h : packoffset(c1.y);
};

Texture2D<float4> screenTexture : register(t0);
SamplerState _screenTexture_sampler : register(s0);
Texture2D<float4> u_motion_vector : register(t1);
SamplerState _u_motion_vector_sampler : register(s1);
Texture2D<float4> u_history : register(t2);
SamplerState _u_history_sampler : register(s2);

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
    float3 current = screenTexture.Sample(_screenTexture_sampler, vTexCoords).xyz;
    float2 mv = u_motion_vector.Sample(_u_motion_vector_sampler, vTexCoords).xy;
    float2 history_uv = (vTexCoords - mv) - float2(_36_u_jitter_x, _36_u_jitter_y);
    history_uv = clamp(history_uv, 0.0f.xx, 1.0f.xx);
    float2 texel = 1.0f.xx / float2(_36_u_screen_w, _36_u_screen_h);
    float3 m1 = 0.0f.xxx;
    float3 m2 = 0.0f.xxx;
    for (int dx = -1; dx <= 1; dx++)
    {
        for (int dy = -1; dy <= 1; dy++)
        {
            float3 s = screenTexture.Sample(_screenTexture_sampler, vTexCoords + (float2(float(dx), float(dy)) * texel)).xyz;
            m1 += s;
            m2 += (s * s);
        }
    }
    m1 /= 9.0f.xxx;
    float3 sigma = sqrt(max((m2 / 9.0f.xxx) - (m1 * m1), 0.0f.xxx));
    float3 aabb_min = m1 - (sigma * 1.25f);
    float3 aabb_max = m1 + (sigma * 1.25f);
    float3 history = u_history.Sample(_u_history_sampler, history_uv).xyz;
    history = clamp(history, aabb_min, aabb_max);
    float velocity_len = length(mv * float2(_36_u_screen_w, _36_u_screen_h));
    float vel_weight = clamp(velocity_len * 0.5f, 0.0f, 0.5f);
    float _165;
    if (_36_u_frame_index < 2)
    {
        _165 = 1.0f;
    }
    else
    {
        _165 = clamp(_36_u_blend_factor + vel_weight, _36_u_blend_factor, 1.0f);
    }
    float alpha = _165;
    FragColor = float4(lerp(history, current, alpha.xxx), 1.0f);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
