/**
 * @file dx11_shader_sources.h
 * @brief D3D11 专用 HLSL 5.0 内置着色器源码
 *
 * 所有着色器使用 Shader Model 5.0，入口点约定：
 * - 顶点着色器: VSMain
 * - 像素着色器: PSMain
 *
 * 常量缓冲布局约定（与 DX11DrawExecutor CB 对齐）：
 *   b0: PerFrame CB   (vp, view, camera_pos)
 *   b1: PerObject CB  (model, skinned, morph_enabled)
 *   b2: PerScene CB   (方向光/阴影参数)
 *   b3: PerMaterial CB (材质参数)
 */

#ifndef DSE_RENDER_DX11_SHADER_SOURCES_H
#define DSE_RENDER_DX11_SHADER_SOURCES_H

namespace dse {
namespace render {
namespace dx11_shaders {

// ============================================================
// 2D 精灵着色器
// ============================================================

constexpr const char* kSpriteVS = R"(
cbuffer PerFrame : register(b0) {
    float4x4 vp;
    float4x4 view;
    float4 camera_pos;
};

cbuffer PerObject : register(b1) {
    float4x4 model;
    int skinned;
    int morph_enabled;
    int _pad0;
    int _pad1;
};

struct VSInput {
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

struct VSOutput {
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

VSOutput VSMain(VSInput input) {
    VSOutput output;
    output.pos = mul(vp, mul(model, float4(input.pos, 0.0, 1.0)));
    output.col = input.col;
    output.uv  = input.uv;
    return output;
}
)";

constexpr const char* kSpritePS = R"(
Texture2D u_texture : register(t0);
SamplerState u_sampler : register(s0);

struct PSInput {
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    float4 texColor = u_texture.Sample(u_sampler, input.uv);
    return texColor * input.col;
}
)";

// ============================================================
// PBR 着色器
// ============================================================

constexpr const char* kPbrVS = R"(
cbuffer PerFrame : register(b0) {
    float4x4 vp;
    float4x4 view;
    float4 camera_pos;
};

cbuffer PerObject : register(b1) {
    float4x4 model;
    int skinned;
    int morph_enabled;
    int _pad0;
    int _pad1;
};

struct VSInput {
    float3 pos       : POSITION;
    float4 color     : COLOR0;
    float2 uv        : TEXCOORD0;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float4 boneWts   : BLENDWEIGHT;
    float4 boneIdx   : BLENDINDICES;
};

struct VSOutput {
    float4 pos            : SV_POSITION;
    float4 color          : COLOR0;
    float2 uv             : TEXCOORD0;
    float3 fragPos        : TEXCOORD1;
    float3 normal         : TEXCOORD2;
    float3 tangent        : TEXCOORD3;
    float3 bitangent      : TEXCOORD4;
    float3 fragPosView    : TEXCOORD5;
};

VSOutput VSMain(VSInput input) {
    VSOutput output;
    float4 worldPos = mul(model, float4(input.pos, 1.0));
    output.pos = mul(vp, worldPos);
    output.fragPos = worldPos.xyz;
    output.fragPosView = mul(view, worldPos).xyz;
    output.color = input.color;
    output.uv = input.uv;

    float3x3 normalMatrix = (float3x3)model;
    float3 N = normalize(mul(normalMatrix, input.normal));
    float3 T = normalize(mul(normalMatrix, input.tangent));
    T = normalize(T - dot(T, N) * N);
    float3 B = cross(N, T);
    output.normal = N;
    output.tangent = T;
    output.bitangent = B;
    return output;
}
)";

constexpr const char* kPbrPS = R"(
cbuffer PerFrame : register(b0) {
    float4x4 vp;
    float4x4 view;
    float4 camera_pos;
};

cbuffer PerScene : register(b2) {
    float4 light_dir_and_enabled;
    float4 light_color_and_ambient;
    float4 light_params;
    float4 cascade_splits;
    float4x4 light_space_matrices[3];
};

cbuffer PerMaterial : register(b3) {
    float4 mat_albedo;
    float4 mat_roughness_ao;
    float4 mat_emissive;
    float4 mat_flags;
};

Texture2D u_texture : register(t0);
Texture2D u_normal_map : register(t1);
Texture2D u_metallic_roughness_map : register(t2);
Texture2D u_emissive_map : register(t3);
Texture2D u_occlusion_map : register(t4);
Texture2D u_shadow_map0 : register(t5);
Texture2D u_shadow_map1 : register(t6);
Texture2D u_shadow_map2 : register(t7);

SamplerState u_sampler : register(s0);
SamplerComparisonState u_cmp_sampler : register(s1);

static const float PI = 3.14159265359;

struct PSInput {
    float4 pos            : SV_POSITION;
    float4 color          : COLOR0;
    float2 uv             : TEXCOORD0;
    float3 fragPos        : TEXCOORD1;
    float3 normal         : TEXCOORD2;
    float3 tangent        : TEXCOORD3;
    float3 bitangent      : TEXCOORD4;
    float3 fragPosView    : TEXCOORD5;
};

float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
           GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

float3 fresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float SampleShadowPCF(Texture2D shadowMap, SamplerComparisonState cmp_sampler,
                       float3 proj_coords, float bias) {
    float shadow = 0.0;
    float2 texel = 1.0 / float2(2048.0, 2048.0);
    [unroll] for (int x = -1; x <= 1; ++x)
    [unroll] for (int y = -1; y <= 1; ++y)
        shadow += shadowMap.SampleCmpLevelZero(
            cmp_sampler, proj_coords.xy + float2(x, y) * texel, proj_coords.z - bias);
    return shadow / 9.0;
}

float ShadowCalculation(float3 fragPosWorld, float3 fragPosView, float3 normal, float3 lightDir) {
    bool receive = (light_params.z != 0.0);
    if (!receive) return 0.0;

    float viewDepth = abs(fragPosView.z);
    int ci = 2;
    if (viewDepth < cascade_splits.x)      ci = 0;
    else if (viewDepth < cascade_splits.y) ci = 1;

    float4 fragPosLS;
    if (ci == 0)      fragPosLS = mul(light_space_matrices[0], float4(fragPosWorld, 1.0));
    else if (ci == 1) fragPosLS = mul(light_space_matrices[1], float4(fragPosWorld, 1.0));
    else              fragPosLS = mul(light_space_matrices[2], float4(fragPosWorld, 1.0));

    float3 proj = fragPosLS.xyz / fragPosLS.w;
    proj.x = proj.x * 0.5 + 0.5;
    proj.y = -proj.y * 0.5 + 0.5;

    if (proj.z > 1.0) return 0.0;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) return 0.0;

    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
    float lit = 0.0;
    if (ci == 0)      lit = SampleShadowPCF(u_shadow_map0, u_cmp_sampler, proj, bias);
    else if (ci == 1) lit = SampleShadowPCF(u_shadow_map1, u_cmp_sampler, proj, bias);
    else              lit = SampleShadowPCF(u_shadow_map2, u_cmp_sampler, proj, bias);
    return clamp((1.0 - lit) * light_params.y, 0.0, 1.0);
}

float4 PSMain(PSInput input) : SV_TARGET {
    float4 texColor = u_texture.Sample(u_sampler, input.uv);
    float3 albedo_color = texColor.rgb * input.color.rgb * mat_albedo.rgb;

    float3 N = normalize(input.normal);
    bool has_normal_map = (mat_flags.x != 0.0);
    if (has_normal_map) {
        float3 normalMap = u_normal_map.Sample(u_sampler, input.uv).rgb;
        normalMap = normalMap * 2.0 - 1.0;
        normalMap.xy *= mat_roughness_ao.z;
        float3x3 TBN = float3x3(input.tangent, input.bitangent, input.normal);
        N = normalize(mul(normalMap, TBN));
    }

    bool lighting_enabled = (light_dir_and_enabled.w != 0.0);
    if (!lighting_enabled) {
        float3 result = albedo_color;
        bool has_emissive_map = (mat_flags.z != 0.0);
        if (has_emissive_map) {
            result += u_emissive_map.Sample(u_sampler, input.uv).rgb * mat_emissive.rgb;
        }
        result = result / (result + float3(1.0, 1.0, 1.0));
        result = pow(result, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));
        return float4(result, texColor.a * input.color.a);
    }

    float3 surface_albedo = pow(albedo_color, float3(2.2, 2.2, 2.2));
    float metallic = saturate(mat_albedo.w);
    float roughness = clamp(mat_roughness_ao.x, 0.04, 1.0);
    float ao = max(mat_roughness_ao.y, 0.0);

    bool has_mr_map = (mat_flags.y != 0.0);
    if (has_mr_map) {
        float4 mrSample = u_metallic_roughness_map.Sample(u_sampler, input.uv);
        roughness = clamp(mrSample.g * mat_roughness_ao.x, 0.04, 1.0);
        metallic = saturate(mrSample.b * mat_albedo.w);
    }
    bool has_occlusion_map = (mat_flags.w != 0.0);
    if (has_occlusion_map) {
        ao *= u_occlusion_map.Sample(u_sampler, input.uv).r;
    }

    float3 V = normalize(camera_pos.xyz - input.fragPos);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), surface_albedo, metallic);

    float3 L = normalize(-light_dir_and_enabled.xyz);
    float3 H = normalize(V + L);
    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    float3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float3 specular = (NDF * G * F) / max(4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001, 0.0001);
    float3 kD = (float3(1.0, 1.0, 1.0) - F) * (1.0 - metallic);
    float NdotL = max(dot(N, L), 0.0);
    float shadow = ShadowCalculation(input.fragPos, input.fragPosView, N, L);
    float3 Lo = (kD * surface_albedo / PI + specular) * light_color_and_ambient.rgb * light_params.x * NdotL * (1.0 - shadow);

    float3 ambient = float3(light_color_and_ambient.w, light_color_and_ambient.w, light_color_and_ambient.w) * surface_albedo * ao;
    float3 color = ambient + Lo + mat_emissive.rgb;
    color = color / (color + float3(1.0, 1.0, 1.0));
    color = pow(color, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));
    return float4(color, texColor.a * input.color.a);
}
)";

// ============================================================
// 天空盒着色器
// ============================================================

constexpr const char* kSkyboxVS = R"(
cbuffer PerFrame : register(b0) {
    float4x4 vp;
    float4x4 view;
    float4 camera_pos;
};

struct VSInput {
    float3 pos : POSITION;
};

struct VSOutput {
    float4 pos : SV_POSITION;
    float3 uv  : TEXCOORD0;
};

VSOutput VSMain(VSInput input) {
    VSOutput output;
    output.uv = input.pos;
    float3x3 rotView = (float3x3)view;
    float4 pos = mul(vp, float4(input.pos, 1.0));
    output.pos = pos.xyww;
    return output;
}
)";

constexpr const char* kSkyboxPS = R"(
TextureCube skybox : register(t0);
SamplerState u_sampler : register(s0);

struct PSInput {
    float4 pos : SV_POSITION;
    float3 uv  : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    return skybox.Sample(u_sampler, input.uv);
}
)";

// ============================================================
// 粒子着色器
// ============================================================

constexpr const char* kParticleVS = R"(
cbuffer PerFrame : register(b0) {
    float4x4 vp;
    float4x4 view;
    float4 camera_pos;
};

struct VSInput {
    float3 pos   : POSITION;
    float2 uv    : TEXCOORD0;
    float3 iPos  : INST_POS;
    float4 iCol  : INST_COLOR;
    float  iSize : INST_SIZE;
};

struct VSOutput {
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

VSOutput VSMain(VSInput input) {
    VSOutput output;
    float3 camera_right = float3(view[0][0], view[1][0], view[2][0]);
    float3 camera_up    = float3(view[0][1], view[1][1], view[2][1]);
    float3 worldPos = input.iPos
        + camera_right * input.pos.x * input.iSize
        + camera_up    * input.pos.y * input.iSize;
    output.pos = mul(vp, float4(worldPos, 1.0));
    output.col = input.iCol;
    output.uv  = input.uv;
    return output;
}
)";

constexpr const char* kParticlePS = R"(
Texture2D u_texture : register(t0);
SamplerState u_sampler : register(s0);

struct PSInput {
    float4 pos : SV_POSITION;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    float4 texColor = u_texture.Sample(u_sampler, input.uv);
    return texColor * input.col;
}
)";

// ============================================================
// 后处理着色器
// ============================================================

constexpr const char* kPostProcessVS = R"(
struct VSInput {
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
};

struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOutput VSMain(VSInput input) {
    VSOutput output;
    output.pos = float4(input.pos, 0.0, 1.0);
    output.uv  = input.uv;
    return output;
}
)";

constexpr const char* kPostProcessPassthroughPS = R"(
Texture2D screenTexture : register(t0);
SamplerState u_sampler : register(s0);

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    return screenTexture.Sample(u_sampler, input.uv);
}
)";

// ============================================================
// 阴影着色器（深度 only）
// ============================================================

constexpr const char* kShadowVS = R"(
cbuffer PerFrame : register(b0) {
    float4x4 vp;
    float4x4 view;
    float4 camera_pos;
};

cbuffer PerObject : register(b1) {
    float4x4 model;
    int skinned;
    int morph_enabled;
    int _pad0;
    int _pad1;
};

struct VSInput {
    float3 pos       : POSITION;
    float4 color     : COLOR0;
    float2 uv        : TEXCOORD0;
    float3 normal    : NORMAL;
    float3 tangent   : TANGENT;
    float4 boneWts   : BLENDWEIGHT;
    float4 boneIdx   : BLENDINDICES;
};

struct VSOutput {
    float4 pos : SV_POSITION;
};

VSOutput VSMain(VSInput input) {
    VSOutput output;
    output.pos = mul(vp, mul(model, float4(input.pos, 1.0)));
    return output;
}
)";

constexpr const char* kShadowPS = R"(
struct PSInput {
    float4 pos : SV_POSITION;
};

float4 PSMain(PSInput input) : SV_TARGET {
    return float4(0, 0, 0, 1);
}
)";

// ============================================================
// Bloom Compute Shader: 降采样（Downsample）
// ============================================================
// t0: 源纹理 (SRV), u0: 目标纹理 (UAV)
// 每个线程组 8x8，覆盖目标尺寸

constexpr const char* kBloomDownsampleCS = R"(
Texture2D<float4> src_tex : register(t0);
RWTexture2D<float4> dst_tex : register(u0);

SamplerState linear_sampler
{
    Filter   = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

cbuffer BloomParams : register(b0) {
    float2 src_texel_size;  // 1.0 / src 尺寸
    float2 dst_texel_size;  // 1.0 / dst 尺寸
};

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatch_id : SV_DispatchThreadID)
{
    // 目标像素中心 UV
    float2 uv = (dispatch_id.xy + 0.5) * dst_texel_size;

    // 13-tap Kawase 降采样
    float4 s = float4(0, 0, 0, 0);
    float2 hp = src_texel_size * 0.5;
    s += src_tex.SampleLevel(linear_sampler, uv + float2(-hp.x,  hp.y) * 2, 0) * 0.125;
    s += src_tex.SampleLevel(linear_sampler, uv + float2( hp.x,  hp.y) * 2, 0) * 0.125;
    s += src_tex.SampleLevel(linear_sampler, uv + float2(-hp.x, -hp.y) * 2, 0) * 0.125;
    s += src_tex.SampleLevel(linear_sampler, uv + float2( hp.x, -hp.y) * 2, 0) * 0.125;
    s += src_tex.SampleLevel(linear_sampler, uv, 0)                           * 0.125;
    s += src_tex.SampleLevel(linear_sampler, uv + float2(-hp.x,  0   ), 0)   * 0.0625;
    s += src_tex.SampleLevel(linear_sampler, uv + float2( hp.x,  0   ), 0)   * 0.0625;
    s += src_tex.SampleLevel(linear_sampler, uv + float2( 0,     hp.y), 0)   * 0.0625;
    s += src_tex.SampleLevel(linear_sampler, uv + float2( 0,    -hp.y), 0)   * 0.0625;
    s += src_tex.SampleLevel(linear_sampler, uv + float2(-hp.x,  hp.y), 0)   * 0.03125;
    s += src_tex.SampleLevel(linear_sampler, uv + float2( hp.x,  hp.y), 0)   * 0.03125;
    s += src_tex.SampleLevel(linear_sampler, uv + float2(-hp.x, -hp.y), 0)   * 0.03125;
    s += src_tex.SampleLevel(linear_sampler, uv + float2( hp.x, -hp.y), 0)   * 0.03125;

    dst_tex[dispatch_id.xy] = s;
}
)";

// ============================================================
// Bloom Compute Shader: 升采样（Upsample / Tent filter）
// ============================================================

constexpr const char* kBloomUpsampleCS = R"(
Texture2D<float4> src_tex : register(t0);
RWTexture2D<float4> dst_tex : register(u0);

SamplerState linear_sampler
{
    Filter   = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

cbuffer BloomParams : register(b0) {
    float2 src_texel_size;
    float2 dst_texel_size;
};

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatch_id : SV_DispatchThreadID)
{
    float2 uv = (dispatch_id.xy + 0.5) * dst_texel_size;

    // 3x3 tent 升采样
    float2 s = src_texel_size;
    float4 t = float4(0, 0, 0, 0);
    t += src_tex.SampleLevel(linear_sampler, uv + float2(-s.x,  s.y), 0) * 1.0;
    t += src_tex.SampleLevel(linear_sampler, uv + float2( 0,    s.y), 0) * 2.0;
    t += src_tex.SampleLevel(linear_sampler, uv + float2( s.x,  s.y), 0) * 1.0;
    t += src_tex.SampleLevel(linear_sampler, uv + float2(-s.x,  0  ), 0) * 2.0;
    t += src_tex.SampleLevel(linear_sampler, uv + float2( 0,    0  ), 0) * 4.0;
    t += src_tex.SampleLevel(linear_sampler, uv + float2( s.x,  0  ), 0) * 2.0;
    t += src_tex.SampleLevel(linear_sampler, uv + float2(-s.x, -s.y), 0) * 1.0;
    t += src_tex.SampleLevel(linear_sampler, uv + float2( 0,   -s.y), 0) * 2.0;
    t += src_tex.SampleLevel(linear_sampler, uv + float2( s.x, -s.y), 0) * 1.0;
    t /= 16.0;

    // 累加到目标（叠加模式）
    dst_tex[dispatch_id.xy] += t;
}
)";

// ============================================================
// Bloom 合成着色器（ACES Filmic Tone Mapping + Gamma）
// ============================================================

constexpr const char* kBloomCompositePS = R"(
Texture2D screenTexture : register(t0);
Texture2D bloomBlur     : register(t1);
SamplerState u_sampler  : register(s0);

cbuffer BloomCompositeParams : register(b0) {
    float exposure;
    float bloomIntensity;
    float2 _pad;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float3 AcesFilmic(float3 x) {
    float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 PSMain(PSInput input) : SV_TARGET {
    float3 color = screenTexture.Sample(u_sampler, input.uv).rgb;
    float3 bloom = bloomBlur.Sample(u_sampler, input.uv).rgb;
    color += bloom * bloomIntensity;
    color = AcesFilmic(color * exposure);
    color = pow(max(color, 0.0f), 1.0f / 2.2f);
    return float4(color, 1.0f);
}
)";

} // namespace dx11_shaders
} // namespace render
} // namespace dse

#endif // DSE_RENDER_DX11_SHADER_SOURCES_H
