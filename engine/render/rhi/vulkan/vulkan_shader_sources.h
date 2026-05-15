/**
 * @file vulkan_shader_sources.h
 * @brief Vulkan 专用 GLSL 450 着色器源码
 *
 * 所有着色器遵循 Vulkan SPIR-V 约定：
 * - #version 450 + GL_EXT_scalar_block_layout / GL_ARB_separate_shader_objects
 * - UBO 使用 layout(std140, set=N, binding=N) 显式绑定
 * - 采样器使用 layout(binding=N) 显式绑定
 * - VS 输出 / FS 输入通过 layout(location=N) 精确匹配
 * - push_constant 用于逐对象数据（model matrix 等）
 *
 * Descriptor Set 布局约定（与 SPIR-V 反射 fallback 一致）：
 *   Set 0, Binding 0: PerFrame UBO    (vp, view, camera_pos)
 *   Set 1, Binding 0: PerScene UBO    (方向光/阴影参数)
 *   Set 1, Binding 1: PointLights UBO (点光源数组 208B std140)
 *   Set 1, Binding 2: SpotLights UBO  (聚光灯数组 272B std140)
 *   Set 2, Binding 0: PerMaterial UBO (材质参数)
 *   Set 2, Binding 1-7: 采样器        (albedo/normal/mr/emissive/occlusion/shadow/spot_shadow)
 *   Set 3, Binding 0: 点光源立方体阴影 samplerCube[4]
 *   Push Constant: 逐对象 model matrix + 骨骼/变形标记
 */

#ifndef DSE_RENDER_VULKAN_SHADER_SOURCES_H
#define DSE_RENDER_VULKAN_SHADER_SOURCES_H

namespace dse {
namespace render {
namespace vulkan_shaders {

// ============================================================
// PBR 着色器
// ============================================================

/// PBR 顶点着色器（Vulkan GLSL 450）
constexpr const char* kPbrVertex = R"(#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aNormal;
layout(location = 4) in vec3 aTangent;
layout(location = 5) in vec4 aBoneWeights;
layout(location = 6) in vec4 aBoneIndices;
layout(location = 7) in vec4 aInstModelCol0;
layout(location = 8) in vec4 aInstModelCol1;
layout(location = 9) in vec4 aInstModelCol2;
layout(location = 10) in vec4 aInstModelCol3;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vTexCoord;
layout(location = 2) out vec3 vFragPos;
layout(location = 3) out vec3 vNormal;
layout(location = 4) out mat3 vTBN;
layout(location = 7) out vec3 vFragPosViewSpace;

// Set 0: PerFrame
layout(std140, set = 0, binding = 0) uniform PerFrame {
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
};

// Push Constant: 逐对象数据
layout(push_constant) uniform PushConstants {
    mat4 model;
    int skinned;
    int morph_enabled;
    int use_instancing;
} pc;

const int MAX_BONES = 100;
layout(std140, set = 2, binding = 8) uniform BoneMatrices {
    mat4 bone_matrices[MAX_BONES];
};

const int MAX_MORPH_TARGETS = 4;
layout(std140, set = 2, binding = 9) uniform MorphWeights {
    float morph_weights[MAX_MORPH_TARGETS];
};

void main() {
    mat4 modelMatrix;
    if (pc.use_instancing != 0) {
        modelMatrix = mat4(aInstModelCol0, aInstModelCol1, aInstModelCol2, aInstModelCol3);
    } else {
        modelMatrix = pc.model;
    }
    mat4 boneTransform = mat4(1.0);
    if (pc.skinned != 0) {
        boneTransform = bone_matrices[int(aBoneIndices[0])] * aBoneWeights[0] +
                        bone_matrices[int(aBoneIndices[1])] * aBoneWeights[1] +
                        bone_matrices[int(aBoneIndices[2])] * aBoneWeights[2] +
                        bone_matrices[int(aBoneIndices[3])] * aBoneWeights[3];
    }

    vec3 morphedPos = aPos;
    vec3 morphedNormal = aNormal;
    if (pc.morph_enabled != 0) {
        morphedPos += vec3(0.01) * morph_weights[0];
    }

    vec4 localPos = boneTransform * vec4(morphedPos, 1.0);
    vec4 worldPos = modelMatrix * localPos;
    gl_Position = vp * worldPos;

    vFragPos = worldPos.xyz;
    vFragPosViewSpace = (view * worldPos).xyz;
    vColor = aColor;
    vTexCoord = aTexCoord;

    mat3 normalMatrix = transpose(inverse(mat3(modelMatrix * boneTransform)));
    vec3 T = normalize(normalMatrix * aTangent);
    vec3 N = normalize(normalMatrix * morphedNormal);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    vTBN = mat3(T, B, N);
    vNormal = N;
}
)";

/// PBR 片段着色器（Vulkan GLSL 450）
constexpr const char* kPbrFragment = R"(#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vTexCoord;
layout(location = 2) in vec3 vFragPos;
layout(location = 3) in vec3 vNormal;
layout(location = 4) in mat3 vTBN;
layout(location = 7) in vec3 vFragPosViewSpace;

layout(location = 0) out vec4 FragColor;

// Set 0: PerFrame
layout(std140, set = 0, binding = 0) uniform PerFrame {
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
};

// Set 1: PerScene
layout(std140, set = 1, binding = 0) uniform PerScene {
    vec4 light_dir_and_enabled;
    vec4 light_color_and_ambient;
    vec4 light_params;
    vec4 cascade_splits;
    mat4 light_space_matrices[3];
};

// Set 2: PerMaterial
layout(std140, set = 2, binding = 0) uniform PerMaterial {
    vec4 albedo;
    vec4 roughness_ao;
    vec4 emissive;
    vec4 flags;
    vec4 extra_params;
    vec4 extra_params2;
    vec4 toon_shadow_color;
    vec4 toon_params;
};

// 采样器 (Set 2)
layout(set = 2, binding = 1) uniform sampler2D u_texture;
layout(set = 2, binding = 2) uniform sampler2D u_normal_map;
layout(set = 2, binding = 3) uniform sampler2D u_metallic_roughness_map;
layout(set = 2, binding = 4) uniform sampler2D u_emissive_map;
layout(set = 2, binding = 5) uniform sampler2D u_occlusion_map;

#define CSM_CASCADES 3
layout(set = 2, binding = 6) uniform sampler2DShadow u_shadow_maps[CSM_CASCADES];
layout(set = 2, binding = 7) uniform sampler2D u_spot_shadow_maps[4];

layout(std140, set = 2, binding = 10) uniform SpotLightData {
    mat4 u_spot_light_space_matrices[4];
};

layout(set = 3, binding = 0) uniform samplerCube u_point_shadow_maps[4];

struct PointLight {
    vec3 color;
    float intensity;
    vec3 position;
    float radius;
    int cast_shadow;
    int shadow_index;
    vec2 _pad;
};
#define MAX_POINT_LIGHTS 64
layout(std140, set = 1, binding = 1) uniform PointLights {
    int u_point_light_count;
    int _pl_pad0;
    int _pl_pad1;
    int _pl_pad2;
    PointLight u_point_lights[MAX_POINT_LIGHTS];
};

struct SpotLight {
    vec3 color;
    float intensity;
    vec3 position;
    float radius;
    vec3 direction;
    float inner_cone;
    float outer_cone;
    int cast_shadow;
    int shadow_index;
    float _pad;  // NOTE: must be float (not vec2) to keep stride=64B matching C++ VulkanSpotLightsUBO::Entry
};
#define MAX_SPOT_LIGHTS 64
layout(std140, set = 1, binding = 2) uniform SpotLights {
    int u_spot_light_count;
    int _sl_pad0;
    int _sl_pad1;
    int _sl_pad2;
    SpotLight u_spot_lights[MAX_SPOT_LIGHTS];
};

const float PI = 3.14159265359;

// UBO 字段便捷访问别名
#define u_lighting_enabled    (light_dir_and_enabled.w != 0.0)
#define u_light_direction     light_dir_and_enabled.xyz
#define u_light_color         light_color_and_ambient.xyz
#define u_light_intensity     light_params.x
#define u_ambient_intensity   light_color_and_ambient.w
#define u_shadow_strength     light_params.y
#define u_receive_shadow      (light_params.z != 0.0)
#define u_cascade_splits      cascade_splits.xyz

#define u_material_albedo           albedo.xyz
#define u_material_metallic         albedo.w
#define u_material_roughness        roughness_ao.x
#define u_material_ao               roughness_ao.y
#define u_material_normal_strength  roughness_ao.z
#define u_material_alpha_cutoff     roughness_ao.w
#define u_material_emissive         emissive.xyz
#define u_material_alpha_test       (emissive.w != 0.0)
#define u_has_normal_map            (flags.x != 0.0)
#define u_has_metallic_roughness_map (flags.y != 0.0)
#define u_has_emissive_map          (flags.z != 0.0)
#define u_has_occlusion_map         (flags.w != 0.0)
#define u_camera_pos                camera_pos.xyz
#define u_sss_strength              extra_params.x
#define u_clear_coat                extra_params.y
#define u_clear_coat_roughness      extra_params.z
#define u_anisotropy                extra_params.w
#define u_pom_height_scale          extra_params2.x
#define u_sss_tint                  extra_params2.yzw
#define u_toon_shadow_color         toon_shadow_color.xyz
#define u_toon_shadow_threshold     toon_shadow_color.w
#define u_toon_shadow_softness      toon_params.x
#define u_toon_specular_size        toon_params.y
#define u_toon_specular_strength    toon_params.z
#define u_toon_rim_strength         toon_params.w

#define POM_NUM_LAYERS 16
vec2 ParallaxOcclusionMapping(vec2 uv, vec3 viewDirTS, float height_scale) {
    const float layerDepth = 1.0 / float(POM_NUM_LAYERS);
    float currentLayerDepth = 0.0;
    vec2 P = viewDirTS.xy / max(viewDirTS.z, 0.001) * height_scale;
    vec2 deltaUV = P / float(POM_NUM_LAYERS);
    vec2 curUV = uv;
    float curDepth = 1.0 - texture(u_normal_map, curUV).a;
    for (int i = 0; i < POM_NUM_LAYERS; ++i) {
        if (currentLayerDepth >= curDepth) break;
        curUV -= deltaUV;
        curDepth = 1.0 - texture(u_normal_map, curUV).a;
        currentLayerDepth += layerDepth;
    }
    vec2 prevUV = curUV + deltaUV;
    float afterDepth = curDepth - currentLayerDepth;
    float beforeDepth = (1.0 - texture(u_normal_map, prevUV).a) - currentLayerDepth + layerDepth;
    float w = afterDepth / (afterDepth - beforeDepth + 0.0001);
    return mix(curUV, prevUV, w);
}

float DistributionGGXAniso(vec3 N, vec3 H, vec3 T, vec3 B, float roughness, float aniso) {
    float at = max(roughness * (1.0 + aniso), 0.001);
    float ab = max(roughness * (1.0 - aniso), 0.001);
    float TdotH = dot(T, H);
    float BdotH = dot(B, H);
    float NdotH = dot(N, H);
    float d = TdotH*TdotH/(at*at) + BdotH*BdotH/(ab*ab) + NdotH*NdotH;
    return 1.0 / (PI * at * ab * d * d + 0.0001);
}

vec3 SubsurfaceScattering(vec3 N, vec3 L, vec3 alb, float sss, vec3 lc, float li, vec3 tint) {
    float wrap = 0.5 * sss;
    float NdotL_wrap = max(0.0, (dot(N, L) + wrap) / (1.0 + wrap));
    float NdotL_std  = max(dot(N, L), 0.0);
    float diff = NdotL_wrap - NdotL_std;
    vec3 sss_tint = (dot(tint, tint) > 0.0) ? tint : vec3(1.0, 0.35, 0.2);
    return alb * sss_tint * diff * lc * li;
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return nom / max(denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float SampleShadowPCF(sampler2DShadow shadowMap, vec3 proj_coords, float bias) {
    float shadow = 0.0;
    vec2 texel_size = 1.0 / vec2(textureSize(shadowMap, 0));
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            shadow += texture(shadowMap, vec3(proj_coords.xy
                      + vec2(x, y) * texel_size, proj_coords.z - bias));
    return shadow / 9.0;
}

float ShadowCalculation(vec3 fragPosWorldSpace, vec3 fragPosViewSpace, vec3 normal, vec3 lightDir) {
    if (!u_receive_shadow) return 0.0;
    int cascadeIndex = CSM_CASCADES - 1;
    for (int i = 0; i < CSM_CASCADES - 1; ++i) {
        if (abs(fragPosViewSpace.z) < u_cascade_splits[i]) {
            cascadeIndex = i;
            break;
        }
    }
    vec4 fragPosLightSpace = light_space_matrices[cascadeIndex] * vec4(fragPosWorldSpace, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if(projCoords.z > 1.0) return 0.0;
    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) return 0.0;
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
    float lit = SampleShadowPCF(u_shadow_maps[cascadeIndex], projCoords, bias);
    return clamp((1.0 - lit) * u_shadow_strength, 0.0, 1.0);
}

float SpotShadowCalculation(int shadowIndex, vec3 fragPosWorldSpace, vec3 normal, vec3 lightDir) {
    if (shadowIndex < 0 || shadowIndex >= 4) return 0.0;
    vec4 fragPosLightSpace = u_spot_light_space_matrices[shadowIndex] * vec4(fragPosWorldSpace, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / max(fragPosLightSpace.w, 0.0001);
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;
    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) return 0.0;
    float currentDepth = projCoords.z;
    float bias = max(0.003 * (1.0 - dot(normal, lightDir)), 0.0005);
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(u_spot_shadow_maps[shadowIndex], 0));
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(u_spot_shadow_maps[shadowIndex], projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;
    return clamp(shadow * u_shadow_strength, 0.0, 1.0);
}

float PointShadowCalculation(int shadowIndex, vec3 fragPosWorldSpace, vec3 lightPos, float lightRadius) {
    if (shadowIndex < 0 || shadowIndex >= 4) return 0.0;
    vec3 fragToLight = fragPosWorldSpace - lightPos;
    float currentDepth = length(fragToLight);
    if (currentDepth >= lightRadius) return 0.0;
    float closestDepth = texture(u_point_shadow_maps[shadowIndex], fragToLight).r * lightRadius;
    float bias = 0.05;
    return (currentDepth - bias) > closestDepth ? u_shadow_strength : 0.0;
}
)" R"(
void main() {
    vec2 finalUV = vTexCoord;
    if (u_pom_height_scale > 0.0 && u_has_normal_map) {
        vec3 viewDirTS = transpose(vTBN) * normalize(u_camera_pos - vFragPos);
        finalUV = ParallaxOcclusionMapping(vTexCoord, viewDirTS, u_pom_height_scale);
    }
    vec4 texColor = texture(u_texture, finalUV) * vColor;
    if (u_material_alpha_test && texColor.a < clamp(u_material_alpha_cutoff, 0.0, 1.0)) discard;

    vec3 N = vNormal;
    if (u_has_normal_map) {
        vec3 normalMap = texture(u_normal_map, finalUV).rgb * 2.0 - 1.0;
        normalMap.xy *= u_material_normal_strength;
        N = normalize(vTBN * normalMap);
    }

    if (!u_lighting_enabled) {
        vec3 result = texColor.rgb * vColor.rgb * u_material_albedo;
        if (u_has_emissive_map) {
            result += texture(u_emissive_map, finalUV).rgb * u_material_emissive;
        }
        if (light_params.w == 0.0) {
            result = result / (result + vec3(1.0));
            result = pow(result, vec3(1.0/2.2));
        }
        FragColor = vec4(result, texColor.a * vColor.a);
        return;
    }

    // Half-Lambert shading mode (KF-style, light_params.w == 2.0)
    if (light_params.w == 2.0) {
        vec3 L = normalize(-u_light_direction);
        vec3 V_hl = normalize(u_camera_pos - vFragPos);
        vec3 R = reflect(u_light_direction, N);
        float half_lambert = dot(N, L) * 0.5 + 0.5;
        vec3 diffuse_color = texColor.rgb * vColor.rgb * u_material_albedo * half_lambert;
        float spec_brightness = pow(max(dot(R, V_hl), 0.0), 100.0);
        vec3 spec_tex = u_has_metallic_roughness_map
            ? texture(u_metallic_roughness_map, finalUV).rgb
            : vec3(0.0);
        vec3 specular_color = spec_tex * spec_brightness;
        float shadow = ShadowCalculation(vFragPos, vFragPosViewSpace, N, L);
        float shadow_multiplier = 1.0 - shadow * 0.5;
        vec3 color = (diffuse_color + specular_color) * shadow_multiplier;
        FragColor = vec4(color, 1.0);
        return;
    }

    // Toon / Cel shading mode (light_params.w == 4.0)
    if (light_params.w == 4.0) {
        vec3 L = normalize(-u_light_direction);
        vec3 V_tn = normalize(u_camera_pos - vFragPos);
        vec3 H = normalize(L + V_tn);
        float NdotL = dot(N, L) * 0.5 + 0.5;
        float band1 = smoothstep(u_toon_shadow_threshold - u_toon_shadow_softness,
                                 u_toon_shadow_threshold + u_toon_shadow_softness, NdotL);
        float band2 = smoothstep(0.7 - u_toon_shadow_softness, 0.7 + u_toon_shadow_softness, NdotL);
        float cel = band1 * 0.7 + band2 * 0.3;
        vec3 baseColor = texColor.rgb * vColor.rgb * u_material_albedo;
        vec3 shadowColor = baseColor * u_toon_shadow_color;
        float shadow = ShadowCalculation(vFragPos, vFragPosViewSpace, N, L);
        vec3 diffuse = mix(shadowColor, baseColor * u_light_color, cel) * (1.0 - shadow);
        float NdotH = max(dot(N, H), 0.0);
        float spec = step(u_toon_specular_size, NdotH) * u_toon_specular_strength;
        vec3 specular = u_light_color * spec * (1.0 - shadow);
        float rim = pow(1.0 - max(dot(N, V_tn), 0.0), 4.0) * u_toon_rim_strength;
        vec3 color = diffuse + specular + vec3(rim);
        color = color / (color + vec3(1.0));
        color = pow(color, vec3(1.0 / 2.2));
        FragColor = vec4(color, texColor.a * vColor.a);
        return;
    }

    // Watercolor shading mode (light_params.w == 5.0)
    if (light_params.w == 5.0) {
        float wc_paper    = toon_shadow_color.x;
        float wc_edge     = toon_shadow_color.y;
        float wc_bleed    = toon_shadow_color.z;
        float wc_pigment  = max(toon_shadow_color.w, 0.1);
        vec3 L = normalize(-u_light_direction);
        vec3 V_wc = normalize(u_camera_pos - vFragPos);
        float NdotL = dot(N, L) * 0.5 + 0.5;
        vec3 baseColor = texColor.rgb * vColor.rgb * u_material_albedo;
        float soft_band = smoothstep(0.25, 0.55, NdotL);
        float shadow = ShadowCalculation(vFragPos, vFragPosViewSpace, N, L);
        vec3 lit = baseColor * u_light_color * u_light_intensity;
        vec3 shade = baseColor * vec3(0.45, 0.4, 0.5) * u_ambient_intensity;
        vec3 diffuse = mix(shade, lit, soft_band) * (1.0 - shadow * 0.6);
        float fresnel = 1.0 - max(dot(N, V_wc), 0.0);
        float edge_factor = pow(fresnel, 3.0) * wc_edge;
        diffuse *= (1.0 - edge_factor * 0.5);
        float paper_noise = fract(sin(dot(gl_FragCoord.xy * 0.01, vec2(12.9898, 78.233))) * 43758.5453);
        paper_noise = paper_noise * 0.5 + 0.5;
        diffuse = mix(diffuse, diffuse * paper_noise, wc_paper * 0.3);
        vec3 warm_shift = vec3(0.03, -0.01, -0.03) * wc_bleed;
        diffuse += warm_shift * (1.0 - soft_band);
        diffuse = pow(diffuse, vec3(1.0 / wc_pigment));
        diffuse = diffuse / (diffuse + vec3(1.0));
        diffuse = pow(diffuse, vec3(1.0 / 2.2));
        FragColor = vec4(diffuse, texColor.a * vColor.a);
        return;
    }

    // Half-Lambert STATIC shading mode (KF default_pixel_shader, light_params.w == 3.0)
    if (light_params.w == 3.0) {
        vec3 L = normalize(-u_light_direction);
        vec3 V_st = normalize(u_camera_pos - vFragPos);
        vec3 R = reflect(u_light_direction, N);
        float half_lambert = dot(N, L) * 0.5 + 0.5;
        vec3 diffuse = u_material_albedo * half_lambert * u_light_color * u_light_intensity;
        float spec_power = max(u_material_roughness, 1.0);
        vec3 spec_color = vec3(u_material_metallic);
        vec3 specular = spec_color * pow(max(dot(R, V_st), 0.0), spec_power);
        vec3 emissive_val = u_material_emissive;
        vec3 material_color = diffuse + specular + emissive_val;
        vec3 color_st = material_color * texColor.rgb * vColor.rgb;
        float shadow = ShadowCalculation(vFragPos, vFragPosViewSpace, N, L);
        float shadow_multiplier = 1.0 - shadow * 0.5;
        FragColor = vec4(color_st * shadow_multiplier, texColor.a * vColor.a);
        return;
    }

    vec3 surface_albedo = pow(texColor.rgb * vColor.rgb * u_material_albedo, vec3(2.2));
    float metallic = clamp(u_material_metallic, 0.0, 1.0);
    float roughness = clamp(u_material_roughness, 0.04, 1.0);
    float ao = max(u_material_ao, 0.0);
    vec3 surface_emissive = u_material_emissive;
    if (u_has_metallic_roughness_map) {
        vec4 mrSample = texture(u_metallic_roughness_map, finalUV);
        roughness = clamp(mrSample.g * u_material_roughness, 0.04, 1.0);
        metallic = clamp(mrSample.b * u_material_metallic, 0.0, 1.0);
    }
    if (u_has_occlusion_map) {
        ao *= texture(u_occlusion_map, finalUV).r;
    }
    if (u_has_emissive_map) {
        surface_emissive *= texture(u_emissive_map, finalUV).rgb;
    }
    vec3 V = normalize(u_camera_pos - vFragPos);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, surface_albedo, metallic);
    vec3 T = normalize(vTBN[0]);
    vec3 B = normalize(vTBN[1]);

    vec3 Lo = vec3(0.0);

    // 方向光
    {
        vec3 L = normalize(-u_light_direction);
        vec3 H = normalize(V + L);
        float NDF = (u_anisotropy != 0.0) ? DistributionGGXAniso(N, H, T, B, roughness, u_anisotropy) : DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular     = numerator / denominator;
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        float NdotL = max(dot(N, L), 0.0);
        float shadow = ShadowCalculation(vFragPos, vFragPosViewSpace, N, L);
        Lo += (kD * surface_albedo / PI + specular) * u_light_color * u_light_intensity * NdotL * (1.0 - shadow);
        if (u_sss_strength > 0.0)
            Lo += SubsurfaceScattering(N, L, surface_albedo, u_sss_strength, u_light_color, u_light_intensity, u_sss_tint) * (1.0 - shadow);
        if (u_clear_coat > 0.0) {
            float cc_r = max(u_clear_coat_roughness, 0.04);
            float NDF_cc = DistributionGGX(N, H, cc_r);
            float G_cc = GeometrySmith(N, V, L, cc_r);
            vec3 F_cc = fresnelSchlick(max(dot(H, V), 0.0), vec3(0.04));
            vec3 spec_cc = (NDF_cc * G_cc * F_cc) / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
            Lo += spec_cc * u_clear_coat * NdotL * u_light_color * u_light_intensity * (1.0 - shadow);
        }
    }

    // 点光源
    for(int i = 0; i < u_point_light_count; ++i) {
        vec3 L = normalize(u_point_lights[i].position - vFragPos);
        vec3 H = normalize(V + L);
        float distance = length(u_point_lights[i].position - vFragPos);
        float attenuation = clamp(1.0 - (distance*distance)/(u_point_lights[i].radius*u_point_lights[i].radius), 0.0, 1.0);
        attenuation *= attenuation;
        vec3 radiance = u_point_lights[i].color * u_point_lights[i].intensity * attenuation;
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular     = numerator / denominator;
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        float NdotL = max(dot(N, L), 0.0);
        float point_shadow = 0.0;
        if (u_point_lights[i].cast_shadow != 0) {
            point_shadow = PointShadowCalculation(u_point_lights[i].shadow_index, vFragPos, u_point_lights[i].position, u_point_lights[i].radius);
        }
        Lo += (kD * surface_albedo / PI + specular) * radiance * NdotL * (1.0 - point_shadow);
    }

    // 聚光灯
    for(int i = 0; i < u_spot_light_count; ++i) {
        vec3 L = normalize(u_spot_lights[i].position - vFragPos);
        vec3 H = normalize(V + L);
        float distance = length(u_spot_lights[i].position - vFragPos);
        float attenuation = clamp(1.0 - (distance * distance) / (u_spot_lights[i].radius * u_spot_lights[i].radius), 0.0, 1.0);
        attenuation *= attenuation;
        vec3 spotDir = normalize(-u_spot_lights[i].direction);
        float theta = dot(L, spotDir);
        float outerCos = cos(radians(u_spot_lights[i].outer_cone));
        float innerCos = cos(radians(u_spot_lights[i].inner_cone));
        float epsilon = max(innerCos - outerCos, 0.0001);
        float cone = clamp((theta - outerCos) / epsilon, 0.0, 1.0);
        if (cone <= 0.0) continue;
        vec3 radiance = u_spot_lights[i].color * u_spot_lights[i].intensity * attenuation * cone;
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular     = numerator / denominator;
        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;
        float NdotL = max(dot(N, L), 0.0);
        float spot_shadow = 0.0;
        if (u_spot_lights[i].cast_shadow != 0) {
            spot_shadow = SpotShadowCalculation(u_spot_lights[i].shadow_index, vFragPos, N, L);
        }
        Lo += (kD * surface_albedo / PI + specular) * radiance * NdotL * (1.0 - spot_shadow);
    }

    // 环境光
    vec3 F = fresnelSchlick(max(dot(N, V), 0.0), F0);
    vec3 kS_ambient = F;
    vec3 kD_ambient = 1.0 - kS_ambient;
    kD_ambient *= 1.0 - metallic;
    vec3 irradiance = vec3(u_ambient_intensity);
    vec3 diffuse_ambient = irradiance * surface_albedo;
    vec3 specular_ambient = irradiance * F0 * (1.0 - roughness);
    vec3 ambient = (kD_ambient * diffuse_ambient + specular_ambient) * ao;
    if (u_clear_coat > 0.0) {
        vec3 F_cc_amb = fresnelSchlick(max(dot(N, V), 0.0), vec3(0.04));
        ambient += F_cc_amb * u_clear_coat * irradiance * (1.0 - u_clear_coat_roughness) * 0.25;
    }
    vec3 color = ambient + Lo + surface_emissive;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));
    FragColor = vec4(color, texColor.a * vColor.a);
}
)";

// ============================================================
// 天空盒着色器
// ============================================================

constexpr const char* kSkyboxVertex = R"(#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 aPos;
layout(location = 0) out vec3 vTexCoords;

layout(push_constant) uniform PushConstants {
    mat4 vp;
} pc;

void main() {
    vTexCoords = aPos;
    // 放大立方体使其大于近平面（near_clip=10 时顶点距离必须 > 10）
    vec4 pos = pc.vp * vec4(aPos * 10000.0, 1.0);
    // Vulkan NDC z∈[0,1]: z=w*0.999 保持在最远深度但不被裁剪
    gl_Position = vec4(pos.xy, pos.w * 0.999, pos.w);
}
)";

constexpr const char* kSkyboxFragment = R"(#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 vTexCoords;
layout(location = 0) out vec4 FragColor;

layout(set = 2, binding = 1) uniform samplerCube skybox;

void main() {
    FragColor = texture(skybox, vTexCoords);
}
)";

// ============================================================
// 粒子着色器
// ============================================================

constexpr const char* kParticleVertex = R"(#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec3 iPos;
layout(location = 3) in vec4 iColor;
layout(location = 4) in float iSize;

layout(location = 0) out vec4 vParticleColor;
layout(location = 1) out vec2 vTexCoord;

layout(std140, set = 0, binding = 0) uniform PerFrame {
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
};

void main() {
    vec3 camera_right = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 camera_up = vec3(view[0][1], view[1][1], view[2][1]);

    vec3 vertexPosition_worldspace = iPos
        + camera_right * aPos.x * iSize
        + camera_up * aPos.y * iSize;

    gl_Position = vp * vec4(vertexPosition_worldspace, 1.0);
    vParticleColor = iColor;
    vTexCoord = aTexCoord;
}
)";

constexpr const char* kParticleFragment = R"(#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 vParticleColor;
layout(location = 1) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 2, binding = 1) uniform sampler2D u_texture;

void main() {
    vec4 texColor = texture(u_texture, vTexCoord);
    FragColor = texColor * vParticleColor;
}
)";

// ============================================================
// 2D 精灵着色器
// ============================================================

/// 2D 精灵顶点着色器（Vulkan GLSL 450）
constexpr const char* kSpriteVertex = R"(#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vTexCoord;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 vp;
} pc;

void main() {
    gl_Position = pc.vp * pc.model * vec4(aPos, 0.0, 1.0);
    vColor = aColor;
    vTexCoord = aTexCoord;
}
)";

/// 2D 精灵片段着色器（Vulkan GLSL 450）
constexpr const char* kSpriteFragment = R"(#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;

layout(set = 2, binding = 1) uniform sampler2D u_texture;

void main() {
    vec4 texColor = texture(u_texture, vTexCoord);
    FragColor = texColor * vColor;
}
)";

// ============================================================
// 后处理着色器（屏幕四边形 VS + 各种 FS）
// ============================================================

constexpr const char* kPostProcessVertex = R"(#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoords;
layout(location = 0) out vec2 vTexCoords;

void main() {
    vTexCoords = aTexCoords;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

constexpr const char* kPostProcessHeader = R"(#version 450
#extension GL_ARB_separate_shader_objects : enable
layout(location = 0) in vec2 vTexCoords;
layout(location = 0) out vec4 FragColor;
layout(set = 2, binding = 1) uniform sampler2D screenTexture;
)";

/// Bloom 降采样
constexpr const char* kBloomDownsampleFS = R"(
uniform vec2 srcResolution;
void main() {
    vec2 srcTexelSize = 1.0 / srcResolution;
    float x = srcTexelSize.x;
    float y = srcTexelSize.y;
    vec3 a = texture(screenTexture, vec2(vTexCoords.x - 2*x, vTexCoords.y + 2*y)).rgb;
    vec3 b = texture(screenTexture, vec2(vTexCoords.x,       vTexCoords.y + 2*y)).rgb;
    vec3 c = texture(screenTexture, vec2(vTexCoords.x + 2*x, vTexCoords.y + 2*y)).rgb;
    vec3 d = texture(screenTexture, vec2(vTexCoords.x - 2*x, vTexCoords.y)).rgb;
    vec3 e = texture(screenTexture, vec2(vTexCoords.x,       vTexCoords.y)).rgb;
    vec3 f = texture(screenTexture, vec2(vTexCoords.x + 2*x, vTexCoords.y)).rgb;
    vec3 g = texture(screenTexture, vec2(vTexCoords.x - 2*x, vTexCoords.y - 2*y)).rgb;
    vec3 h = texture(screenTexture, vec2(vTexCoords.x,       vTexCoords.y - 2*y)).rgb;
    vec3 i = texture(screenTexture, vec2(vTexCoords.x + 2*x, vTexCoords.y - 2*y)).rgb;
    vec3 j = texture(screenTexture, vec2(vTexCoords.x - x, vTexCoords.y + y)).rgb;
    vec3 k = texture(screenTexture, vec2(vTexCoords.x + x, vTexCoords.y + y)).rgb;
    vec3 l = texture(screenTexture, vec2(vTexCoords.x - x, vTexCoords.y - y)).rgb;
    vec3 m = texture(screenTexture, vec2(vTexCoords.x + x, vTexCoords.y - y)).rgb;
    vec3 downsample = e*0.125;
    downsample += (a+c+g+i)*0.03125;
    downsample += (b+d+f+h)*0.0625;
    downsample += (j+k+l+m)*0.125;
    FragColor = vec4(downsample, 1.0);
}
)";

/// Bloom 升采样
constexpr const char* kBloomUpsampleFS = R"(
uniform float filterRadius;
void main() {
    float x = filterRadius;
    float y = filterRadius;
    vec3 a = texture(screenTexture, vec2(vTexCoords.x - x, vTexCoords.y + y)).rgb;
    vec3 b = texture(screenTexture, vec2(vTexCoords.x,     vTexCoords.y + y)).rgb;
    vec3 c = texture(screenTexture, vec2(vTexCoords.x + x, vTexCoords.y + y)).rgb;
    vec3 d = texture(screenTexture, vec2(vTexCoords.x - x, vTexCoords.y)).rgb;
    vec3 e = texture(screenTexture, vec2(vTexCoords.x,     vTexCoords.y)).rgb;
    vec3 f = texture(screenTexture, vec2(vTexCoords.x + x, vTexCoords.y)).rgb;
    vec3 g = texture(screenTexture, vec2(vTexCoords.x - x, vTexCoords.y - y)).rgb;
    vec3 h = texture(screenTexture, vec2(vTexCoords.x,     vTexCoords.y - y)).rgb;
    vec3 i = texture(screenTexture, vec2(vTexCoords.x + x, vTexCoords.y - y)).rgb;
    vec3 upsample = e*4.0;
    upsample += (b+d+f+h)*2.0;
    upsample += (a+c+g+i);
    upsample *= 1.0 / 16.0;
    FragColor = vec4(upsample, 1.0);
}
)";

/// Bloom 亮度提取
constexpr const char* kBloomExtractFS = R"(
uniform float threshold;
void main() {
    vec3 color = texture(screenTexture, vTexCoords).rgb;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    if(brightness > threshold)
        FragColor = vec4(color, 1.0);
    else
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
)";

/// Bloom 水平模糊
constexpr const char* kBloomBlurHFS = R"(
uniform float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
void main() {
    vec2 tex_offset = 1.0 / textureSize(screenTexture, 0);
    vec3 result = texture(screenTexture, vTexCoords).rgb * weight[0];
    for(int i = 1; i < 5; ++i) {
        result += texture(screenTexture, vTexCoords + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
        result += texture(screenTexture, vTexCoords - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
    }
    FragColor = vec4(result, 1.0);
}
)";

/// Bloom 垂直模糊
constexpr const char* kBloomBlurVFS = R"(
uniform float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
void main() {
    vec2 tex_offset = 1.0 / textureSize(screenTexture, 0);
    vec3 result = texture(screenTexture, vTexCoords).rgb * weight[0];
    for(int i = 1; i < 5; ++i) {
        result += texture(screenTexture, vTexCoords + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
        result += texture(screenTexture, vTexCoords - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
    }
    FragColor = vec4(result, 1.0);
}
)";

/// Bloom 合成（ACES Filmic Tone Mapping + Gamma）
constexpr const char* kBloomCompositeFS = R"(
uniform sampler2D bloomBlur;
uniform float exposure;
uniform float bloomIntensity;

vec3 AcesFilmic(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 color = texture(screenTexture, vTexCoords).rgb;
    vec3 bloomColor = texture(bloomBlur, vTexCoords).rgb;
    color += bloomColor * bloomIntensity;
    color = AcesFilmic(color * exposure);
    color = pow(color, vec3(1.0 / 2.2));
    float ign = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
    color += (ign - 0.5) / 255.0;
    FragColor = vec4(color, 1.0);
}
)";

/// 默认后处理（直通）
constexpr const char* kPostProcessPassthroughFS = R"(
void main() {
    FragColor = texture(screenTexture, vTexCoords);
}
)";

/// FXAA
constexpr const char* kFxaaFS = R"(
layout(push_constant) uniform FxaaParams {
    vec2 u_resolution;
};

float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

void main() {
    vec2 texel = 1.0 / u_resolution;
    float lumaM  = luma(texture(screenTexture, vTexCoords).rgb);
    float lumaNW = luma(texture(screenTexture, vTexCoords + vec2(-1.0,-1.0) * texel).rgb);
    float lumaNE = luma(texture(screenTexture, vTexCoords + vec2( 1.0,-1.0) * texel).rgb);
    float lumaSW = luma(texture(screenTexture, vTexCoords + vec2(-1.0, 1.0) * texel).rgb);
    float lumaSE = luma(texture(screenTexture, vTexCoords + vec2( 1.0, 1.0) * texel).rgb);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    float lumaRange = lumaMax - lumaMin;
    if (lumaRange < max(0.0312, lumaMax * 0.125)) {
        FragColor = texture(screenTexture, vTexCoords);
        return;
    }
    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * 0.25, 1.0/128.0);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(8.0), max(vec2(-8.0), dir * rcpDirMin)) * texel;
    vec3 rgbA = 0.5 * (
        texture(screenTexture, vTexCoords + dir * (1.0/3.0 - 0.5)).rgb +
        texture(screenTexture, vTexCoords + dir * (2.0/3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(screenTexture, vTexCoords + dir * -0.5).rgb +
        texture(screenTexture, vTexCoords + dir *  0.5).rgb);
    float lumaB = luma(rgbB);
    if (lumaB < lumaMin || lumaB > lumaMax)
        FragColor = vec4(rgbA, 1.0);
    else
        FragColor = vec4(rgbB, 1.0);
}
)";

/// SSAO
constexpr const char* kSsaoFS = R"(
layout(push_constant) uniform SsaoParams {
    float u_radius;
    float u_bias;
    float u_near;
    float u_far;
    vec2 u_screen_size;
};

float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
}

vec3 reconstructNormal(vec2 uv) {
    vec2 texel = 1.0 / u_screen_size;
    float dc = linearizeDepth(texture(screenTexture, uv).r);
    float dl = linearizeDepth(texture(screenTexture, uv - vec2(texel.x, 0.0)).r);
    float dr = linearizeDepth(texture(screenTexture, uv + vec2(texel.x, 0.0)).r);
    float db = linearizeDepth(texture(screenTexture, uv - vec2(0.0, texel.y)).r);
    float dt = linearizeDepth(texture(screenTexture, uv + vec2(0.0, texel.y)).r);
    return normalize(vec3(dl - dr, db - dt, 2.0 * texel.x * dc));
}

const vec3 kernel[16] = vec3[](
    vec3( 0.5381, 0.1856,-0.4319), vec3( 0.1379, 0.2486, 0.4430),
    vec3( 0.3371, 0.5679,-0.0057), vec3(-0.6999,-0.0451,-0.0019),
    vec3( 0.0689,-0.1598,-0.8547), vec3( 0.0560, 0.0069,-0.1843),
    vec3(-0.0146, 0.1402, 0.0762), vec3( 0.0100,-0.1924,-0.0344),
    vec3(-0.3577,-0.5301,-0.4358), vec3(-0.3169, 0.1063, 0.0158),
    vec3( 0.0103,-0.5869, 0.0046), vec3(-0.0897,-0.4940, 0.3287),
    vec3( 0.7119,-0.0154,-0.0918), vec3(-0.0533, 0.0596,-0.5411),
    vec3( 0.0352,-0.0631, 0.5460), vec3(-0.4776, 0.2847,-0.0271)
);

void main() {
    float depth = texture(screenTexture, vTexCoords).r;
    if (depth >= 1.0) { FragColor = vec4(1.0); return; }
    float linDepth = linearizeDepth(depth);
    vec3 normal = reconstructNormal(vTexCoords);
    float occlusion = 0.0;
    float rScale = u_radius / linDepth;
    for (int i = 0; i < 16; ++i) {
        vec3 sampleDir = kernel[i];
        if (dot(sampleDir, normal) < 0.0) sampleDir = -sampleDir;
        vec2 sampleUV = vTexCoords + sampleDir.xy * rScale * (1.0 / u_screen_size);
        float sampleDepth = linearizeDepth(texture(screenTexture, sampleUV).r);
        float rangeCheck = smoothstep(0.0, 1.0, u_radius / abs(linDepth - sampleDepth));
        if (sampleDepth < linDepth - u_bias) occlusion += rangeCheck;
    }
    occlusion = 1.0 - (occlusion / 16.0);
    FragColor = vec4(vec3(occlusion), 1.0);
}
)";

/// SSAO Blur
constexpr const char* kSsaoBlurFS = R"(
void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(screenTexture, 0));
    float result = 0.0;
    for (int x = -2; x <= 2; ++x) {
        for (int y = -2; y <= 2; ++y) {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(screenTexture, vTexCoords + offset).r;
        }
    }
    FragColor = vec4(vec3(result / 25.0), 1.0);
}
)";

/// Contact Shadow (screen-space ray march toward light direction)
constexpr const char* kContactShadowFS = R"(
layout(push_constant) uniform ContactShadowParams {
    vec3 u_light_dir;
    float u_near;
    float u_far;
    vec2 u_screen_size;
    float u_strength;
    float u_step_size;
    int u_num_steps;
};

float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
}

void main() {
    float depth = texture(screenTexture, vTexCoords).r;
    if (depth >= 1.0) { FragColor = vec4(1.0); return; }
    float linDepth = linearizeDepth(depth);
    vec3 lightDir = normalize(u_light_dir);
    vec2 texelSize = 1.0 / u_screen_size;
    float occlusion = 0.0;
    int validSteps = 0;
    for (int i = 1; i <= u_num_steps; ++i) {
        float dist = u_step_size * float(i);
        vec2 sampleUV = vTexCoords + lightDir.xy * texelSize * dist * 50.0;
        if (sampleUV.x < 0.0 || sampleUV.y < 0.0 || sampleUV.x > 1.0 || sampleUV.y > 1.0) break;
        float sampleDepth = texture(screenTexture, sampleUV).r;
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
    FragColor = vec4(vec3(shadow), 1.0);
}
)";

/// Bloom 降采样 Compute Shader（13-tap Kawase）
constexpr const char* kBloomDownsampleCS = R"(
#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D u_src;
layout(set = 0, binding = 1, rgba16f) uniform writeonly image2D u_dst;

layout(push_constant) uniform BloomParams {
    float src_texel_w;
    float src_texel_h;
    float dst_texel_w;
    float dst_texel_h;
} u_params;

void main() {
    ivec2 dst_coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dst_size  = imageSize(u_dst);
    if (dst_coord.x >= dst_size.x || dst_coord.y >= dst_size.y) return;

    vec2 uv = (vec2(dst_coord) + 0.5) * vec2(u_params.dst_texel_w, u_params.dst_texel_h);
    float x = u_params.src_texel_w;
    float y = u_params.src_texel_h;

    vec3 a = texture(u_src, uv + vec2(-2.0*x,  2.0*y)).rgb;
    vec3 b = texture(u_src, uv + vec2(  0.0,   2.0*y)).rgb;
    vec3 c = texture(u_src, uv + vec2( 2.0*x,  2.0*y)).rgb;
    vec3 d = texture(u_src, uv + vec2(-2.0*x,    0.0)).rgb;
    vec3 e = texture(u_src, uv).rgb;
    vec3 f = texture(u_src, uv + vec2( 2.0*x,    0.0)).rgb;
    vec3 g = texture(u_src, uv + vec2(-2.0*x, -2.0*y)).rgb;
    vec3 h = texture(u_src, uv + vec2(  0.0,  -2.0*y)).rgb;
    vec3 i = texture(u_src, uv + vec2( 2.0*x, -2.0*y)).rgb;
    vec3 j = texture(u_src, uv + vec2(    -x,      y)).rgb;
    vec3 k = texture(u_src, uv + vec2(     x,      y)).rgb;
    vec3 l = texture(u_src, uv + vec2(    -x,     -y)).rgb;
    vec3 m = texture(u_src, uv + vec2(     x,     -y)).rgb;

    vec3 result = e * 0.125
        + (a + c + g + i) * 0.03125
        + (b + d + f + h) * 0.0625
        + (j + k + l + m) * 0.125;

    imageStore(u_dst, dst_coord, vec4(result, 1.0));
}
)";

/// Bloom 升采样 Compute Shader（3x3 tent filter）
constexpr const char* kBloomUpsampleCS = R"(
#version 450
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D u_src;
layout(set = 0, binding = 1, rgba16f) uniform writeonly image2D u_dst;

layout(push_constant) uniform BloomParams {
    float src_texel_w;
    float src_texel_h;
    float dst_texel_w;
    float dst_texel_h;
} u_params;

void main() {
    ivec2 dst_coord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 dst_size  = imageSize(u_dst);
    if (dst_coord.x >= dst_size.x || dst_coord.y >= dst_size.y) return;

    vec2 uv = (vec2(dst_coord) + 0.5) * vec2(u_params.dst_texel_w, u_params.dst_texel_h);
    float x = u_params.src_texel_w;
    float y = u_params.src_texel_h;

    vec3 a = texture(u_src, uv + vec2(-x,  y)).rgb;
    vec3 b = texture(u_src, uv + vec2( 0,  y)).rgb;
    vec3 c = texture(u_src, uv + vec2( x,  y)).rgb;
    vec3 d = texture(u_src, uv + vec2(-x,  0)).rgb;
    vec3 e = texture(u_src, uv).rgb;
    vec3 f = texture(u_src, uv + vec2( x,  0)).rgb;
    vec3 g = texture(u_src, uv + vec2(-x, -y)).rgb;
    vec3 h = texture(u_src, uv + vec2( 0, -y)).rgb;
    vec3 i = texture(u_src, uv + vec2( x, -y)).rgb;

    vec3 result = (e * 4.0 + (b + d + f + h) * 2.0 + (a + c + g + i)) * (1.0 / 16.0);
    imageStore(u_dst, dst_coord, vec4(result, 1.0));
}
)";

/// Auto Exposure: Luminance 计算 (64 样本 log 平均)
constexpr const char* kLumComputeFS = R"(
void main() {
    float logSum = 0.0;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            vec2 uv = (vec2(float(i), float(j)) + 0.5) / 8.0;
            vec3 c = texture(screenTexture, uv).rgb;
            float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
            logSum += log(max(lum, 0.0001));
        }
    }
    float avgLogLum = logSum / 64.0;
    FragColor = vec4(avgLogLum, 0.0, 0.0, 1.0);
}
)";

/// Auto Exposure: EMA 自适应曝光
constexpr const char* kLumAdaptFS = R"(
layout(set = 2, binding = 2) uniform sampler2D prevAdaptedTex;
layout(push_constant) uniform LumAdaptParams {
    float u_dt;
    float u_speed_up;
    float u_speed_down;
    float u_min_exposure;
    float u_max_exposure;
    float u_compensation;
};
void main() {
    float avgLogLum = texture(screenTexture, vec2(0.5, 0.5)).r;
    float avgLum = exp(avgLogLum);
    float targetExposure = 0.18 / max(avgLum, 0.001);
    targetExposure = clamp(targetExposure * exp2(u_compensation), u_min_exposure, u_max_exposure);
    float prevExposure = texture(prevAdaptedTex, vec2(0.5, 0.5)).r;
    if (prevExposure <= 0.0) prevExposure = targetExposure;
    float speed = (targetExposure > prevExposure) ? u_speed_up : u_speed_down;
    float adapted = prevExposure + (targetExposure - prevExposure) * (1.0 - exp(-u_dt * speed));
    FragColor = vec4(adapted, 0.0, 0.0, 1.0);
}
)";

/// Tonemapping（带可选 Auto Exposure + LUT）
constexpr const char* kTonemappingFS = R"(
layout(set = 2, binding = 2) uniform sampler2D autoExposureTex;
layout(set = 2, binding = 5) uniform sampler3D u_lut;
layout(push_constant) uniform TonemapParams {
    float u_manual_exposure;
    int u_auto_exposure_enabled;
    int u_lut_enabled;
    float u_lut_intensity;
};
vec3 AcesFilmic(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
void main() {
    vec3 hdrColor = texture(screenTexture, vTexCoords).rgb;
    float finalExposure = u_manual_exposure;
    if (u_auto_exposure_enabled != 0) {
        finalExposure = texture(autoExposureTex, vec2(0.5, 0.5)).r;
    }
    vec3 result = AcesFilmic(hdrColor * finalExposure);
    result = pow(result, vec3(1.0 / 2.2));
    if (u_lut_enabled != 0) {
        vec3 lutColor = texture(u_lut, clamp(result, 0.0, 1.0)).rgb;
        result = mix(result, lutColor, u_lut_intensity);
    }
    float ign = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
    result += (ign - 0.5) / 255.0;
    FragColor = vec4(result, 1.0);
}
)";

/// Bloom Composite + SSAO + Auto Exposure + LUT + Contact Shadow + Vignette + Film Grain
constexpr const char* kBloomCompositeSsaoAeFS = R"(
layout(set = 2, binding = 2) uniform sampler2D bloomBlur;
layout(set = 2, binding = 3) uniform sampler2D ssaoTexture;
layout(set = 2, binding = 4) uniform sampler2D autoExposureTex;
layout(set = 2, binding = 5) uniform sampler3D u_lut;
layout(set = 2, binding = 6) uniform sampler2D contactShadowTex;
layout(push_constant) uniform BloomCompositeAeParams {
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
vec3 AcesFilmic(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
float GrainNoise(vec2 uv, float time_seed) {
    return fract(sin(dot(uv + vec2(time_seed, time_seed * 0.37), vec2(12.9898, 78.233))) * 43758.5453);
}
void main() {
    vec3 color = texture(screenTexture, vTexCoords).rgb;
    if (ssaoEnabled != 0) {
        float ao = texture(ssaoTexture, vTexCoords).r;
        color *= ao;
    }
    if (bloomEnabled != 0) {
        vec3 bloomColor = texture(bloomBlur, vTexCoords).rgb;
        color += bloomColor * bloomIntensity;
    }
    if (csEnabled != 0) {
        float cs = texture(contactShadowTex, vTexCoords).r;
        color *= (1.0 - (1.0 - cs) * csStrength);
    }
    float finalExposure = exposure;
    if (autoExposureEnabled != 0) {
        finalExposure = texture(autoExposureTex, vec2(0.5, 0.5)).r;
    }
    color = AcesFilmic(color * finalExposure);
    color = pow(color, vec3(1.0 / 2.2));
    if (lutEnabled != 0) {
        vec3 lutColor = texture(u_lut, clamp(color, 0.0, 1.0)).rgb;
        color = mix(color, lutColor, lutIntensity);
    }
    if (vignetteEnabled != 0) {
        float dist = length(vTexCoords - vec2(0.5));
        float radius = clamp(vignetteRadius, 0.001, 1.5);
        float softness = max(vignetteSoftness, 0.0001);
        float vignette = 1.0 - smoothstep(radius, radius + softness, dist);
        color *= mix(1.0, vignette, clamp(vignetteIntensity, 0.0, 1.0));
    }
    if (filmGrainEnabled != 0) {
        float grain = GrainNoise(vTexCoords * vec2(1280.0, 720.0), filmGrainTime) - 0.5;
        color = clamp(color + grain * filmGrainIntensity, 0.0, 1.0);
    }
    float ign = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
    color += (ign - 0.5) / 255.0;
    FragColor = vec4(color, 1.0);
}
)";

/// Color Grading (LUT only, no tonemapping)
constexpr const char* kColorGradingFS = R"(
layout(set = 2, binding = 5) uniform sampler3D u_lut;
layout(push_constant) uniform ColorGradingParams {
    float u_lut_intensity;
};
void main() {
    vec3 color = texture(screenTexture, vTexCoords).rgb;
    vec3 lutColor = texture(u_lut, clamp(color, 0.0, 1.0)).rgb;
    color = mix(color, lutColor, u_lut_intensity);
    float ign = fract(52.9829189 * fract(0.06711056 * gl_FragCoord.x + 0.00583715 * gl_FragCoord.y));
    color += (ign - 0.5) / 255.0;
    FragColor = vec4(color, 1.0);
}
)";

/// TAA Resolve — motion vector reprojection + variance clipping
constexpr const char* kTaaResolveFS = R"(
layout(set = 2, binding = 5) uniform sampler2D u_history;
layout(set = 2, binding = 2) uniform sampler2D u_motion_vector;
layout(push_constant) uniform TaaParams {
    float u_blend_factor;
    float u_jitter_x;
    float u_jitter_y;
    int   u_frame_index;
    float u_screen_w;
    float u_screen_h;
};
void main() {
    vec3 current = texture(screenTexture, vTexCoords).rgb;

    // 使用 motion vector 做重投影（比纯 jitter 更精确）
    vec2 mv = texture(u_motion_vector, vTexCoords).rg;
    vec2 history_uv = vTexCoords - mv - vec2(u_jitter_x, u_jitter_y);
    history_uv = clamp(history_uv, vec2(0.0), vec2(1.0));

    // 邻域 3×3 variance clipping
    vec2 texel = 1.0 / vec2(u_screen_w, u_screen_h);
    vec3 m1 = vec3(0.0), m2 = vec3(0.0);
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            vec3 s = texture(screenTexture, vTexCoords + vec2(dx, dy) * texel).rgb;
            m1 += s;
            m2 += s * s;
        }
    }
    m1 /= 9.0;
    vec3 sigma = sqrt(max(m2 / 9.0 - m1 * m1, vec3(0.0)));
    vec3 aabb_min = m1 - 1.25 * sigma;
    vec3 aabb_max = m1 + 1.25 * sigma;

    vec3 history = texture(u_history, history_uv).rgb;
    history = clamp(history, aabb_min, aabb_max);

    // velocity rejection：运动越快，越偏向当前帧
    float velocity_len = length(mv * vec2(u_screen_w, u_screen_h));
    float vel_weight = clamp(velocity_len * 0.5, 0.0, 0.5);
    float alpha = u_frame_index < 2 ? 1.0 : clamp(u_blend_factor + vel_weight, u_blend_factor, 1.0);

    FragColor = vec4(mix(history, current, alpha), 1.0);
}
)";

// ============================================================
// Motion Vector Fragment Shader (深度重投影 → 速度场)
// ============================================================
constexpr const char* kMotionVectorFS = R"(
layout(push_constant) uniform MvParams {
    float screen_w;
    float screen_h;
    mat4 reproj;
};
void main() {
    float depth = texture(screenTexture, vTexCoords).r;
    vec2 ndc = vTexCoords * 2.0 - 1.0;
    float z_ndc = depth * 2.0 - 1.0;
    vec4 clip_pos = vec4(ndc, z_ndc, 1.0);
    vec4 prev_clip = reproj * clip_pos;
    prev_clip.xy /= prev_clip.w;
    vec2 prev_uv = prev_clip.xy * 0.5 + 0.5;
    vec2 velocity = vTexCoords - prev_uv;
    FragColor = vec4(velocity, 0.0, 1.0);
}
)";

// ============================================================
// DOF (Depth of Field) Fragment Shader
// ============================================================
constexpr const char* kDofFS = R"(
layout(set = 2, binding = 2) uniform sampler2D u_color_texture;
layout(push_constant) uniform DofParams {
    float focus_distance;
    float focus_range;
    float bokeh_radius;
    float near_plane;
    float far_plane;
    float screen_w;
    float screen_h;
};
float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
}
void main() {
    float depth = texture(screenTexture, vTexCoords).r;
    float lin_depth = linearizeDepth(depth);
    float coc = clamp(abs(lin_depth - focus_distance) / focus_range, 0.0, 1.0);
    vec2 texel = 1.0 / vec2(screen_w, screen_h);
    float radius = coc * bokeh_radius;
    vec3 color = vec3(0.0);
    float total_weight = 0.0;
    const int SAMPLES = 16;
    const float GOLDEN_ANGLE = 2.39996323;
    for (int i = 0; i < SAMPLES; ++i) {
        float r = sqrt(float(i) / float(SAMPLES)) * radius;
        float theta = float(i) * GOLDEN_ANGLE;
        vec2 offset = vec2(cos(theta), sin(theta)) * r * texel;
        float sample_depth = linearizeDepth(texture(screenTexture, vTexCoords + offset).r);
        float sample_coc = clamp(abs(sample_depth - focus_distance) / focus_range, 0.0, 1.0);
        float w = max(sample_coc, coc);
        color += texture(u_color_texture, vTexCoords + offset).rgb * w;
        total_weight += w;
    }
    if (total_weight > 0.0) color /= total_weight;
    else color = texture(u_color_texture, vTexCoords).rgb;
    FragColor = vec4(color, 1.0);
}
)";

// ============================================================
// Motion Blur Fragment Shader
// ============================================================
constexpr const char* kMotionBlurFS = R"(
layout(set = 2, binding = 2) uniform sampler2D u_color_texture;
layout(push_constant) uniform MotionBlurParams {
    float intensity;
    float num_samples;
    float screen_w;
    float screen_h;
};
void main() {
    // screenTexture = motion_vector RT (rg = velocity)
    vec2 velocity = texture(screenTexture, vTexCoords).rg * intensity;
    int samples = max(int(num_samples), 1);
    vec3 color = texture(u_color_texture, vTexCoords).rgb;
    float total = 1.0;
    for (int i = 1; i < samples; ++i) {
        float t = float(i) / float(samples);
        vec2 sample_uv = vTexCoords + velocity * t;
        if (sample_uv.x >= 0.0 && sample_uv.x <= 1.0 && sample_uv.y >= 0.0 && sample_uv.y <= 1.0) {
            color += texture(u_color_texture, sample_uv).rgb;
            total += 1.0;
        }
    }
    FragColor = vec4(color / total, 1.0);
}
)";

// ============================================================
// SSR (Screen Space Reflections) Fragment Shader
// ============================================================
constexpr const char* kSsrFS = R"(
layout(set = 2, binding = 2) uniform sampler2D u_color_texture;
layout(push_constant) uniform SsrParams {
    float max_distance;
    float thickness;
    float step_size;
    int max_steps;
    float near_plane;
    float far_plane;
    float screen_w;
    float screen_h;
};
float linearizeDepth(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
}
vec3 reconstructNormal(vec2 uv) {
    vec2 texel = 1.0 / vec2(screen_w, screen_h);
    float dc = linearizeDepth(texture(screenTexture, uv).r);
    float dl = linearizeDepth(texture(screenTexture, uv - vec2(texel.x, 0.0)).r);
    float dr = linearizeDepth(texture(screenTexture, uv + vec2(texel.x, 0.0)).r);
    float db = linearizeDepth(texture(screenTexture, uv - vec2(0.0, texel.y)).r);
    float dt = linearizeDepth(texture(screenTexture, uv + vec2(0.0, texel.y)).r);
    return normalize(vec3(dl - dr, db - dt, 2.0 * texel.x * dc));
}
void main() {
    float depth = texture(screenTexture, vTexCoords).r;
    if (depth >= 1.0) { FragColor = vec4(0.0); return; }
    float lin_depth = linearizeDepth(depth);
    vec3 normal = reconstructNormal(vTexCoords);
    vec3 view_dir = vec3(vTexCoords * 2.0 - 1.0, 1.0);
    view_dir = normalize(view_dir);
    vec3 reflect_dir = reflect(view_dir, normal);
    vec2 texel = 1.0 / vec2(screen_w, screen_h);
    vec2 ray_uv = vTexCoords;
    float ray_depth = lin_depth;
    for (int i = 0; i < max_steps; ++i) {
        ray_uv += reflect_dir.xy * texel * step_size;
        if (ray_uv.x < 0.0 || ray_uv.x > 1.0 || ray_uv.y < 0.0 || ray_uv.y > 1.0) break;
        float sample_depth = linearizeDepth(texture(screenTexture, ray_uv).r);
        ray_depth += reflect_dir.z * step_size;
        float depth_diff = ray_depth - sample_depth;
        if (depth_diff > 0.0 && depth_diff < thickness) {
            float fade = 1.0 - float(i) / float(max_steps);
            vec3 hit_color = texture(u_color_texture, ray_uv).rgb;
            FragColor = vec4(hit_color * fade, fade);
            return;
        }
    }
    FragColor = vec4(0.0);
}
)";

// ============================================================
// GBuffer 几何通道片段着色器（MRT 输出 albedo/normal/position）
// ============================================================

constexpr const char* kGBufferFragment = R"(#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vTexCoord;
layout(location = 2) in vec3 vFragPos;
layout(location = 3) in vec3 vNormal;

// Set 0: PerFrame（由 VS 使用，此处保持 layout 兼容）
layout(std140, set = 0, binding = 0) uniform PerFrame {
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
};

// Set 1: PerScene（占位，保持与 PBR 管线 descriptor layout 兼容）
layout(std140, set = 1, binding = 0) uniform PerScene {
    vec4 _gbuf_dummy;
};

// Set 2: Samplers
layout(set = 2, binding = 1) uniform sampler2D u_texture;

layout(location = 0) out vec4 gAlbedo;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gPosition;

void main() {
    vec4 albedo = texture(u_texture, vTexCoord) * vColor;
    if (albedo.a < 0.01) discard;
    gAlbedo   = albedo;
    gNormal   = vec4(normalize(vNormal) * 0.5 + 0.5, 1.0);
    gPosition = vec4(vFragPos, 1.0);
}
)";

// ============================================================
// Deferred Lighting 片段着色器（全屏后处理光照合成）
// ============================================================

// kDeferredLightingFS 不含 #version header，需与 kPostProcessHeader 拼接使用
constexpr const char* kDeferredLightingFS = R"(
layout(set = 2, binding = 2) uniform sampler2D u_gbuf_normal;
layout(set = 2, binding = 3) uniform sampler2D u_gbuf_position;

layout(push_constant) uniform DeferredLightParams {
    vec3 u_light_dir;
    float u_light_intensity;
    vec3 u_light_color;
    float u_ambient;
};

void main() {
    vec3 albedo   = texture(screenTexture, vTexCoords).rgb;
    vec3 normal   = texture(u_gbuf_normal, vTexCoords).rgb * 2.0 - 1.0;
    vec3 position = texture(u_gbuf_position, vTexCoords).rgb;
    if (length(normal) < 0.01) { FragColor = vec4(0.0, 0.0, 0.0, 1.0); return; }
    normal = normalize(normal);
    float NdotL = max(dot(normal, -normalize(u_light_dir)), 0.0);
    vec3 diffuse = albedo * u_light_color * u_light_intensity * NdotL;
    vec3 ambient_color = albedo * u_ambient;
    FragColor = vec4(diffuse + ambient_color, 1.0);
}
)";

// ============================================================
// Edge Detection (Outline) Fragment Shader
// ============================================================
constexpr const char* kEdgeDetectFS = R"(
layout(push_constant) uniform EdgeDetectParams {
    float u_thickness;
    float u_depth_threshold;
    float u_normal_threshold;
    float u_outline_r;
    float u_outline_g;
    float u_outline_b;
    float u_near;
    float u_far;
    float u_screen_w;
    float u_screen_h;
};
float linearize_depth(float d) {
    float ndc = d * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - ndc * (u_far - u_near));
}
vec3 reconstruct_normal(vec2 uv, vec2 texel_size) {
    float dc = linearize_depth(texture(screenTexture, uv).r);
    float dl = linearize_depth(texture(screenTexture, uv - vec2(texel_size.x, 0.0)).r);
    float dr = linearize_depth(texture(screenTexture, uv + vec2(texel_size.x, 0.0)).r);
    float db = linearize_depth(texture(screenTexture, uv - vec2(0.0, texel_size.y)).r);
    float dt = linearize_depth(texture(screenTexture, uv + vec2(0.0, texel_size.y)).r);
    return normalize(vec3(dl - dr, db - dt, 2.0 * texel_size.x * dc));
}
void main() {
    vec2 base_texel = vec2(1.0 / u_screen_w, 1.0 / u_screen_h);
    vec2 texel = base_texel * u_thickness;

    float d_c = linearize_depth(texture(screenTexture, vTexCoords).r);
    float d_l = linearize_depth(texture(screenTexture, vTexCoords + vec2(-texel.x, 0.0)).r);
    float d_r = linearize_depth(texture(screenTexture, vTexCoords + vec2( texel.x, 0.0)).r);
    float d_t = linearize_depth(texture(screenTexture, vTexCoords + vec2(0.0,  texel.y)).r);
    float d_b = linearize_depth(texture(screenTexture, vTexCoords + vec2(0.0, -texel.y)).r);

    float depth_diff = abs(d_l - d_r) + abs(d_t - d_b);
    float depth_edge = smoothstep(0.0, u_depth_threshold * d_c, depth_diff);

    vec3 n_c = reconstruct_normal(vTexCoords, base_texel);
    vec3 n_l = reconstruct_normal(vTexCoords + vec2(-texel.x, 0.0), base_texel);
    vec3 n_r = reconstruct_normal(vTexCoords + vec2( texel.x, 0.0), base_texel);
    vec3 n_t = reconstruct_normal(vTexCoords + vec2(0.0,  texel.y), base_texel);
    vec3 n_b = reconstruct_normal(vTexCoords + vec2(0.0, -texel.y), base_texel);
    float normal_diff = length(n_l - n_r) + length(n_t - n_b);
    float normal_edge = smoothstep(0.0, u_normal_threshold, normal_diff);

    float edge = clamp(max(depth_edge, normal_edge), 0.0, 1.0);
    FragColor = vec4(u_outline_r, u_outline_g, u_outline_b, edge);
}
)";

// ============================================================
// Volumetric Fog Fragment Shader
// ============================================================
constexpr const char* kVolumetricFogFS = R"(
layout(binding = 2) uniform sampler2D u_depth_tex;
layout(push_constant) uniform VolumetricFogParams {
    float u_depth_handle;
    float u_fog_r;    float u_fog_g;    float u_fog_b;
    float u_fog_density;
    float u_height_falloff;
    float u_height_offset;
    float u_fog_start;
    float u_fog_end;
    float u_fog_steps;
    float u_sun_scatter;
    float u_sun_dir_x; float u_sun_dir_y; float u_sun_dir_z;
    float u_cam_pos_x; float u_cam_pos_y; float u_cam_pos_z;
    float u_near;      float u_far;
    float u_right_x;   float u_right_y;  float u_right_z;
    float u_up_x;      float u_up_y;     float u_up_z;
    float u_fwd_x;     float u_fwd_y;    float u_fwd_z;
    float u_tan_fov_y;
    float u_aspect;
};
float VFogLinZ(float d) {
    float z = d * 2.0 - 1.0;
    return (2.0 * u_near * u_far) / (u_far + u_near - z * (u_far - u_near));
}
void main() {
    vec4 scene = texture(screenTexture, vTexCoords);
    float depth = texture(u_depth_tex, vTexCoords).r;
    if (depth >= 0.9999) { FragColor = scene; return; }

    float viewZ = VFogLinZ(depth);
    vec2 ndc = vTexCoords * 2.0 - 1.0;
    vec3 camFwd   = vec3(u_fwd_x,   u_fwd_y,   u_fwd_z);
    vec3 camRight = vec3(u_right_x, u_right_y, u_right_z);
    vec3 camUp    = vec3(u_up_x,    u_up_y,    u_up_z);
    vec3 viewDir = normalize(camFwd
        + ndc.x * camRight * u_tan_fov_y * u_aspect
        + ndc.y * camUp    * u_tan_fov_y);
    float cosAngle = max(dot(viewDir, camFwd), 0.0001);
    float rayLen   = viewZ / cosAngle;

    float marchStart = u_fog_start;
    float marchEnd   = min(rayLen, u_fog_end);
    float steps = max(u_fog_steps, 1.0);
    if (marchEnd <= marchStart) { FragColor = scene; return; }

    float stepLen  = (marchEnd - marchStart) / steps;
    vec3  sunDir   = vec3(u_sun_dir_x, u_sun_dir_y, u_sun_dir_z);
    float cosTheta = dot(viewDir, -sunDir);
    float g = 0.76; float g2 = g * g;
    float mie = (1.0 - g2) / (4.0 * 3.14159265 *
        pow(max(1.0 + g2 - 2.0 * g * cosTheta, 0.001), 1.5));

    vec3  fogColor  = vec3(u_fog_r, u_fog_g, u_fog_b);
    vec3  camPos    = vec3(u_cam_pos_x, u_cam_pos_y, u_cam_pos_z);
    float transmit  = 1.0;
    vec3  inscatter = vec3(0.0);
    for (float i = 0.0; i < steps; i += 1.0) {
        float t   = marchStart + (i + 0.5) * stepLen;
        vec3  pos = camPos + viewDir * t;
        float h   = max(pos.y - u_height_offset, 0.0);
        float den = u_fog_density * exp(-u_height_falloff * h);
        float sT  = exp(-den * stepLen);
        inscatter += transmit * (1.0 - sT) * (fogColor + mie * u_sun_scatter * vec3(1.0));
        transmit *= sT;
        if (transmit < 0.001) break;
    }
    FragColor = vec4(scene.rgb * transmit + inscatter, scene.a);
}
)";

// ============================================================
// Screen-Space Decal Fragment Shader
// ============================================================
constexpr const char* kDecalFS = R"(
layout(binding = 2) uniform sampler2D u_depth_tex;
layout(binding = 3) uniform sampler2D u_decal_tex;
layout(push_constant) uniform DecalParams {
    float u_depth_handle; float u_decal_handle;
    float m00; float m01; float m02; float m03;
    float m10; float m11; float m12; float m13;
    float m20; float m21; float m22; float m23;
    float m30; float m31; float m32; float m33;
    float u_color_r; float u_color_g; float u_color_b; float u_color_a;
    float u_angle_fade;
    float u_decal_up_x; float u_decal_up_y; float u_decal_up_z;
};
void main() {
    float depth = texture(u_depth_tex, vTexCoords).r;
    if (depth >= 0.9999) discard;

    mat4 inv_mvp = mat4(
        vec4(m00, m01, m02, m03),
        vec4(m10, m11, m12, m13),
        vec4(m20, m21, m22, m23),
        vec4(m30, m31, m32, m33));
    vec4 clip = vec4(vTexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 local4 = inv_mvp * clip;
    vec3 local = local4.xyz / local4.w;

    if (abs(local.x) > 0.5 || abs(local.y) > 0.5 || abs(local.z) > 0.5) discard;

    vec2 decal_uv = local.xz + 0.5;
    vec4 color = vec4(u_color_r, u_color_g, u_color_b, u_color_a);
    vec4 decal = texture(u_decal_tex, decal_uv) * color;

    float angle_factor = 1.0;
    if (u_angle_fade > 0.0) {
        vec2 texel = 1.0 / textureSize(u_depth_tex, 0);
        float dl = texture(u_depth_tex, vTexCoords + vec2(-texel.x, 0.0)).r;
        float dr = texture(u_depth_tex, vTexCoords + vec2( texel.x, 0.0)).r;
        float dt = texture(u_depth_tex, vTexCoords + vec2(0.0,  texel.y)).r;
        float db = texture(u_depth_tex, vTexCoords + vec2(0.0, -texel.y)).r;
        vec3 normal = normalize(vec3(dl - dr, dt - db, 2.0 * texel.x));
        vec3 decal_up = vec3(u_decal_up_x, u_decal_up_y, u_decal_up_z);
        float facing = abs(dot(normal, decal_up));
        angle_factor = smoothstep(0.0, 1.0 - u_angle_fade, facing);
    }
    FragColor = vec4(decal.rgb, decal.a * angle_factor);
}
)";

} // namespace vulkan_shaders
} // namespace render
} // namespace dse

#endif // DSE_RENDER_VULKAN_SHADER_SOURCES_H
