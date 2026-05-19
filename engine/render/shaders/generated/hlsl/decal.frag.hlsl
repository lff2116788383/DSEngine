cbuffer DecalParams
{
    float _35_u_depth_handle : packoffset(c0);
    float _35_u_decal_handle : packoffset(c0.y);
    float _35_m00 : packoffset(c0.z);
    float _35_m01 : packoffset(c0.w);
    float _35_m02 : packoffset(c1);
    float _35_m03 : packoffset(c1.y);
    float _35_m10 : packoffset(c1.z);
    float _35_m11 : packoffset(c1.w);
    float _35_m12 : packoffset(c2);
    float _35_m13 : packoffset(c2.y);
    float _35_m20 : packoffset(c2.z);
    float _35_m21 : packoffset(c2.w);
    float _35_m22 : packoffset(c3);
    float _35_m23 : packoffset(c3.y);
    float _35_m30 : packoffset(c3.z);
    float _35_m31 : packoffset(c3.w);
    float _35_m32 : packoffset(c4);
    float _35_m33 : packoffset(c4.y);
    float _35_u_color_r : packoffset(c4.z);
    float _35_u_color_g : packoffset(c4.w);
    float _35_u_color_b : packoffset(c5);
    float _35_u_color_a : packoffset(c5.y);
    float _35_u_angle_fade : packoffset(c5.z);
    float _35_u_decal_up_x : packoffset(c5.w);
    float _35_u_decal_up_y : packoffset(c6);
    float _35_u_decal_up_z : packoffset(c6.y);
};

Texture2D<float4> u_depth_tex : register(t1);
SamplerState _u_depth_tex_sampler : register(s1);
Texture2D<float4> u_decal_tex : register(t2);
SamplerState _u_decal_tex_sampler : register(s2);
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

uint2 spvTextureSize(Texture2D<float4> Tex, uint Level, out uint Param)
{
    uint2 ret;
    Tex.GetDimensions(Level, ret.x, ret.y, Param);
    return ret;
}

void frag_main()
{
    float depth = u_depth_tex.Sample(_u_depth_tex_sampler, vTexCoords).x;
    if (depth >= 0.99989998340606689453125f)
    {
        discard;
    }
    float4x4 inv_mvp = float4x4(float4(float4(_35_m00, _35_m01, _35_m02, _35_m03)), float4(float4(_35_m10, _35_m11, _35_m12, _35_m13)), float4(float4(_35_m20, _35_m21, _35_m22, _35_m23)), float4(float4(_35_m30, _35_m31, _35_m32, _35_m33)));
    float4 clip = float4((vTexCoords * 2.0f) - 1.0f.xx, (depth * 2.0f) - 1.0f, 1.0f);
    float4 local4 = mul(clip, inv_mvp);
    float3 local = local4.xyz / local4.w.xxx;
    bool _144 = abs(local.x) > 0.5f;
    bool _153;
    if (!_144)
    {
        _153 = abs(local.y) > 0.5f;
    }
    else
    {
        _153 = _144;
    }
    bool _162;
    if (!_153)
    {
        _162 = abs(local.z) > 0.5f;
    }
    else
    {
        _162 = _153;
    }
    if (_162)
    {
        discard;
    }
    float2 decal_uv = local.xz + 0.5f.xx;
    float4 color = float4(_35_u_color_r, _35_u_color_g, _35_u_color_b, _35_u_color_a);
    float4 decal = u_decal_tex.Sample(_u_decal_tex_sampler, decal_uv) * color;
    float angle_factor = 1.0f;
    if (_35_u_angle_fade > 0.0f)
    {
        uint _205_dummy_parameter;
        float2 texel = 1.0f.xx / float2(int2(spvTextureSize(u_depth_tex, uint(0), _205_dummy_parameter)));
        float dl = u_depth_tex.Sample(_u_depth_tex_sampler, vTexCoords + float2(-texel.x, 0.0f)).x;
        float dr = u_depth_tex.Sample(_u_depth_tex_sampler, vTexCoords + float2(texel.x, 0.0f)).x;
        float dt = u_depth_tex.Sample(_u_depth_tex_sampler, vTexCoords + float2(0.0f, texel.y)).x;
        float db = u_depth_tex.Sample(_u_depth_tex_sampler, vTexCoords + float2(0.0f, -texel.y)).x;
        float3 normal = normalize(float3(dl - dr, dt - db, 2.0f * texel.x));
        float3 decal_up = float3(_35_u_decal_up_x, _35_u_decal_up_y, _35_u_decal_up_z);
        float facing = abs(dot(normal, decal_up));
        angle_factor = smoothstep(0.0f, 1.0f - _35_u_angle_fade, facing);
    }
    FragColor = float4(decal.xyz, decal.w * angle_factor);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    vTexCoords = stage_input.vTexCoords;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
