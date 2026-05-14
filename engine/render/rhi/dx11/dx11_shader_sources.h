/**
 * @file dx11_shader_sources.h
 * @brief D3D11 专用 HLSL 5.0 内置着色器源码
 *
 * 所有着色器使用 Shader Model 5.0，入口点约定：
 * - 顶点着色器: VSMain
 * - 像素着色器: PSMain
 *
 * 常量缓冲布局约定（与 DX11DrawExecutor CB 对齐）：
 *   b0: PerFrame CB       (vp, view, camera_pos)
 *   b1: PerObject CB      (model, skinned, morph_enabled)
 *   b2: PerScene CB       (方向光/阴影参数)
 *   b3: PerMaterial CB    (材质参数)
 *   b4: PointLights CB    (点光源数组 208B)
 *   b5: SpotLights CB     (聚光灯数组 272B)
 *   b6: SpotMatrices CB   (聚光灯光源空间矩阵 256B)
 *
 * 纹理寄存器布局：
 *   t0-t4:  PBR 材质贴图 (albedo/normal/mr/emissive/occlusion)
 *   t5-t7:  CSM 阴影贴图 (方向光3级联)
 *   t8-t11: 点光源立方体阴影贴图 (TextureCube)
 *   t12-t15: 聚光灯阴影贴图 (Texture2D PCF)
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

cbuffer BoneMatrices : register(b7) {
    float4x4 u_bone_matrices[100];
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

    float4x4 skinMatrix = model;
    if (skinned) {
        skinMatrix = u_bone_matrices[int(input.boneIdx[0])] * input.boneWts[0] +
                     u_bone_matrices[int(input.boneIdx[1])] * input.boneWts[1] +
                     u_bone_matrices[int(input.boneIdx[2])] * input.boneWts[2] +
                     u_bone_matrices[int(input.boneIdx[3])] * input.boneWts[3];
    }

    float4 worldPos = mul(skinMatrix, float4(input.pos, 1.0));
    output.pos = mul(vp, worldPos);
    output.fragPos = worldPos.xyz;
    output.fragPosView = mul(view, worldPos).xyz;
    output.color = input.color;
    output.uv = input.uv;

    float3x3 normalMatrix = (float3x3)skinMatrix;
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

constexpr const char* kPbrPS_Part1 = R"(
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
    float4 mat_extra_params;
    float4 mat_extra_params2;
};

struct PointLightEntry {
    float3 color;     float intensity;
    float3 position;  float radius;
    int cast_shadow;  int shadow_index;
    int _pad0;        int _pad1;
};
cbuffer PointLights : register(b4) {
    int u_point_light_count;
    int _pl_pad0, _pl_pad1, _pl_pad2;
    PointLightEntry u_point_lights[64];
};

struct SpotLightEntry {
    float3 color;       float intensity;
    float3 position;    float radius;
    float3 direction;   float inner_cone;
    float outer_cone;   int cast_shadow;
    int shadow_index;   float _pad0;
};
cbuffer SpotLights : register(b5) {
    int u_spot_light_count;
    int _sl_pad0, _sl_pad1, _sl_pad2;
    SpotLightEntry u_spot_lights[64];
};

cbuffer SpotMatrices : register(b6) {
    float4x4 u_spot_light_space_matrices[4];
};

cbuffer LightProbeData : register(b9) {
    float4 sh_coefficients[9];
    float4 probe_params;
};

Texture2D u_texture : register(t0);
Texture2D u_normal_map : register(t1);
Texture2D u_metallic_roughness_map : register(t2);
Texture2D u_emissive_map : register(t3);
Texture2D u_occlusion_map : register(t4);
Texture2D u_shadow_map0 : register(t5);
Texture2D u_shadow_map1 : register(t6);
Texture2D u_shadow_map2 : register(t7);
TextureCube u_point_shadow_map0 : register(t8);
TextureCube u_point_shadow_map1 : register(t9);
TextureCube u_point_shadow_map2 : register(t10);
TextureCube u_point_shadow_map3 : register(t11);
Texture2D u_spot_shadow_map0 : register(t12);
Texture2D u_spot_shadow_map1 : register(t13);
Texture2D u_spot_shadow_map2 : register(t14);
Texture2D u_spot_shadow_map3 : register(t15);

SamplerState u_sampler : register(s0);
SamplerComparisonState u_cmp_sampler : register(s1);

static const float PI = 3.14159265359;

float2 ParallaxOcclusionMapping(float2 uv, float3 viewDirTS, float height_scale) {
    const int numLayers = 16;
    float layerDepth = 1.0 / (float)numLayers;
    float currentLayerDepth = 0.0;
    float2 P = viewDirTS.xy / max(viewDirTS.z, 0.001) * height_scale;
    float2 deltaUV = P / (float)numLayers;
    float2 curUV = uv;
    float curDepth = 1.0 - u_normal_map.Sample(u_sampler, curUV).a;
    [loop] for (int i = 0; i < numLayers; ++i) {
        if (currentLayerDepth >= curDepth) break;
        curUV -= deltaUV;
        curDepth = 1.0 - u_normal_map.Sample(u_sampler, curUV).a;
        currentLayerDepth += layerDepth;
    }
    float2 prevUV = curUV + deltaUV;
    float afterDepth = curDepth - currentLayerDepth;
    float beforeDepth = (1.0 - u_normal_map.Sample(u_sampler, prevUV).a) - currentLayerDepth + layerDepth;
    float w = afterDepth / (afterDepth - beforeDepth + 0.0001);
    return lerp(curUV, prevUV, w);
}

float DistributionGGXAniso(float3 N, float3 H, float3 T, float3 B, float roughness, float aniso) {
    float at = max(roughness * (1.0 + aniso), 0.001);
    float ab = max(roughness * (1.0 - aniso), 0.001);
    float TdotH = dot(T, H);
    float BdotH = dot(B, H);
    float NdotH = dot(N, H);
    float d = TdotH*TdotH/(at*at) + BdotH*BdotH/(ab*ab) + NdotH*NdotH;
    return 1.0 / (PI * at * ab * d * d + 0.0001);
}

float3 SubsurfaceScattering(float3 N, float3 L, float3 alb, float sss, float3 lc, float li, float3 tint) {
    float wrap = 0.5 * sss;
    float NdotL_wrap = max(0.0, (dot(N, L) + wrap) / (1.0 + wrap));
    float NdotL_std  = max(dot(N, L), 0.0);
    float diff = NdotL_wrap - NdotL_std;
    float3 sss_tint = (dot(tint, tint) > 0.0) ? tint : float3(1.0, 0.35, 0.2);
    return alb * sss_tint * diff * lc * li;
}

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

float3 EvaluateSH(float3 N) {
    float3 result = sh_coefficients[0].xyz *  0.282095
                  + sh_coefficients[1].xyz *  0.488603 * N.y
                  + sh_coefficients[2].xyz *  0.488603 * N.z
                  + sh_coefficients[3].xyz *  0.488603 * N.x
                  + sh_coefficients[4].xyz *  1.092548 * N.x * N.y
                  + sh_coefficients[5].xyz *  1.092548 * N.y * N.z
                  + sh_coefficients[6].xyz *  0.315392 * (3.0 * N.z * N.z - 1.0)
                  + sh_coefficients[7].xyz *  1.092548 * N.x * N.z
                  + sh_coefficients[8].xyz *  0.546274 * (N.x * N.x - N.y * N.y);
    return max(result, float3(0.0, 0.0, 0.0));
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

// PCSS: Blocker search -> penumbra estimation -> variable-size PCF
#define PCSS_BLOCKER_SAMPLES 16
#define PCSS_PCF_SAMPLES 25
#define PCSS_LIGHT_SIZE 0.04

static const float2 kPoissonDisk16[16] = {
    float2(-0.94201624, -0.39906216), float2( 0.94558609, -0.76890725),
    float2(-0.09418410, -0.92938870), float2( 0.34495938,  0.29387760),
    float2(-0.91588581,  0.45771432), float2(-0.81544232, -0.87912464),
    float2(-0.38277543,  0.27676845), float2( 0.97484398,  0.75648379),
    float2( 0.44323325, -0.97511554), float2( 0.53742981, -0.47373420),
    float2(-0.26496911, -0.41893023), float2( 0.79197514,  0.19090188),
    float2(-0.24188840,  0.99706507), float2(-0.81409955,  0.91437590),
    float2( 0.19984126,  0.78641367), float2( 0.14383161, -0.14100790)
};

static const float2 kPoissonDisk25[25] = {
    float2(-0.86804624, -0.18409416), float2(-0.60420109, -0.55890725),
    float2(-0.33418410, -0.83238870), float2(-0.05504062,  0.11387760),
    float2(-0.76588581,  0.32771432), float2(-0.53544232, -0.22912464),
    float2(-0.20277543,  0.55676845), float2( 0.06484398,  0.82648379),
    float2( 0.33323325, -0.67511554), float2( 0.59742981, -0.01373420),
    float2( 0.85496911,  0.34893023), float2(-0.94197514, -0.69090188),
    float2(-0.45188840,  0.87706507), float2( 0.18409955,  0.49437590),
    float2( 0.42984126, -0.35641367), float2( 0.68383161,  0.71100790),
    float2(-0.14201624,  0.01906216), float2( 0.12558609, -0.94890725),
    float2( 0.37418410,  0.26938870), float2( 0.61495938, -0.45387760),
    float2( 0.85588581,  0.03228568), float2(-0.67544232,  0.65087536),
    float2(-0.36277543, -0.44323155), float2( 0.90484398, -0.24351621),
    float2(-0.00676675,  0.30488446)
};

float FindBlockerDepthDX(Texture2D shadowMap, SamplerComparisonState cmp_sampler,
                          float3 projCoords, float bias, float searchRadius) {
    float blockerSum = 0.0;
    int blockerCount = 0;
    float2 texelSize = 1.0 / float2(2048.0, 2048.0);
    float receiverDepth = projCoords.z - bias;
    [unroll] for (int i = 0; i < PCSS_BLOCKER_SAMPLES; ++i) {
        float2 offset = kPoissonDisk16[i] * searchRadius * texelSize;
        float sampleLit = shadowMap.SampleCmpLevelZero(cmp_sampler, projCoords.xy + offset, receiverDepth);
        if (sampleLit < 0.5) {
            blockerSum += receiverDepth;
            blockerCount++;
        }
    }
    return blockerCount > 0 ? blockerSum / float(blockerCount) : -1.0;
}

float PCSS_ShadowDX(Texture2D shadowMap, SamplerComparisonState cmp_sampler,
                     float3 projCoords, float bias) {
    float searchRadius = PCSS_LIGHT_SIZE * projCoords.z * 20.0;
    searchRadius = clamp(searchRadius, 1.0, 12.0);
    float avgBlockerDepth = FindBlockerDepthDX(shadowMap, cmp_sampler, projCoords, bias, searchRadius);
    if (avgBlockerDepth < 0.0) return 1.0;
    float receiverDepth = projCoords.z - bias;
    float penumbraWidth = (receiverDepth - avgBlockerDepth) / max(avgBlockerDepth, 0.001) * PCSS_LIGHT_SIZE * 40.0;
    penumbraWidth = clamp(penumbraWidth, 1.0, 10.0);
    float shadow = 0.0;
    float2 texelSize = 1.0 / float2(2048.0, 2048.0);
    [unroll] for (int i = 0; i < PCSS_PCF_SAMPLES; ++i) {
        float2 offset = kPoissonDisk25[i] * penumbraWidth * texelSize;
        shadow += shadowMap.SampleCmpLevelZero(cmp_sampler, projCoords.xy + offset, receiverDepth);
    }
    return shadow / float(PCSS_PCF_SAMPLES);
}

float ShadowForCascade(int ci, float3 fragPosWorld, float3 normal, float3 lightDir) {
    float4 fragPosLS;
    if (ci == 0)      fragPosLS = mul(light_space_matrices[0], float4(fragPosWorld, 1.0));
    else if (ci == 1) fragPosLS = mul(light_space_matrices[1], float4(fragPosWorld, 1.0));
    else              fragPosLS = mul(light_space_matrices[2], float4(fragPosWorld, 1.0));

    float3 proj = fragPosLS.xyz / fragPosLS.w;
    proj.x = proj.x * 0.5 + 0.5;
    proj.y = -proj.y * 0.5 + 0.5;
    proj.z = proj.z * 0.5 + 0.5;

    if (proj.z > 1.0) return 0.0;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0) return 0.0;

    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
    float lit = 0.0;
    if (ci == 0)      lit = PCSS_ShadowDX(u_shadow_map0, u_cmp_sampler, proj, bias);
    else if (ci == 1) lit = PCSS_ShadowDX(u_shadow_map1, u_cmp_sampler, proj, bias);
    else              lit = PCSS_ShadowDX(u_shadow_map2, u_cmp_sampler, proj, bias);
    return 1.0 - lit;
}

float ShadowCalculation(float3 fragPosWorld, float3 fragPosView, float3 normal, float3 lightDir) {
    bool receive = (light_params.z != 0.0);
    if (!receive) return 0.0;

    float viewDepth = abs(fragPosView.z);
    int ci = 2;
    if (viewDepth < cascade_splits.x)      ci = 0;
    else if (viewDepth < cascade_splits.y) ci = 1;

    float shadow = ShadowForCascade(ci, fragPosWorld, normal, lightDir);

    // 级联边界 smoothstep 混合：在当前级联范围末尾 20% 区域混合到下一级
    if (ci < 2) {
        float splitEnd = (ci == 0) ? cascade_splits.x : cascade_splits.y;
        float blendStart = splitEnd * 0.8;
        if (viewDepth > blendStart) {
            float blendFactor = smoothstep(blendStart, splitEnd, viewDepth);
            float nextShadow = ShadowForCascade(ci + 1, fragPosWorld, normal, lightDir);
            shadow = lerp(shadow, nextShadow, blendFactor);
        }
    }

    return clamp(shadow * light_params.y, 0.0, 1.0);
}
)";

constexpr const char* kPbrPS_Part2 = R"(
float4 PSMain(PSInput input) : SV_TARGET {
    float2 finalUV = input.uv;
    bool has_normal_map = (mat_flags.x != 0.0);
    if (mat_extra_params2.x > 0.0 && has_normal_map) {
        float3x3 TBN_t = float3x3(input.tangent, input.bitangent, input.normal);
        float3 viewDirTS = mul(TBN_t, normalize(camera_pos.xyz - input.fragPos));
        finalUV = ParallaxOcclusionMapping(input.uv, viewDirTS, mat_extra_params2.x);
    }
    float4 texColor = u_texture.Sample(u_sampler, finalUV);
    float3 albedo_color = texColor.rgb * input.color.rgb * mat_albedo.rgb;

    float3 N = normalize(input.normal);
    if (has_normal_map) {
        float3 normalMap = u_normal_map.Sample(u_sampler, finalUV).rgb;
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
            result += u_emissive_map.Sample(u_sampler, finalUV).rgb * mat_emissive.rgb;
        }
        if (light_params.w == 0.0) {
            result = result / (result + float3(1.0, 1.0, 1.0));
            result = pow(result, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));
        }
        return float4(result, texColor.a * input.color.a);
    }

    // Half-Lambert shading mode (KF-style, light_params.w == 2.0)
    if (light_params.w == 2.0) {
        float3 L = normalize(-light_dir_and_enabled.xyz);
        float3 V_hl = normalize(camera_pos.xyz - input.fragPos);
        float3 R = reflect(light_dir_and_enabled.xyz, N);
        float half_lambert = dot(N, L) * 0.5 + 0.5;
        float3 diffuse_color = albedo_color * half_lambert;
        float spec_brightness = pow(max(dot(R, V_hl), 0.0), 100.0);
        bool has_mr_map_hl = (mat_flags.y != 0.0);
        float3 spec_tex = has_mr_map_hl
            ? u_metallic_roughness_map.Sample(u_sampler, input.uv).rgb
            : float3(0.0, 0.0, 0.0);
        float3 specular_color = spec_tex * spec_brightness;
        float shadow = ShadowCalculation(input.fragPos, input.fragPosView, N, L);
        float shadow_multiplier = 1.0 - shadow * 0.5;
        float3 color = (diffuse_color + specular_color) * shadow_multiplier;
        return float4(color, 1.0);
    }

    // Half-Lambert STATIC shading mode (KF default_pixel_shader, light_params.w == 3.0)
    if (light_params.w == 3.0) {
        float3 L = normalize(-light_dir_and_enabled.xyz);
        float3 V_st = normalize(camera_pos.xyz - input.fragPos);
        float3 R = reflect(light_dir_and_enabled.xyz, N);
        float half_lambert = dot(N, L) * 0.5 + 0.5;
        float3 diffuse = mat_albedo.rgb * half_lambert * light_color_and_ambient.rgb * light_params.x;
        float spec_power = max(mat_roughness_ao.x, 1.0);
        float3 spec_color = float3(mat_albedo.w, mat_albedo.w, mat_albedo.w);
        float3 specular = spec_color * pow(max(dot(R, V_st), 0.0), spec_power);
        float3 emissive_val = mat_emissive.rgb;
        float3 material_color = diffuse + specular + emissive_val;
        float3 color_st = material_color * texColor.rgb * input.color.rgb;
        float shadow = ShadowCalculation(input.fragPos, input.fragPosView, N, L);
        float shadow_multiplier = 1.0 - shadow * 0.5;
        return float4(color_st * shadow_multiplier, texColor.a * input.color.a);
    }

    float3 surface_albedo = pow(albedo_color, float3(2.2, 2.2, 2.2));
    float metallic = saturate(mat_albedo.w);
    float roughness = clamp(mat_roughness_ao.x, 0.04, 1.0);
    float ao = max(mat_roughness_ao.y, 0.0);

    bool has_mr_map = (mat_flags.y != 0.0);
    if (has_mr_map) {
        float4 mrSample = u_metallic_roughness_map.Sample(u_sampler, finalUV);
        roughness = clamp(mrSample.g * mat_roughness_ao.x, 0.04, 1.0);
        metallic = saturate(mrSample.b * mat_albedo.w);
    }
    bool has_occlusion_map = (mat_flags.w != 0.0);
    if (has_occlusion_map) {
        ao *= u_occlusion_map.Sample(u_sampler, finalUV).r;
    }

    float3 V = normalize(camera_pos.xyz - input.fragPos);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), surface_albedo, metallic);
    float3 T_dir = normalize(input.tangent);
    float3 B_dir = normalize(input.bitangent);

    float3 L = normalize(-light_dir_and_enabled.xyz);
    float3 H = normalize(V + L);
    float NDF = (mat_extra_params.w != 0.0) ? DistributionGGXAniso(N, H, T_dir, B_dir, roughness, mat_extra_params.w) : DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    float3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float3 specular = (NDF * G * F) / max(4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001, 0.0001);
    float3 kD = (float3(1.0, 1.0, 1.0) - F) * (1.0 - metallic);
    float NdotL = max(dot(N, L), 0.0);
    float shadow = ShadowCalculation(input.fragPos, input.fragPosView, N, L);
    float3 Lo = (kD * surface_albedo / PI + specular) * light_color_and_ambient.rgb * light_params.x * NdotL * (1.0 - shadow);
    if (mat_extra_params.x > 0.0)
        Lo += SubsurfaceScattering(N, L, surface_albedo, mat_extra_params.x, light_color_and_ambient.rgb, light_params.x, mat_extra_params2.yzw) * (1.0 - shadow);
    if (mat_extra_params.y > 0.0) {
        float cc_r = max(mat_extra_params.z, 0.04);
        float NDF_cc = DistributionGGX(N, H, cc_r);
        float G_cc = GeometrySmith(N, V, L, cc_r);
        float3 F_cc = fresnelSchlick(max(dot(H, V), 0.0), float3(0.04, 0.04, 0.04));
        float3 spec_cc = (NDF_cc * G_cc * F_cc) / max(4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001, 0.0001);
        Lo += spec_cc * mat_extra_params.y * NdotL * light_color_and_ambient.rgb * light_params.x * (1.0 - shadow);
    }

)" R"(
    // 聚光灯 PBR 循环
    [loop] for (int si = 0; si < u_spot_light_count; ++si) {
        SpotLightEntry sl = u_spot_lights[si];
        float3 Ls = sl.position - input.fragPos;
        float dist_s = length(Ls);
        if (dist_s >= sl.radius) continue;
        float atten_s = saturate(1.0 - (dist_s * dist_s) / (sl.radius * sl.radius));
        atten_s *= atten_s;
        float3 Lsdir = Ls / max(dist_s, 0.0001);
        float theta_s = dot(Lsdir, normalize(-sl.direction));
        float outerCos = cos(radians(sl.outer_cone));
        float innerCos = cos(radians(sl.inner_cone));
        float epsilon_s = max(innerCos - outerCos, 0.0001);
        float cone = saturate((theta_s - outerCos) / epsilon_s);
        if (cone <= 0.0) continue;
        float3 Hs = normalize(V + Lsdir);
        float NDFs = DistributionGGX(N, Hs, roughness);
        float Gs   = GeometrySmith(N, V, Lsdir, roughness);
        float3 Fs  = fresnelSchlick(max(dot(Hs, V), 0.0), F0);
        float3 specS = (NDFs * Gs * Fs) / max(4.0 * max(dot(N, V), 0.0) * max(dot(N, Lsdir), 0.0) + 0.0001, 0.0001);
        float3 kDs = (float3(1.0, 1.0, 1.0) - Fs) * (1.0 - metallic);
        float NdotLs = max(dot(N, Lsdir), 0.0);
        float sl_shadow = 0.0;
        if (sl.cast_shadow != 0) {
            float4x4 lsm = (sl.shadow_index == 1) ? u_spot_light_space_matrices[1] :
                           (sl.shadow_index == 2) ? u_spot_light_space_matrices[2] :
                           (sl.shadow_index >= 3) ? u_spot_light_space_matrices[3] :
                                                    u_spot_light_space_matrices[0];
            float4 fragPosLS = mul(lsm, float4(input.fragPos, 1.0));
            float3 sproj = fragPosLS.xyz / fragPosLS.w;
            sproj.x = sproj.x * 0.5 + 0.5;
            sproj.y = -sproj.y * 0.5 + 0.5;
            sproj.z = sproj.z * 0.5 + 0.5;
            if (sproj.z <= 1.0 && sproj.x >= 0.0 && sproj.x <= 1.0 && sproj.y >= 0.0 && sproj.y <= 1.0) {
                float sbias = max(0.003 * (1.0 - dot(N, Lsdir)), 0.0005);
                float lit_s = 0.0;
                if      (sl.shadow_index == 0) lit_s = SampleShadowPCF(u_spot_shadow_map0, u_cmp_sampler, sproj, sbias);
                else if (sl.shadow_index == 1) lit_s = SampleShadowPCF(u_spot_shadow_map1, u_cmp_sampler, sproj, sbias);
                else if (sl.shadow_index == 2) lit_s = SampleShadowPCF(u_spot_shadow_map2, u_cmp_sampler, sproj, sbias);
                else                           lit_s = SampleShadowPCF(u_spot_shadow_map3, u_cmp_sampler, sproj, sbias);
                sl_shadow = clamp((1.0 - lit_s) * light_params.y, 0.0, 1.0);
            }
        }
        Lo += (kDs * surface_albedo / PI + specS) * sl.color * sl.intensity * atten_s * cone * NdotLs * (1.0 - sl_shadow);
    }

)" R"(
    // 点光源 PBR 循环
    [loop] for (int pi = 0; pi < u_point_light_count; ++pi) {
        PointLightEntry pl = u_point_lights[pi];
        float3 Lp = pl.position - input.fragPos;
        float dist = length(Lp);
        if (dist >= pl.radius) continue;
        float atten = 1.0 / max(dist * dist, 0.0001);
        float3 Ldir = Lp / max(dist, 0.0001);
        float3 Hp = normalize(V + Ldir);
        float NDFp = DistributionGGX(N, Hp, roughness);
        float Gp   = GeometrySmith(N, V, Ldir, roughness);
        float3 Fp  = fresnelSchlick(max(dot(Hp, V), 0.0), F0);
        float3 specP = (NDFp * Gp * Fp) / max(4.0 * max(dot(N, V), 0.0) * max(dot(N, Ldir), 0.0) + 0.0001, 0.0001);
        float3 kDp = (float3(1.0, 1.0, 1.0) - Fp) * (1.0 - metallic);
        float NdotLp = max(dot(N, Ldir), 0.0);
        float pl_shadow = 0.0;
        if (pl.cast_shadow != 0) {
            float3 sampleDir = input.fragPos - pl.position;
            float closestDepth = 0.0;
            if      (pl.shadow_index == 0) closestDepth = u_point_shadow_map0.Sample(u_sampler, sampleDir).r;
            else if (pl.shadow_index == 1) closestDepth = u_point_shadow_map1.Sample(u_sampler, sampleDir).r;
            else if (pl.shadow_index == 2) closestDepth = u_point_shadow_map2.Sample(u_sampler, sampleDir).r;
            else                           closestDepth = u_point_shadow_map3.Sample(u_sampler, sampleDir).r;
            float currentDepth = dist / pl.radius;
            pl_shadow = (currentDepth - 0.005 > closestDepth) ? 1.0 : 0.0;
        }
        Lo += (kDp * surface_albedo / PI + specP) * pl.color * pl.intensity * atten * NdotLp * (1.0 - pl_shadow);
    }

    float3 sh_irradiance = (probe_params.x > 0.5) ? EvaluateSH(N) : float3(light_color_and_ambient.w, light_color_and_ambient.w, light_color_and_ambient.w);
    float3 ambient = sh_irradiance * surface_albedo * ao;
    if (mat_extra_params.y > 0.0) {
        float3 F_cc_amb = fresnelSchlick(max(dot(N, V), 0.0), float3(0.04, 0.04, 0.04));
        ambient += F_cc_amb * mat_extra_params.y * sh_irradiance * (1.0 - mat_extra_params.z) * 0.25;
    }
    float3 surface_emissive = mat_emissive.rgb;
    bool has_emissive_map = (mat_flags.z != 0.0);
    if (has_emissive_map) {
        surface_emissive *= u_emissive_map.Sample(u_sampler, finalUV).rgb;
    }
    float3 color = ambient + Lo + surface_emissive;
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

cbuffer BoneMatrices : register(b7) {
    float4x4 u_bone_matrices[100];
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
    float4x4 skinMatrix = model;
    if (skinned) {
        skinMatrix = u_bone_matrices[int(input.boneIdx[0])] * input.boneWts[0] +
                     u_bone_matrices[int(input.boneIdx[1])] * input.boneWts[1] +
                     u_bone_matrices[int(input.boneIdx[2])] * input.boneWts[2] +
                     u_bone_matrices[int(input.boneIdx[3])] * input.boneWts[3];
    }
    output.pos = mul(vp, mul(skinMatrix, float4(input.pos, 1.0)));
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

// ============================================================
// FXAA 像素着色器
// ============================================================

constexpr const char* kFxaaPS = R"(
Texture2D screenTexture : register(t0);
SamplerState u_sampler  : register(s0);

cbuffer FxaaParams : register(b0) {
    float2 u_resolution;
    float2 _pad;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float luma(float3 c) { return dot(c, float3(0.299, 0.587, 0.114)); }

float4 PSMain(PSInput input) : SV_TARGET {
    float2 texel = 1.0 / u_resolution;
    float lumaM  = luma(screenTexture.Sample(u_sampler, input.uv).rgb);
    float lumaNW = luma(screenTexture.Sample(u_sampler, input.uv + float2(-1.0,-1.0) * texel).rgb);
    float lumaNE = luma(screenTexture.Sample(u_sampler, input.uv + float2( 1.0,-1.0) * texel).rgb);
    float lumaSW = luma(screenTexture.Sample(u_sampler, input.uv + float2(-1.0, 1.0) * texel).rgb);
    float lumaSE = luma(screenTexture.Sample(u_sampler, input.uv + float2( 1.0, 1.0) * texel).rgb);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;
    if (lumaRange < max(0.0312, lumaMax * 0.125)) {
        return screenTexture.Sample(u_sampler, input.uv);
    }
    float2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * 0.25, 1.0/128.0);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(float2(8.0,8.0), max(float2(-8.0,-8.0), dir * rcpDirMin)) * texel;
    float3 rgbA = 0.5 * (
        screenTexture.Sample(u_sampler, input.uv + dir * (1.0/3.0 - 0.5)).rgb +
        screenTexture.Sample(u_sampler, input.uv + dir * (2.0/3.0 - 0.5)).rgb);
    float3 rgbB = rgbA * 0.5 + 0.25 * (
        screenTexture.Sample(u_sampler, input.uv + dir * -0.5).rgb +
        screenTexture.Sample(u_sampler, input.uv + dir *  0.5).rgb);
    float lumaB = luma(rgbB);
    if (lumaB < lumaMin || lumaB > lumaMax)
        return float4(rgbA, 1.0);
    else
        return float4(rgbB, 1.0);
}
)";

// ============================================================
// SSAO 像素着色器
// ============================================================

constexpr const char* kSsaoPS = R"(
Texture2D screenTexture : register(t0);
SamplerState u_sampler  : register(s0);

cbuffer SsaoParams : register(b0) {
    float u_radius;
    float u_bias;
    float u_near;
    float u_far;
    float2 u_screen_size;
    float2 _pad;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
}

float3 reconstructNormal(float2 uv) {
    float2 texel = 1.0 / u_screen_size;
    float dc = linearizeDepth(screenTexture.Sample(u_sampler, uv).r);
    float dl = linearizeDepth(screenTexture.Sample(u_sampler, uv - float2(texel.x, 0.0)).r);
    float dr = linearizeDepth(screenTexture.Sample(u_sampler, uv + float2(texel.x, 0.0)).r);
    float db = linearizeDepth(screenTexture.Sample(u_sampler, uv - float2(0.0, texel.y)).r);
    float dt = linearizeDepth(screenTexture.Sample(u_sampler, uv + float2(0.0, texel.y)).r);
    return normalize(float3(dl - dr, db - dt, 2.0 * texel.x * dc));
}

static const float3 kernel[16] = {
    float3( 0.5381, 0.1856,-0.4319), float3( 0.1379, 0.2486, 0.4430),
    float3( 0.3371, 0.5679,-0.0057), float3(-0.6999,-0.0451,-0.0019),
    float3( 0.0689,-0.1598,-0.8547), float3( 0.0560, 0.0069,-0.1843),
    float3(-0.0146, 0.1402, 0.0762), float3( 0.0100,-0.1924,-0.0344),
    float3(-0.3577,-0.5301,-0.4358), float3(-0.3169, 0.1063, 0.0158),
    float3( 0.0103,-0.5869, 0.0046), float3(-0.0897,-0.4940, 0.3287),
    float3( 0.7119,-0.0154,-0.0918), float3(-0.0533, 0.0596,-0.5411),
    float3( 0.0352,-0.0631, 0.5460), float3(-0.4776, 0.2847,-0.0271)
};

float4 PSMain(PSInput input) : SV_TARGET {
    float depth = screenTexture.Sample(u_sampler, input.uv).r;
    if (depth >= 1.0) return float4(1.0, 1.0, 1.0, 1.0);
    float linDepth = linearizeDepth(depth);
    float3 normal = reconstructNormal(input.uv);
    float occlusion = 0.0;
    float rScale = u_radius / linDepth;
    for (int i = 0; i < 16; ++i) {
        float3 sampleDir = kernel[i];
        if (dot(sampleDir, normal) < 0.0) sampleDir = -sampleDir;
        float2 sampleUV = input.uv + sampleDir.xy * rScale * (1.0 / u_screen_size);
        float sampleDepth = linearizeDepth(screenTexture.Sample(u_sampler, sampleUV).r);
        float rangeCheck = smoothstep(0.0, 1.0, u_radius / abs(linDepth - sampleDepth));
        if (sampleDepth < linDepth - u_bias) occlusion += rangeCheck;
    }
    occlusion = 1.0 - (occlusion / 16.0);
    return float4(occlusion, occlusion, occlusion, 1.0);
}
)";

// ============================================================
// SSAO 模糊像素着色器
// ============================================================

constexpr const char* kSsaoBlurPS = R"(
Texture2D screenTexture : register(t0);
SamplerState u_sampler  : register(s0);

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    uint w, h;
    screenTexture.GetDimensions(w, h);
    float2 texelSize = 1.0 / float2(w, h);
    float result = 0.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            float2 offset = float2(x, y) * texelSize;
            result += screenTexture.Sample(u_sampler, input.uv + offset).r;
        }
    }
    float v = result / 25.0;
    return float4(v, v, v, 1.0);
}
)";

// ============================================================
// Contact Shadow 像素着色器
// ============================================================

constexpr const char* kContactShadowPS = R"(
Texture2D screenTexture : register(t0);
SamplerState u_sampler  : register(s0);

cbuffer ContactShadowParams : register(b0) {
    float3 u_light_dir;
    float u_near;
    float u_far;
    float2 u_screen_size;
    float u_strength;
    float u_step_size;
    int u_num_steps;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
}

float4 PSMain(PSInput input) : SV_TARGET {
    float depth = screenTexture.Sample(u_sampler, input.uv).r;
    if (depth >= 1.0) return float4(1.0, 1.0, 1.0, 1.0);
    float linDepth = linearizeDepth(depth);
    float3 lightDir = normalize(u_light_dir);
    float2 texelSize = 1.0 / u_screen_size;
    float occlusion = 0.0;
    int validSteps = 0;
    for (int i = 1; i <= u_num_steps; ++i) {
        float dist = u_step_size * float(i);
        float2 sampleUV = input.uv + lightDir.xy * texelSize * dist * 50.0;
        if (sampleUV.x < 0.0 || sampleUV.y < 0.0 || sampleUV.x > 1.0 || sampleUV.y > 1.0) break;
        float sampleDepth = screenTexture.Sample(u_sampler, sampleUV).r;
        if (sampleDepth >= 1.0) continue;
        float sampleLin = linearizeDepth(sampleDepth);
        float diff = sampleLin - linDepth;
        if (diff > 0.0 && diff < u_step_size) {
            float k = 1.0 - (diff / u_step_size);
            occlusion += k * k;
        }
        ++validSteps;
    }
    float shadow = validSteps > 0 ? 1.0 - clamp(occlusion / float(validSteps) * u_strength, 0.0, 1.0) : 1.0;
    return float4(shadow, shadow, shadow, 1.0);
}
)";

// ============================================================
// SSAO 应用像素着色器（带 tone mapping）
// ============================================================

constexpr const char* kSsaoApplyPS = R"(
Texture2D screenTexture    : register(t0);
Texture2D ssaoTexture      : register(t1);
Texture2D autoExposureTex  : register(t2);
Texture3D lutTexture       : register(t4);
SamplerState u_sampler     : register(s0);
SamplerState u_lut_sampler : register(s1);

cbuffer SsaoApplyParams : register(b0) {
    float exposure;
    int autoExposureEnabled;
    int lutEnabled;
    float lutIntensity;
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
    float3 hdrColor = screenTexture.Sample(u_sampler, input.uv).rgb;
    float ao = ssaoTexture.Sample(u_sampler, input.uv).r;
    hdrColor *= ao;
    float finalExposure = exposure;
    if (autoExposureEnabled) {
        finalExposure = autoExposureTex.Sample(u_sampler, float2(0.5, 0.5)).r;
    }
    float3 result = AcesFilmic(hdrColor * finalExposure);
    result = pow(max(result, 0.0f), 1.0 / 2.2);
    if (lutEnabled) {
        float3 lutColor = lutTexture.Sample(u_lut_sampler, saturate(result)).rgb;
        result = lerp(result, lutColor, lutIntensity);
    }
    return float4(result, 1.0);
}
)";

// ============================================================
// Bloom Composite + SSAO 像素着色器
// ============================================================

constexpr const char* kBloomCompositeSsaoPS = R"(
Texture2D screenTexture : register(t0);
Texture2D bloomBlur     : register(t1);
Texture2D ssaoTexture   : register(t2);
SamplerState u_sampler  : register(s0);

cbuffer BloomCompositeSsaoParams : register(b0) {
    float exposure;
    float bloomIntensity;
    int ssaoEnabled;
    float _pad;
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
    if (ssaoEnabled) {
        float ao = ssaoTexture.Sample(u_sampler, input.uv).r;
        color *= ao;
    }
    float3 bloom = bloomBlur.Sample(u_sampler, input.uv).rgb;
    color += bloom * bloomIntensity;
    color = AcesFilmic(color * exposure);
    color = pow(max(color, 0.0f), 1.0f / 2.2f);
    return float4(color, 1.0f);
}
)";

// ============================================================
// Auto Exposure: Luminance 计算 (64 样本 log 平均)
// ============================================================

constexpr const char* kLumComputePS = R"(
Texture2D screenTexture : register(t0);
SamplerState u_sampler  : register(s0);

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    float logSum = 0.0;
    [unroll]
    for (int i = 0; i < 8; i++) {
        [unroll]
        for (int j = 0; j < 8; j++) {
            float2 uv = (float2(i, j) + 0.5) / 8.0;
            float3 c = screenTexture.Sample(u_sampler, uv).rgb;
            float lum = dot(c, float3(0.2126, 0.7152, 0.0722));
            logSum += log(max(lum, 0.0001));
        }
    }
    float avgLogLum = logSum / 64.0;
    return float4(avgLogLum, 0.0, 0.0, 1.0);
}
)";

// ============================================================
// Auto Exposure: EMA 自适应曝光
// ============================================================

constexpr const char* kLumAdaptPS = R"(
Texture2D screenTexture  : register(t0);
Texture2D prevAdaptedTex : register(t1);
SamplerState u_sampler   : register(s0);

cbuffer LumAdaptParams : register(b0) {
    float u_dt;
    float u_speed_up;
    float u_speed_down;
    float u_min_exposure;
    float u_max_exposure;
    float u_compensation;
    float2 _pad;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    float avgLogLum = screenTexture.Sample(u_sampler, float2(0.5, 0.5)).r;
    float avgLum = exp(avgLogLum);
    float targetExposure = 0.18 / max(avgLum, 0.001);
    targetExposure = clamp(targetExposure * exp2(u_compensation), u_min_exposure, u_max_exposure);
    float prevExposure = prevAdaptedTex.Sample(u_sampler, float2(0.5, 0.5)).r;
    if (prevExposure <= 0.0) prevExposure = targetExposure;
    float speed = (targetExposure > prevExposure) ? u_speed_up : u_speed_down;
    float adapted = prevExposure + (targetExposure - prevExposure) * (1.0 - exp(-u_dt * speed));
    return float4(adapted, 0.0, 0.0, 1.0);
}
)";

// ============================================================
// Tonemapping (带可选 Auto Exposure)
// ============================================================

constexpr const char* kTonemappingPS = R"(
Texture2D screenTexture    : register(t0);
Texture2D autoExposureTex  : register(t1);
Texture3D lutTexture       : register(t4);
SamplerState u_sampler     : register(s0);
SamplerState u_lut_sampler : register(s1);

cbuffer TonemapParams : register(b0) {
    float u_manual_exposure;
    int u_auto_exposure_enabled;
    int u_lut_enabled;
    float u_lut_intensity;
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
    float3 hdrColor = screenTexture.Sample(u_sampler, input.uv).rgb;
    float finalExposure = u_manual_exposure;
    if (u_auto_exposure_enabled) {
        finalExposure = autoExposureTex.Sample(u_sampler, float2(0.5, 0.5)).r;
    }
    float3 result = AcesFilmic(hdrColor * finalExposure);
    result = pow(max(result, 0.0f), 1.0 / 2.2);
    if (u_lut_enabled) {
        float3 lutColor = lutTexture.Sample(u_lut_sampler, saturate(result)).rgb;
        result = lerp(result, lutColor, u_lut_intensity);
    }
    float ign = frac(52.9829189f * frac(0.06711056f * input.pos.x + 0.00583715f * input.pos.y));
    result += (ign - 0.5f) / 255.0f;
    return float4(result, 1.0);
}
)";

// ============================================================
// Bloom Composite + SSAO + Auto Exposure 像素着色器
// ============================================================

constexpr const char* kBloomCompositeSsaoAePS = R"(
Texture2D screenTexture    : register(t0);
Texture2D bloomBlur        : register(t1);
Texture2D ssaoTexture      : register(t2);
Texture2D autoExposureTex  : register(t3);
Texture3D lutTexture       : register(t4);
Texture2D contactShadowTex : register(t5);
SamplerState u_sampler     : register(s0);
SamplerState u_lut_sampler : register(s1);

cbuffer BloomCompositeAeParams : register(b0) {
    float exposure;
    float bloomIntensity;
    int bloomEnabled;
    int ssaoEnabled;
    int autoExposureEnabled;
    int lutEnabled;
    float lutIntensity;
    int csEnabled;
    float csStrength;
    int vignetteEnabled;
    float vignetteIntensity;
    float vignetteRadius;
    float vignetteSoftness;
    int filmGrainEnabled;
    float filmGrainIntensity;
    float filmGrainTime;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float3 AcesFilmic(float3 x) {
    float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float GrainNoise(float2 uv, float time_seed) {
    return frac(sin(dot(uv + float2(time_seed, time_seed * 0.37f), float2(12.9898f, 78.233f))) * 43758.5453f);
}

float4 PSMain(PSInput input) : SV_TARGET {
    float3 color = screenTexture.Sample(u_sampler, input.uv).rgb;
    if (ssaoEnabled) {
        float ao = ssaoTexture.Sample(u_sampler, input.uv).r;
        color *= ao;
    }
    if (bloomEnabled) {
        float3 bloom = bloomBlur.Sample(u_sampler, input.uv).rgb;
        color += bloom * bloomIntensity;
    }
    if (csEnabled) {
        float cs = contactShadowTex.Sample(u_sampler, input.uv).r;
        color *= (1.0 - (1.0 - cs) * csStrength);
    }
    float finalExposure = exposure;
    if (autoExposureEnabled) {
        finalExposure = autoExposureTex.Sample(u_sampler, float2(0.5, 0.5)).r;
    }
    color = AcesFilmic(color * finalExposure);
    color = pow(max(color, 0.0f), 1.0f / 2.2f);
    if (lutEnabled) {
        float3 lutColor = lutTexture.Sample(u_lut_sampler, saturate(color)).rgb;
        color = lerp(color, lutColor, lutIntensity);
    }
    if (vignetteEnabled) {
        float dist = length(input.uv - float2(0.5f, 0.5f));
        float radius = clamp(vignetteRadius, 0.001f, 1.5f);
        float softness = max(vignetteSoftness, 0.0001f);
        float vignette = 1.0f - smoothstep(radius, radius + softness, dist);
        color *= lerp(1.0f, vignette, clamp(vignetteIntensity, 0.0f, 1.0f));
    }
    if (filmGrainEnabled) {
        float grain = GrainNoise(input.uv * float2(1280.0f, 720.0f), filmGrainTime) - 0.5f;
        color = saturate(color + grain * filmGrainIntensity);
    }
    float ign = frac(52.9829189f * frac(0.06711056f * input.pos.x + 0.00583715f * input.pos.y));
    color += (ign - 0.5f) / 255.0f;
    return float4(color, 1.0f);
}
)";

// ============================================================
// Color Grading (LUT only, no tonemapping)
// ============================================================

constexpr const char* kColorGradingPS = R"(
Texture2D screenTexture    : register(t0);
Texture3D lutTexture       : register(t4);
SamplerState u_sampler     : register(s0);
SamplerState u_lut_sampler : register(s1);

cbuffer ColorGradingParams : register(b0) {
    float u_lut_intensity;
    float3 _pad;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    float3 color = screenTexture.Sample(u_sampler, input.uv).rgb;
    float3 lutColor = lutTexture.Sample(u_lut_sampler, saturate(color)).rgb;
    color = lerp(color, lutColor, u_lut_intensity);
    float ign = frac(52.9829189f * frac(0.06711056f * input.pos.x + 0.00583715f * input.pos.y));
    color += (ign - 0.5f) / 255.0f;
    return float4(color, 1.0f);
}
)";

// ============================================================
// TAA Resolve 像素着色器 — Variance Clipping
// ============================================================

constexpr const char* kTaaResolvePS = R"(
Texture2D screenTexture  : register(t0);
Texture2D u_history      : register(t1);
Texture2D u_motion_vector: register(t2);
SamplerState u_sampler   : register(s0);

cbuffer TaaParams : register(b0) {
    float u_blend_factor;
    float u_jitter_x;
    float u_jitter_y;
    int   u_frame_index;
    float u_screen_w;
    float u_screen_h;
    float _pad0;
    float _pad1;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    float3 current = screenTexture.Sample(u_sampler, input.uv).rgb;

    float2 mv = u_motion_vector.Sample(u_sampler, input.uv).rg;
    float2 history_uv = input.uv - mv - float2(u_jitter_x, u_jitter_y);
    history_uv = clamp(history_uv, 0.0f, 1.0f);

    float2 texel = float2(1.0f / u_screen_w, 1.0f / u_screen_h);
    float3 m1 = 0.0f, m2 = 0.0f;
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            float3 s = screenTexture.Sample(u_sampler, input.uv + float2(dx, dy) * texel).rgb;
            m1 += s;
            m2 += s * s;
        }
    }
    m1 /= 9.0f;
    float3 sigma = sqrt(max(m2 / 9.0f - m1 * m1, 0.0f));

    float3 history = u_history.Sample(u_sampler, history_uv).rgb;
    history = clamp(history, m1 - 1.25f * sigma, m1 + 1.25f * sigma);

    float velocity_len = length(mv * float2(u_screen_w, u_screen_h));
    float vel_weight = clamp(velocity_len * 0.5f, 0.0f, 0.5f);
    float alpha = (u_frame_index < 2) ? 1.0f : clamp(u_blend_factor + vel_weight, u_blend_factor, 1.0f);
    return float4(lerp(history, current, alpha), 1.0f);
}
)";

// ============================================================
// Motion Vector Pixel Shader
// ============================================================
constexpr const char* kMotionVectorPS = R"(
Texture2D screenTexture : register(t0);
SamplerState u_sampler  : register(s0);

cbuffer MvParams : register(b0) {
    float screen_w;
    float screen_h;
    float _pad0;
    float _pad1;
    float4x4 reproj;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    float depth = screenTexture.Sample(u_sampler, input.uv).r;
    float2 ndc = input.uv * 2.0f - 1.0f;
    float z_ndc = depth * 2.0f - 1.0f;
    float4 clip_pos = float4(ndc, z_ndc, 1.0f);
    float4 prev_clip = mul(clip_pos, reproj);
    prev_clip.xy /= prev_clip.w;
    float2 prev_uv = prev_clip.xy * 0.5f + 0.5f;
    float2 velocity = input.uv - prev_uv;
    return float4(velocity, 0.0f, 1.0f);
}
)";

// ============================================================
// DOF (Depth of Field) Pixel Shader
// ============================================================
constexpr const char* kDofPS = R"(
Texture2D screenTexture : register(t0);
Texture2D colorTexture  : register(t1);
SamplerState u_sampler  : register(s0);

cbuffer DofParams : register(b0) {
    float focus_distance;
    float focus_range;
    float bokeh_radius;
    float near_plane;
    float far_plane;
    float screen_w;
    float screen_h;
    float _pad0;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float linearizeDepth(float d) {
    float z = d * 2.0f - 1.0f;
    return (2.0f * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
}

float4 PSMain(PSInput input) : SV_TARGET {
    float depth = screenTexture.Sample(u_sampler, input.uv).r;
    float lin_depth = linearizeDepth(depth);
    float coc = saturate(abs(lin_depth - focus_distance) / focus_range);
    float2 texel = float2(1.0f / screen_w, 1.0f / screen_h);
    float radius = coc * bokeh_radius;
    float3 color = float3(0.0f, 0.0f, 0.0f);
    float total_weight = 0.0f;
    static const int SAMPLES = 16;
    static const float GOLDEN_ANGLE = 2.39996323f;
    for (int i = 0; i < SAMPLES; ++i) {
        float r = sqrt(float(i) / float(SAMPLES)) * radius;
        float theta = float(i) * GOLDEN_ANGLE;
        float2 offset = float2(cos(theta), sin(theta)) * r * texel;
        float sample_depth = linearizeDepth(screenTexture.Sample(u_sampler, input.uv + offset).r);
        float sample_coc = saturate(abs(sample_depth - focus_distance) / focus_range);
        float w = max(sample_coc, coc);
        color += colorTexture.Sample(u_sampler, input.uv + offset).rgb * w;
        total_weight += w;
    }
    if (total_weight > 0.0f) color /= total_weight;
    else color = colorTexture.Sample(u_sampler, input.uv).rgb;
    return float4(color, 1.0f);
}
)";

// ============================================================
// Motion Blur Pixel Shader
// ============================================================
constexpr const char* kMotionBlurPS = R"(
Texture2D screenTexture : register(t0);
Texture2D colorTexture  : register(t1);
SamplerState u_sampler  : register(s0);

cbuffer MotionBlurParams : register(b0) {
    float intensity;
    float num_samples;
    float screen_w;
    float screen_h;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    float2 velocity = screenTexture.Sample(u_sampler, input.uv).rg * intensity;
    int samples = max(int(num_samples), 1);
    float3 color = colorTexture.Sample(u_sampler, input.uv).rgb;
    float total = 1.0f;
    for (int i = 1; i < samples; ++i) {
        float t = float(i) / float(samples);
        float2 sample_uv = input.uv + velocity * t;
        if (sample_uv.x >= 0.0f && sample_uv.x <= 1.0f && sample_uv.y >= 0.0f && sample_uv.y <= 1.0f) {
            color += colorTexture.Sample(u_sampler, sample_uv).rgb;
            total += 1.0f;
        }
    }
    return float4(color / total, 1.0f);
}
)";

// ============================================================
// SSR (Screen Space Reflections) Pixel Shader
// ============================================================
constexpr const char* kSsrPS = R"(
Texture2D screenTexture : register(t0);
Texture2D colorTexture  : register(t1);
SamplerState u_sampler  : register(s0);

cbuffer SsrParams : register(b0) {
    float max_distance;
    float thickness;
    float step_size;
    int max_steps;
    float near_plane;
    float far_plane;
    float screen_w;
    float screen_h;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float linearizeDepth(float d) {
    float z = d * 2.0f - 1.0f;
    return (2.0f * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
}

float3 reconstructNormal(float2 uv) {
    float2 texel = float2(1.0f / screen_w, 1.0f / screen_h);
    float dc = linearizeDepth(screenTexture.Sample(u_sampler, uv).r);
    float dl = linearizeDepth(screenTexture.Sample(u_sampler, uv - float2(texel.x, 0.0f)).r);
    float dr = linearizeDepth(screenTexture.Sample(u_sampler, uv + float2(texel.x, 0.0f)).r);
    float db = linearizeDepth(screenTexture.Sample(u_sampler, uv - float2(0.0f, texel.y)).r);
    float dt = linearizeDepth(screenTexture.Sample(u_sampler, uv + float2(0.0f, texel.y)).r);
    return normalize(float3(dl - dr, db - dt, 2.0f * texel.x * dc));
}

float4 PSMain(PSInput input) : SV_TARGET {
    float depth = screenTexture.Sample(u_sampler, input.uv).r;
    if (depth >= 1.0f) return float4(0.0f, 0.0f, 0.0f, 0.0f);
    float lin_depth = linearizeDepth(depth);
    float3 normal = reconstructNormal(input.uv);
    float3 view_dir = normalize(float3(input.uv * 2.0f - 1.0f, 1.0f));
    float3 reflect_dir = reflect(view_dir, normal);
    float2 texel = float2(1.0f / screen_w, 1.0f / screen_h);
    float2 ray_uv = input.uv;
    float ray_depth = lin_depth;
    for (int i = 0; i < max_steps; ++i) {
        ray_uv += reflect_dir.xy * texel * step_size;
        if (ray_uv.x < 0.0f || ray_uv.x > 1.0f || ray_uv.y < 0.0f || ray_uv.y > 1.0f) break;
        float sample_depth = linearizeDepth(screenTexture.Sample(u_sampler, ray_uv).r);
        ray_depth += reflect_dir.z * step_size;
        float depth_diff = ray_depth - sample_depth;
        if (depth_diff > 0.0f && depth_diff < thickness) {
            float fade = 1.0f - float(i) / float(max_steps);
            float3 hit_color = colorTexture.Sample(u_sampler, ray_uv).rgb;
            return float4(hit_color * fade, fade);
        }
    }
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}
)";

// ============================================================
// GBuffer 几何通道 Pixel Shader（MRT 输出 albedo/normal/position）
// ============================================================

constexpr const char* kGBufferPS = R"(
Texture2D u_texture : register(t0);
SamplerState u_sampler : register(s0);

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

struct GBufferOutput {
    float4 albedo   : SV_TARGET0;
    float4 normal   : SV_TARGET1;
    float4 position : SV_TARGET2;
};

GBufferOutput PSMain(PSInput input) {
    GBufferOutput output;
    float4 texColor = u_texture.Sample(u_sampler, input.uv);
    float4 albedo = texColor * input.color;
    clip(albedo.a - 0.01);
    output.albedo   = albedo;
    output.normal   = float4(normalize(input.normal) * 0.5 + 0.5, 1.0);
    output.position = float4(input.fragPos, 1.0);
    return output;
}
)";

// ============================================================
// Deferred Lighting Pixel Shader（全屏后处理光照合成）
// ============================================================

constexpr const char* kDeferredLightingPS = R"(
Texture2D screenTexture  : register(t0);
Texture2D u_gbuf_normal  : register(t1);
Texture2D u_gbuf_position: register(t2);
SamplerState u_sampler   : register(s0);

cbuffer DeferredLightParams : register(b0) {
    float3 u_light_dir;
    float  u_light_intensity;
    float3 u_light_color;
    float  u_ambient;
};

struct PSInput {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 PSMain(PSInput input) : SV_TARGET {
    float3 albedo = screenTexture.Sample(u_sampler, input.uv).rgb;
    float3 normal = u_gbuf_normal.Sample(u_sampler, input.uv).rgb * 2.0 - 1.0;
    float3 position = u_gbuf_position.Sample(u_sampler, input.uv).rgb;
    if (length(normal) < 0.01) return float4(0.0, 0.0, 0.0, 1.0);
    normal = normalize(normal);
    float NdotL = max(dot(normal, -normalize(u_light_dir)), 0.0);
    float3 diffuse = albedo * u_light_color * u_light_intensity * NdotL;
    float3 ambient = albedo * u_ambient;
    return float4(diffuse + ambient, 1.0);
}
)";

} // namespace dx11_shaders
} // namespace render
} // namespace dse

#endif // DSE_RENDER_DX11_SHADER_SOURCES_H
