static const uint3 gl_WorkGroupSize = uint3(8u, 8u, 1u);

cbuffer BloomParams
{
    float u_params_src_texel_w : packoffset(c0);
    float u_params_src_texel_h : packoffset(c0.y);
    float u_params_dst_texel_w : packoffset(c0.z);
    float u_params_dst_texel_h : packoffset(c0.w);
};

RWTexture2D<float4> u_dst : register(u1);
Texture2D<float4> u_src : register(t0);
SamplerState _u_src_sampler : register(s0);

static uint3 gl_GlobalInvocationID;
struct SPIRV_Cross_Input
{
    uint3 gl_GlobalInvocationID : SV_DispatchThreadID;
};

uint2 spvImageSize(RWTexture2D<float4> Tex, out uint Param)
{
    uint2 ret;
    Tex.GetDimensions(ret.x, ret.y);
    Param = 0u;
    return ret;
}

void comp_main()
{
    int2 dst_coord = int2(gl_GlobalInvocationID.xy);
    uint _24_dummy_parameter;
    int2 dst_size = int2(spvImageSize(u_dst, _24_dummy_parameter));
    bool _32 = dst_coord.x >= dst_size.x;
    bool _42;
    if (!_32)
    {
        _42 = dst_coord.y >= dst_size.y;
    }
    else
    {
        _42 = _32;
    }
    if (_42)
    {
        return;
    }
    float2 uv = (float2(dst_coord) + 0.5f.xx) * float2(u_params_dst_texel_w, u_params_dst_texel_h);
    float x = u_params_src_texel_w;
    float y = u_params_src_texel_h;
    float3 a = u_src.SampleLevel(_u_src_sampler, uv + float2(-x, y), 0.0f).xyz;
    float3 b = u_src.SampleLevel(_u_src_sampler, uv + float2(0.0f, y), 0.0f).xyz;
    float3 c = u_src.SampleLevel(_u_src_sampler, uv + float2(x, y), 0.0f).xyz;
    float3 d = u_src.SampleLevel(_u_src_sampler, uv + float2(-x, 0.0f), 0.0f).xyz;
    float3 e = u_src.SampleLevel(_u_src_sampler, uv, 0.0f).xyz;
    float3 f = u_src.SampleLevel(_u_src_sampler, uv + float2(x, 0.0f), 0.0f).xyz;
    float3 g = u_src.SampleLevel(_u_src_sampler, uv + float2(-x, -y), 0.0f).xyz;
    float3 h = u_src.SampleLevel(_u_src_sampler, uv + float2(0.0f, -y), 0.0f).xyz;
    float3 i = u_src.SampleLevel(_u_src_sampler, uv + float2(x, -y), 0.0f).xyz;
    float3 result = (((e * 4.0f) + ((((b + d) + f) + h) * 2.0f)) + (((a + c) + g) + i)) * 0.0625f;
    u_dst[dst_coord] = float4(result, 1.0f);
}

[numthreads(8, 8, 1)]
void main(SPIRV_Cross_Input stage_input)
{
    gl_GlobalInvocationID = stage_input.gl_GlobalInvocationID;
    comp_main();
}
