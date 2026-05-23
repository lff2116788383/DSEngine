#version 450
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
#define MAX_POINT_LIGHTS 256
layout(std430, set = 1, binding = 1) readonly buffer PointLightSSBO {
    int u_point_light_count;
    int _pl_pad0;
    int _pl_pad1;
    int _pl_pad2;
    PointLight u_point_lights[];
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
#define MAX_SPOT_LIGHTS 256
layout(std430, set = 1, binding = 2) readonly buffer SpotLightSSBO {
    int u_spot_light_count;
    int _sl_pad0;
    int _sl_pad1;
    int _sl_pad2;
    SpotLight u_spot_lights[];
};

// Clustered Forward+: cluster 网格参数 + 每 cluster 光源列表
struct ClusterInfoEntry {
    uint offset;
    uint point_count;
    uint spot_count;
    uint _pad;
};
layout(std430, set = 1, binding = 3) readonly buffer ClusterInfoSSBO {
    uint cluster_tiles_x;
    uint cluster_tiles_y;
    uint cluster_z_slices;
    float cluster_near;
    float cluster_far;
    uint _ci_pad0, _ci_pad1, _ci_pad2;
    ClusterInfoEntry cluster_infos[];
};
layout(std430, set = 1, binding = 4) readonly buffer LightIndexSSBO {
    uint light_indices[];
};

// Set 1: LightProbeData (SH L2 间接漫反射)
layout(std140, set = 1, binding = 5) uniform LightProbeData {
    vec4 sh_coefficients[9];
    vec4 probe_params;
    // probe_params: x=sh_enabled, y=ibl_enabled, z=unused, w=unused
};
#define u_sh_enabled  (probe_params.x > 0.5)
#define u_ibl_enabled (probe_params.y > 0.5)

// Terrain Splatmap (standalone uniforms, binding 11-15)
layout(set = 2, binding = 11) uniform sampler2D u_splat_weight_map;
layout(set = 2, binding = 12) uniform sampler2D u_splat_layer0;
layout(set = 2, binding = 13) uniform sampler2D u_splat_layer1;
layout(set = 2, binding = 14) uniform sampler2D u_splat_layer2;
layout(set = 2, binding = 15) uniform sampler2D u_splat_layer3;
layout(std140, set = 2, binding = 16) uniform TerrainParams {
    float u_splat_enabled;  // >0.5 = splatmap mode
    float _tp_pad0;
    float _tp_pad1;
    float _tp_pad2;
    vec4  u_splat_tiling;   // per-layer UV tiling factor
};

// IBL: Reflection Probe (Set 2, binding 17/18)
// 注意：binding 8/9 被 pbr.vert 用于 BoneMatrices / MorphWeights UBO，binding 16 被
// TerrainParams UBO 占用。同 set/binding 不能跨 stage 复用为不同 descriptor type
// （VUID-VkGraphicsPipelineCreateInfo-layout-07990），故 IBL sampler 移到 17/18。
layout(set = 2, binding = 17) uniform samplerCube u_reflection_cubemap;
layout(set = 2, binding = 18) uniform sampler2D   u_brdf_lut;

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
#define u_wboit_mode          cascade_splits.w

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

vec3 EvaluateSH(vec3 N) {
    vec3 result = sh_coefficients[0].xyz *  0.282095
               + sh_coefficients[1].xyz *  0.488603 * N.y
               + sh_coefficients[2].xyz *  0.488603 * N.z
               + sh_coefficients[3].xyz *  0.488603 * N.x
               + sh_coefficients[4].xyz *  1.092548 * N.x * N.y
               + sh_coefficients[5].xyz *  1.092548 * N.y * N.z
               + sh_coefficients[6].xyz *  0.315392 * (3.0 * N.z * N.z - 1.0)
               + sh_coefficients[7].xyz *  1.092548 * N.x * N.z
               + sh_coefficients[8].xyz *  0.546274 * (N.x * N.x - N.y * N.y);
    return max(result, vec3(0.0));
}

vec2 ParallaxOcclusionMapping(vec2 uv, vec3 viewDirTS, float height_scale) {
    const int numLayers = 16;
    float layerDepth = 1.0 / float(numLayers);
    float currentLayerDepth = 0.0;
    vec2 P = viewDirTS.xy / max(viewDirTS.z, 0.001) * height_scale;
    vec2 deltaUV = P / float(numLayers);
    vec2 curUV = uv;
    float curDepth = 1.0 - textureLod(u_normal_map, curUV, 0.0).a;
    for (int i = 0; i < numLayers; ++i) {
        if (currentLayerDepth >= curDepth) break;
        curUV -= deltaUV;
        curDepth = 1.0 - textureLod(u_normal_map, curUV, 0.0).a;
        currentLayerDepth += layerDepth;
    }
    vec2 prevUV = curUV + deltaUV;
    float afterDepth = curDepth - currentLayerDepth;
    float beforeDepth = (1.0 - textureLod(u_normal_map, prevUV, 0.0).a) - currentLayerDepth + layerDepth;
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

vec3 SubsurfaceScattering(vec3 N, vec3 L, vec3 albedo, float sss, vec3 light_col, float li, vec3 tint) {
    float wrap = 0.5 * sss;
    float NdotL_wrap = max(0.0, (dot(N, L) + wrap) / (1.0 + wrap));
    float NdotL_std  = max(dot(N, L), 0.0);
    float diff = NdotL_wrap - NdotL_std;
    vec3 sss_tint = (dot(tint, tint) > 0.0) ? tint : vec3(1.0, 0.35, 0.2);
    return albedo * sss_tint * diff * light_col * li;
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 SampleIBLSpecular(vec3 N, vec3 V, float roughness, vec3 F0) {
    vec3 R = reflect(-V, N);
    float NdotV = max(dot(N, V), 0.0);
    vec3 F = FresnelSchlickRoughness(NdotV, F0, roughness);
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefiltered = textureLod(u_reflection_cubemap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf = texture(u_brdf_lut, vec2(NdotV, roughness)).rg;
    return prefiltered * (F * brdf.x + brdf.y);
}

float SampleShadowPCF(sampler2DShadow shadowMap, vec3 proj_coords, float bias) {
    float shadow = 0.0;
    vec2 texel_size = 1.0 / vec2(textureSize(shadowMap, 0));
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            shadow += textureLod(shadowMap, vec3(proj_coords.xy
                      + vec2(x, y) * texel_size, proj_coords.z - bias), 0.0);
    return shadow / 9.0;
}

// --- PCSS (Percentage Closer Soft Shadows) ---

const vec2 kPoissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216),  vec2( 0.94558609, -0.76890725),
    vec2(-0.09418410, -0.92938870),  vec2( 0.34495938,  0.29387760),
    vec2(-0.91588581,  0.45771432),  vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543,  0.27676845),  vec2( 0.97484398,  0.75648379),
    vec2( 0.44323325, -0.97511554),  vec2( 0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),  vec2( 0.79197514,  0.19090188),
    vec2(-0.24188840,  0.99706507),  vec2(-0.81409955,  0.91437590),
    vec2( 0.19984126,  0.78641367),  vec2( 0.14383161, -0.14100790)
);

const float PCSS_LIGHT_SIZE    = 0.004;
const float PCSS_SEARCH_RADIUS = 0.008;
const int   PCSS_BLOCKER_SEARCH_STEPS = 3;

// 用 sampler2DShadow 的比较结果 + 二分法近似遮挡体深度。
// texture(shadowMap, vec3(uv, ref)) 在 ref <= stored_depth 时返回 1.0，否则 0.0。
// 二分搜索 [0, receiverDepth] 中的比较翻转点即为 stored_depth（blocker depth）。
float FindBlockerDepth(sampler2DShadow shadowMap, vec2 uv, float receiverDepth,
                       float searchRadius) {
    float blockerSum = 0.0;
    int   blockerCount = 0;
    for (int i = 0; i < 16; ++i) {
        vec2 sampleUV = uv + kPoissonDisk[i] * searchRadius;
        float vis = textureLod(shadowMap, vec3(sampleUV, receiverDepth), 0.0);
        if (vis < 0.5) {
            float lo = 0.0, hi = receiverDepth;
            for (int b = 0; b < PCSS_BLOCKER_SEARCH_STEPS; ++b) {
                float mid = (lo + hi) * 0.5;
                if (textureLod(shadowMap, vec3(sampleUV, mid), 0.0) < 0.5)
                    hi = mid;
                else
                    lo = mid;
            }
            blockerSum += (lo + hi) * 0.5;
            blockerCount++;
        }
    }
    if (blockerCount == 0) return -1.0;
    return blockerSum / float(blockerCount);
}

float PCSS_Shadow(sampler2DShadow shadowMap, vec3 projCoords, float bias) {
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));
    float receiverDepth = projCoords.z - bias;

    // Step 1: Blocker search — 找遮挡体平均深度
    float avgBlockerDepth = FindBlockerDepth(shadowMap, projCoords.xy, receiverDepth,
                                              PCSS_SEARCH_RADIUS);
    if (avgBlockerDepth < 0.0) return 1.0;

    // Step 2: 半影宽度 = lightSize * (dReceiver - dBlocker) / dBlocker
    float penumbraWidth = PCSS_LIGHT_SIZE * (receiverDepth - avgBlockerDepth)
                          / max(avgBlockerDepth, 0.0001);
    float filterRadius = max(penumbraWidth, texelSize.x);

    // Step 3: 可变核 Poisson PCF
    float shadow = 0.0;
    for (int i = 0; i < 16; ++i) {
        vec2 offset = kPoissonDisk[i] * filterRadius;
        shadow += textureLod(shadowMap, vec3(projCoords.xy + offset, receiverDepth), 0.0);
    }
    return shadow / 16.0;
}

float ShadowForCascade(int idx, vec3 fragPosWorldSpace, vec3 normal, vec3 lightDir) {
    vec4 fragPosLightSpace = light_space_matrices[idx] * vec4(fragPosWorldSpace, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0) return 0.0;
    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) return 0.0;
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
    float lit;
    if (idx == 0)      lit = PCSS_Shadow(u_shadow_maps[0], projCoords, bias);
    else if (idx == 1) lit = PCSS_Shadow(u_shadow_maps[1], projCoords, bias);
    else               lit = PCSS_Shadow(u_shadow_maps[2], projCoords, bias);
    return clamp((1.0 - lit) * u_shadow_strength, 0.0, 1.0);
}

float ShadowCalculation(vec3 fragPosWorldSpace, vec3 fragPosViewSpace, vec3 normal, vec3 lightDir) {
    if (!u_receive_shadow) return 0.0;
    float viewDepth = abs(fragPosViewSpace.z);
    int cascadeIndex = CSM_CASCADES - 1;
    for (int i = 0; i < CSM_CASCADES - 1; ++i) {
        if (viewDepth < u_cascade_splits[i]) {
            cascadeIndex = i;
            break;
        }
    }
    float shadow = ShadowForCascade(cascadeIndex, fragPosWorldSpace, normal, lightDir);

    // 级联边界 smoothstep 混合：在当前级联范围末尾 20% 区域混合到下一级
    if (cascadeIndex < CSM_CASCADES - 1) {
        float splitEnd = u_cascade_splits[cascadeIndex];
        float blendStart = splitEnd * 0.8;
        if (viewDepth > blendStart) {
            float blendFactor = smoothstep(blendStart, splitEnd, viewDepth);
            float nextShadow = ShadowForCascade(cascadeIndex + 1, fragPosWorldSpace, normal, lightDir);
            shadow = mix(shadow, nextShadow, blendFactor);
        }
    }

    return clamp(shadow * u_shadow_strength, 0.0, 1.0);
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
    vec2 texelSize;
    if (shadowIndex == 0)      texelSize = 1.0 / vec2(textureSize(u_spot_shadow_maps[0], 0));
    else if (shadowIndex == 1) texelSize = 1.0 / vec2(textureSize(u_spot_shadow_maps[1], 0));
    else if (shadowIndex == 2) texelSize = 1.0 / vec2(textureSize(u_spot_shadow_maps[2], 0));
    else                       texelSize = 1.0 / vec2(textureSize(u_spot_shadow_maps[3], 0));
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth;
            if (shadowIndex == 0)      pcfDepth = textureLod(u_spot_shadow_maps[0], projCoords.xy + vec2(x, y) * texelSize, 0.0).r;
            else if (shadowIndex == 1) pcfDepth = textureLod(u_spot_shadow_maps[1], projCoords.xy + vec2(x, y) * texelSize, 0.0).r;
            else if (shadowIndex == 2) pcfDepth = textureLod(u_spot_shadow_maps[2], projCoords.xy + vec2(x, y) * texelSize, 0.0).r;
            else                       pcfDepth = textureLod(u_spot_shadow_maps[3], projCoords.xy + vec2(x, y) * texelSize, 0.0).r;
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
    float closestDepth;
    if (shadowIndex == 0)      closestDepth = textureLod(u_point_shadow_maps[0], fragToLight, 0.0).r * lightRadius;
    else if (shadowIndex == 1) closestDepth = textureLod(u_point_shadow_maps[1], fragToLight, 0.0).r * lightRadius;
    else if (shadowIndex == 2) closestDepth = textureLod(u_point_shadow_maps[2], fragToLight, 0.0).r * lightRadius;
    else                       closestDepth = textureLod(u_point_shadow_maps[3], fragToLight, 0.0).r * lightRadius;
    float bias = 0.05;
    return (currentDepth - bias) > closestDepth ? u_shadow_strength : 0.0;
}

void OutputFragment(vec3 color, float alpha) {
    if (u_wboit_mode > 0.5) {
        float z = gl_FragCoord.z;
        float w = alpha * max(1e-2, 3e3 * pow(1.0 - z, 3.0));
        if (u_wboit_mode < 1.5) {
            FragColor = vec4(color * alpha * w, alpha * w);
        } else {
            FragColor = vec4(0.0, 0.0, 0.0, alpha);
        }
        return;
    }
    FragColor = vec4(color, alpha);
}

void main() {
    vec2 finalUV = vTexCoord;
    if (u_pom_height_scale > 0.0 && u_has_normal_map) {
        vec3 viewDirTS = transpose(vTBN) * normalize(u_camera_pos - vFragPos);
        finalUV = ParallaxOcclusionMapping(vTexCoord, viewDirTS, u_pom_height_scale);
    }
    vec4 texColor;
    if (u_splat_enabled > 0.5) {
        vec4 w = texture(u_splat_weight_map, finalUV);
        float w_sum = w.r + w.g + w.b + w.a;
        if (w_sum > 0.001) w /= w_sum;
        vec3 c0 = texture(u_splat_layer0, finalUV * u_splat_tiling.x).rgb;
        vec3 c1 = texture(u_splat_layer1, finalUV * u_splat_tiling.y).rgb;
        vec3 c2 = texture(u_splat_layer2, finalUV * u_splat_tiling.z).rgb;
        vec3 c3 = texture(u_splat_layer3, finalUV * u_splat_tiling.w).rgb;
        texColor = vec4(c0 * w.r + c1 * w.g + c2 * w.b + c3 * w.a, 1.0) * vColor;
    } else {
        texColor = texture(u_texture, finalUV) * vColor;
    }
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
        OutputFragment(result, texColor.a * vColor.a);
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
        OutputFragment(color, 1.0);
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
        OutputFragment(color, texColor.a * vColor.a);
        return;
    }

    // Watercolor shading mode (light_params.w == 5.0)
    // UBO packing: toon_shadow_color.x=paper_strength, .y=edge_darkening, .z=color_bleed, .w=pigment_density
    if (light_params.w == 5.0) {
        float wc_paper    = toon_shadow_color.x;
        float wc_edge     = toon_shadow_color.y;
        float wc_bleed    = toon_shadow_color.z;
        float wc_pigment  = max(toon_shadow_color.w, 0.1);

        vec3 L = normalize(-u_light_direction);
        vec3 V_wc = normalize(u_camera_pos - vFragPos);
        float NdotL = dot(N, L) * 0.5 + 0.5;

        vec3 baseColor = texColor.rgb * vColor.rgb * u_material_albedo;

        // 1) 分段漫反射 + 柔和阴影过渡
        float soft_band = smoothstep(0.25, 0.55, NdotL);
        float shadow = ShadowCalculation(vFragPos, vFragPosViewSpace, N, L);
        vec3 lit = baseColor * u_light_color * u_light_intensity;
        vec3 shade = baseColor * vec3(0.45, 0.4, 0.5) * u_ambient_intensity;
        vec3 diffuse = mix(shade, lit, soft_band) * (1.0 - shadow * 0.6);

        // 2) 边缘加深（模拟颜料在笔触边缘的沉积）
        float fresnel = 1.0 - max(dot(N, V_wc), 0.0);
        float edge_factor = pow(fresnel, 3.0) * wc_edge;
        diffuse *= (1.0 - edge_factor * 0.5);

        // 3) 纸张颗粒纹理（程序化噪声近似）
        float paper_noise = fract(sin(dot(gl_FragCoord.xy * 0.01, vec2(12.9898, 78.233))) * 43758.5453);
        paper_noise = paper_noise * 0.5 + 0.5;
        diffuse = mix(diffuse, diffuse * paper_noise, wc_paper * 0.3);

        // 4) 色彩渗透（邻域色相偏移近似）
        vec3 warm_shift = vec3(0.03, -0.01, -0.03) * wc_bleed;
        diffuse += warm_shift * (1.0 - soft_band);

        // 5) 颜料浓度调整
        diffuse = pow(diffuse, vec3(1.0 / wc_pigment));

        OutputFragment(diffuse, texColor.a * vColor.a);
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
        OutputFragment(color_st * shadow_multiplier, texColor.a * vColor.a);
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

    // Clustered Forward+: 定位当前 fragment 所属 cluster
    int cl_tx = int(gl_FragCoord.x) / 16;  // kClusterTileSize
    int cl_ty = int(gl_FragCoord.y) / 16;
    float cl_linear_z = max(-vFragPosViewSpace.z, 0.0001);
    float cl_log_ratio = log(cluster_far / max(cluster_near, 0.0001));
    int cl_tz = (cl_log_ratio > 0.0) ? clamp(int(log(cl_linear_z / max(cluster_near, 0.0001)) / cl_log_ratio * float(cluster_z_slices)), 0, int(cluster_z_slices) - 1) : 0;
    int cl_idx = (cl_tz * int(cluster_tiles_y) + cl_ty) * int(cluster_tiles_x) + cl_tx;
    // 边界保护
    int cl_total = int(cluster_tiles_x) * int(cluster_tiles_y) * int(cluster_z_slices);
    cl_idx = clamp(cl_idx, 0, max(cl_total - 1, 0));
    uint cl_offset = cluster_infos[cl_idx].offset;
    uint cl_point_count = cluster_infos[cl_idx].point_count;
    uint cl_spot_count  = cluster_infos[cl_idx].spot_count;

    // 点光源 — 只遍历当前 cluster 分配的光源
    for(uint ci = 0u; ci < cl_point_count; ++ci) {
        int i = int(light_indices[cl_offset + ci]);
        if (i >= u_point_light_count) continue;
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

    // 聚光灯 — 只遍历当前 cluster 分配的光源
    for(uint si = 0u; si < cl_spot_count; ++si) {
        int i = int(light_indices[cl_offset + cl_point_count + si]);
        if (i >= u_spot_light_count) continue;
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

    // 环境光（SH 间接漫反射 + IBL 间接高光）
    vec3 F = fresnelSchlick(max(dot(N, V), 0.0), F0);
    vec3 kS_ambient = F;
    vec3 kD_ambient = 1.0 - kS_ambient;
    kD_ambient *= 1.0 - metallic;
    vec3 irradiance = u_sh_enabled ? EvaluateSH(N) : vec3(u_ambient_intensity);
    vec3 diffuse_ambient = kD_ambient * irradiance * surface_albedo;
    vec3 specular_ambient = u_ibl_enabled
        ? SampleIBLSpecular(N, V, roughness, F0)
        : (irradiance * F0 * (1.0 - roughness));
    vec3 ambient = (diffuse_ambient + specular_ambient) * ao;
    if (u_clear_coat > 0.0) {
        vec3 F_cc_amb = fresnelSchlick(max(dot(N, V), 0.0), vec3(0.04));
        ambient += F_cc_amb * u_clear_coat * irradiance * (1.0 - u_clear_coat_roughness) * 0.25;
    }
    vec3 color = ambient + Lo + surface_emissive;

    OutputFragment(color, texColor.a * vColor.a);
}
