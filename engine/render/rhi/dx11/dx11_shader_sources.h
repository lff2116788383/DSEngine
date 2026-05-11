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

} // namespace dx11_shaders
} // namespace render
} // namespace dse

#endif // DSE_RENDER_DX11_SHADER_SOURCES_H
