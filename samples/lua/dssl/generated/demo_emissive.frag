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

// DSSL PerMaterial UBO (auto-generated)
layout(std140, set = 2, binding = 0) uniform PerMaterial {
    vec4 _mat_albedo_color;
    vec3 _mat_emission_color;
    float _mat_emission_strength;
    float _mat_metallic;
    float _mat_roughness;
};
#define albedo_color _mat_albedo_color
#define emission_color _mat_emission_color
#define emission_strength _mat_emission_strength
#define metallic _mat_metallic
#define roughness _mat_roughness

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
    float _pad;
};
#define MAX_SPOT_LIGHTS 256
layout(std430, set = 1, binding = 2) readonly buffer SpotLightSSBO {
    int u_spot_light_count;
    int _sl_pad0;
    int _sl_pad1;
    int _sl_pad2;
    SpotLight u_spot_lights[];
};

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

layout(std140, set = 1, binding = 5) uniform LightProbeData {
    vec4 sh_coefficients[9];
    vec4 probe_params;
};
#define u_sh_enabled (probe_params.x > 0.5)

const float PI = 3.14159265359;

#define u_lighting_enabled    (light_dir_and_enabled.w != 0.0)
#define u_light_direction     light_dir_and_enabled.xyz
#define u_light_color         light_color_and_ambient.xyz
#define u_light_intensity     light_params.x
#define u_ambient_intensity   light_color_and_ambient.w
#define u_shadow_strength     light_params.y
#define u_receive_shadow      (light_params.z != 0.0)
#define u_cascade_splits      cascade_splits.xyz
#define u_camera_pos          camera_pos.xyz

// DSSL 内置变量 (可读输入)
#define UV          vTexCoord
#define UV2         vTexCoord
#define COLOR       vColor
#define VERTEX      vFragPos
#define NORMAL      vNormal
#define WORLD_POSITION vFragPos
#define WORLD_NORMAL   vNormal
#define VIEW_DIR    _dssl_view_dir
#define SCREEN_UV   _dssl_screen_uv
#define TIME        light_params.w

vec4 dssl_sample(sampler2D tex, vec2 uv) { return texture(tex, uv); }
vec4 dssl_sample_lod(sampler2D tex, vec2 uv, float lod) { return textureLod(tex, uv, lod); }
vec4 dssl_sample_cube(samplerCube tex, vec3 dir) { return texture(tex, dir); }
float remap(float v, float lo, float hi, float nlo, float nhi) { return nlo + (v - lo) / (hi - lo) * (nhi - nlo); }
float fresnel(float power, vec3 normal, vec3 view_d) { return pow(1.0 - max(dot(normal, view_d), 0.0), power); }
vec3 unpack_normal(vec4 n) { return n.rgb * 2.0 - 1.0; }

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

float SampleShadowPCF(sampler2DShadow shadowMap, vec3 proj_coords, float bias) {
    float shadow = 0.0;
    vec2 texel_size = 1.0 / vec2(textureSize(shadowMap, 0));
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            shadow += texture(shadowMap, vec3(proj_coords.xy
                      + vec2(x, y) * texel_size, proj_coords.z - bias));
    return shadow / 9.0;
}

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

float FindBlockerDepth(sampler2DShadow shadowMap, vec2 uv, float receiverDepth,
                       float searchRadius) {
    float blockerSum = 0.0;
    int   blockerCount = 0;
    for (int i = 0; i < 16; ++i) {
        vec2 sampleUV = uv + kPoissonDisk[i] * searchRadius;
        float vis = texture(shadowMap, vec3(sampleUV, receiverDepth));
        if (vis < 0.5) {
            float lo = 0.0, hi = receiverDepth;
            for (int b = 0; b < PCSS_BLOCKER_SEARCH_STEPS; ++b) {
                float mid = (lo + hi) * 0.5;
                if (texture(shadowMap, vec3(sampleUV, mid)) < 0.5)
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
    float avgBlockerDepth = FindBlockerDepth(shadowMap, projCoords.xy, receiverDepth,
                                              PCSS_SEARCH_RADIUS);
    if (avgBlockerDepth < 0.0) return 1.0;
    float penumbraWidth = PCSS_LIGHT_SIZE * (receiverDepth - avgBlockerDepth)
                          / max(avgBlockerDepth, 0.0001);
    float filterRadius = max(penumbraWidth, texelSize.x);
    float shadow = 0.0;
    for (int i = 0; i < 16; ++i) {
        vec2 offset = kPoissonDisk[i] * filterRadius;
        shadow += texture(shadowMap, vec3(projCoords.xy + offset, receiverDepth));
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
    float lit = PCSS_Shadow(u_shadow_maps[idx], projCoords, bias);
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
    // DSSL surface 输出变量（默认值）
    vec3  ALBEDO = vec3(1.0);
    float ALPHA = 1.0;
    float METALLIC = 0.0;
    float ROUGHNESS = 0.5;
    float AO = 1.0;
    vec3  EMISSION = vec3(0.0);
    vec3  NORMAL_MAP = vec3(0.5, 0.5, 1.0);
    float NORMAL_MAP_STRENGTH = 1.0;
    float ALPHA_SCISSOR = 0.5;
    float RIM = 0.0;
    vec3  RIM_COLOR = vec3(1.0);
    bool  _has_normal_map = false;

    vec3 _dssl_view_dir = normalize(u_camera_pos - vFragPos);
    vec2 _dssl_screen_uv = gl_FragCoord.xy / vec2(1280.0, 720.0);

    // ===== 用户 surface() 代码 =====
    {
        ALBEDO = albedo_color.rgb;
    METALLIC = metallic;
    ROUGHNESS = roughness;
    EMISSION = emission_color * emission_strength;
    }
    // ===== 用户代码结束 =====

    vec3 N = vNormal;
    if (NORMAL_MAP != vec3(0.5, 0.5, 1.0)) {
        _has_normal_map = true;
        vec3 nm = NORMAL_MAP * 2.0 - 1.0;
        nm.xy *= NORMAL_MAP_STRENGTH;
        N = normalize(vTBN * nm);
    }

    if (!u_lighting_enabled) {
        vec3 result = ALBEDO + EMISSION;
        result = result / (result + vec3(1.0));
        result = pow(result, vec3(1.0/2.2));
        FragColor = vec4(result, ALPHA);
        return;
    }

    vec3 surface_albedo = pow(ALBEDO, vec3(2.2));
    float metallic = clamp(METALLIC, 0.0, 1.0);
    float roughness = clamp(ROUGHNESS, 0.04, 1.0);
    float ao = max(AO, 0.0);

    vec3 V = _dssl_view_dir;
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, surface_albedo, metallic);

    vec3 Lo = vec3(0.0);

    // 方向光 — 引擎默认 PBR
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

    // Clustered Forward+: 定位当前 fragment 所属 cluster
    int cl_tx = int(gl_FragCoord.x) / 16;
    int cl_ty = int(gl_FragCoord.y) / 16;
    float cl_linear_z = max(-vFragPosViewSpace.z, 0.0001);
    float cl_log_ratio = log(cluster_far / max(cluster_near, 0.0001));
    int cl_tz = (cl_log_ratio > 0.0) ? clamp(int(log(cl_linear_z / max(cluster_near, 0.0001)) / cl_log_ratio * float(cluster_z_slices)), 0, int(cluster_z_slices) - 1) : 0;
    int cl_idx = (cl_tz * int(cluster_tiles_y) + cl_ty) * int(cluster_tiles_x) + cl_tx;
    int cl_total = int(cluster_tiles_x) * int(cluster_tiles_y) * int(cluster_z_slices);
    cl_idx = clamp(cl_idx, 0, max(cl_total - 1, 0));
    uint cl_offset = cluster_infos[cl_idx].offset;
    uint cl_point_count = cluster_infos[cl_idx].point_count;
    uint cl_spot_count  = cluster_infos[cl_idx].spot_count;

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

    // 环境光
    vec3 F_a = fresnelSchlick(max(dot(N, V), 0.0), F0);
    vec3 kS_ambient = F_a;
    vec3 kD_ambient = 1.0 - kS_ambient;
    kD_ambient *= 1.0 - metallic;
    vec3 irradiance = u_sh_enabled ? EvaluateSH(N) : vec3(u_ambient_intensity);
    vec3 diffuse_ambient = irradiance * surface_albedo;
    vec3 specular_ambient = irradiance * F0 * (1.0 - roughness);
    vec3 ambient = (kD_ambient * diffuse_ambient + specular_ambient) * ao;

    // 边缘光
    float rim_factor = RIM * (1.0 - max(dot(N, V), 0.0));
    vec3 rim_contribution = RIM_COLOR * rim_factor;

    vec3 color = ambient + Lo + EMISSION + rim_contribution;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));
    FragColor = vec4(color, ALPHA);
}
