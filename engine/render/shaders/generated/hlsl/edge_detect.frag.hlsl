cbuffer EdgeDetectParams
{
    float _28_u_thickness : packoffset(c0);
    float _28_u_depth_threshold : packoffset(c0.y);
    float _28_u_normal_threshold : packoffset(c0.z);
    float _28_u_outline_r : packoffset(c0.w);
    float _28_u_outline_g : packoffset(c1);
    float _28_u_outline_b : packoffset(c1.y);
    float _28_u_near : packoffset(c1.z);
    float _28_u_far : packoffset(c1.w);
    float _28_u_screen_w : packoffset(c2);
    float _28_u_screen_h : packoffset(c2.y);
};

Texture2D<float4> screenTexture : register(t0);
SamplerState _screenTexture_sampler : register(s0);

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

float linearize_depth(float d)
{
    float ndc = (d * 2.0f) - 1.0f;
    return ((2.0f * _28_u_near) * _28_u_far) / ((_28_u_far + _28_u_near) - (ndc * (_28_u_far - _28_u_near)));
}

float3 reconstruct_normal(float2 uv, float2 texel_size)
{
    float param = screenTexture.Sample(_screenTexture_sampler, uv).x;
    float dc = linearize_depth(param);
    float param_1 = screenTexture.Sample(_screenTexture_sampler, uv - float2(texel_size.x, 0.0f)).x;
    float dl = linearize_depth(param_1);
    float param_2 = screenTexture.Sample(_screenTexture_sampler, uv + float2(texel_size.x, 0.0f)).x;
    float dr = linearize_depth(param_2);
    float param_3 = screenTexture.Sample(_screenTexture_sampler, uv - float2(0.0f, texel_size.y)).x;
    float db = linearize_depth(param_3);
    float param_4 = screenTexture.Sample(_screenTexture_sampler, uv + float2(0.0f, texel_size.y)).x;
    float dt = linearize_depth(param_4);
    return normalize(float3(dl - dr, db - dt, (2.0f * texel_size.x) * dc));
}

void frag_main()
{
    float2 base_texel = float2(1.0f / _28_u_screen_w, 1.0f / _28_u_screen_h);
    float2 texel = base_texel * _28_u_thickness;
    float param = screenTexture.Sample(_screenTexture_sampler, vTexCoords).x;
    float d_c = linearize_depth(param);
    float param_1 = screenTexture.Sample(_screenTexture_sampler, vTexCoords + float2(-texel.x, 0.0f)).x;
    float d_l = linearize_depth(param_1);
    float param_2 = screenTexture.Sample(_screenTexture_sampler, vTexCoords + float2(texel.x, 0.0f)).x;
    float d_r = linearize_depth(param_2);
    float param_3 = screenTexture.Sample(_screenTexture_sampler, vTexCoords + float2(0.0f, texel.y)).x;
    float d_t = linearize_depth(param_3);
    float param_4 = screenTexture.Sample(_screenTexture_sampler, vTexCoords + float2(0.0f, -texel.y)).x;
    float d_b = linearize_depth(param_4);
    float depth_diff = abs(d_l - d_r) + abs(d_t - d_b);
    float depth_edge = smoothstep(0.0f, _28_u_depth_threshold * d_c, depth_diff);
    float2 param_5 = vTexCoords;
    float2 param_6 = base_texel;
    float3 n_c = reconstruct_normal(param_5, param_6);
    float2 param_7 = vTexCoords + float2(-texel.x, 0.0f);
    float2 param_8 = base_texel;
    float3 n_l = reconstruct_normal(param_7, param_8);
    float2 param_9 = vTexCoords + float2(texel.x, 0.0f);
    float2 param_10 = base_texel;
    float3 n_r = reconstruct_normal(param_9, param_10);
    float2 param_11 = vTexCoords + float2(0.0f, texel.y);
    float2 param_12 = base_texel;
    float3 n_t = reconstruct_normal(param_11, param_12);
    float2 param_13 = vTexCoords + float2(0.0f, -texel.y);
    float2 param_14 = base_texel;
    float3 n_b = reconstruct_normal(param_13, param_14);
    float normal_diff = length(n_l - n_r) + length(n_t - n_b);
    float normal_edge = smoothstep(0.0f, _28_u_normal_threshold, normal_diff);
    float edge = clamp(max(depth_edge, normal_edge), 0.0f, 1.0f);
    FragColor = float4(_28_u_outline_r, _28_u_outline_g, _28_u_outline_b, edge);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
