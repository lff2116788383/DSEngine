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
SamplerState u_shadow_sampler : register(s1);

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

float SampleShadowMap(int cascade, float2 uv) {
    if (cascade == 0) return u_shadow_map0.Sample(u_shadow_sampler, uv).r;
    if (cascade == 1) return u_shadow_map1.Sample(u_shadow_sampler, uv).r;
    return u_shadow_map2.Sample(u_shadow_sampler, uv).r;
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

    float currentDepth = proj.z;
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);

    float shadow = 0.0;
    float texelSize = 1.0 / 1024.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcf = SampleShadowMap(ci, proj.xy + float2(x, y) * texelSize);
            shadow += (currentDepth - bias) > pcf ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    return clamp(shadow * light_params.y, 0.0, 1.0);
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

} // namespace dx11_shaders
} // namespace render
} // namespace dse

#endif // DSE_RENDER_DX11_SHADER_SOURCES_H
