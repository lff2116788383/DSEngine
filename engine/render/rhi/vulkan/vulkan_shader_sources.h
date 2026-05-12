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
    vec4 worldPos = pc.model * localPos;
    gl_Position = vp * worldPos;

    vFragPos = worldPos.xyz;
    vFragPosViewSpace = (view * worldPos).xyz;
    vColor = aColor;
    vTexCoord = aTexCoord;

    mat3 normalMatrix = transpose(inverse(mat3(pc.model * boneTransform)));
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

void main() {
    vec4 texColor = texture(u_texture, vTexCoord);
    if (u_material_alpha_test && texColor.a < clamp(u_material_alpha_cutoff, 0.0, 1.0)) discard;

    vec3 N = vNormal;
    if (u_has_normal_map) {
        vec3 normalMap = texture(u_normal_map, vTexCoord).rgb;
        normalMap = normalMap * 2.0 - 1.0;
        normalMap.xy *= u_material_normal_strength;
        N = normalize(vTBN * normalMap);
    }

    if (!u_lighting_enabled) {
        vec3 result = texColor.rgb * vColor.rgb * u_material_albedo;
        if (u_has_emissive_map) {
            result += texture(u_emissive_map, vTexCoord).rgb * u_material_emissive;
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
            ? texture(u_metallic_roughness_map, vTexCoord).rgb
            : vec3(0.0);
        vec3 specular_color = spec_tex * spec_brightness;
        float shadow = ShadowCalculation(vFragPos, vFragPosViewSpace, N, L);
        float shadow_multiplier = 1.0 - shadow * 0.5;
        vec3 color = (diffuse_color + specular_color) * shadow_multiplier;
        FragColor = vec4(color, 1.0);
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
        vec4 mrSample = texture(u_metallic_roughness_map, vTexCoord);
        roughness = clamp(mrSample.g * u_material_roughness, 0.04, 1.0);
        metallic = clamp(mrSample.b * u_material_metallic, 0.0, 1.0);
    }
    if (u_has_occlusion_map) {
        ao *= texture(u_occlusion_map, vTexCoord).r;
    }
    if (u_has_emissive_map) {
        surface_emissive *= texture(u_emissive_map, vTexCoord).rgb;
    }
    vec3 V = normalize(u_camera_pos - vFragPos);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, surface_albedo, metallic);

    vec3 Lo = vec3(0.0);

    // 方向光
    {
        vec3 L = normalize(-u_light_direction);
        vec3 H = normalize(V + L);
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
        float shadow = ShadowCalculation(vFragPos, vFragPosViewSpace, N, L);
        Lo += (kD * surface_albedo / PI + specular) * u_light_color * u_light_intensity * NdotL * (1.0 - shadow);
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
    FragColor = vec4(result, 1.0);
}
)";

/// Bloom Composite + SSAO + Auto Exposure + LUT
constexpr const char* kBloomCompositeSsaoAeFS = R"(
layout(set = 2, binding = 2) uniform sampler2D bloomBlur;
layout(set = 2, binding = 3) uniform sampler2D ssaoTexture;
layout(set = 2, binding = 4) uniform sampler2D autoExposureTex;
layout(set = 2, binding = 5) uniform sampler3D u_lut;
layout(push_constant) uniform BloomCompositeAeParams {
    float exposure;
    float bloomIntensity;
    int ssaoEnabled;
    int autoExposureEnabled;
    int lutEnabled;
    float lutIntensity;
};
vec3 AcesFilmic(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
void main() {
    vec3 color = texture(screenTexture, vTexCoords).rgb;
    if (ssaoEnabled != 0) {
        float ao = texture(ssaoTexture, vTexCoords).r;
        color *= ao;
    }
    vec3 bloomColor = texture(bloomBlur, vTexCoords).rgb;
    color += bloomColor * bloomIntensity;
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
    FragColor = vec4(color, 1.0);
}
)";

} // namespace vulkan_shaders
} // namespace render
} // namespace dse

#endif // DSE_RENDER_VULKAN_SHADER_SOURCES_H
