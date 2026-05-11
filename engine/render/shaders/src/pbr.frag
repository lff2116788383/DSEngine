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
#define MAX_POINT_LIGHTS 4
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
#define MAX_SPOT_LIGHTS 4
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
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
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
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
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
