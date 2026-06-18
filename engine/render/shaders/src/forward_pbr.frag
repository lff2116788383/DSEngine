#version 450
#extension GL_ARB_separate_shader_objects : enable
// B2b-1: 自包含静态 forward PBR 片元着色器。
// 真实 Cook-Torrance BRDF（GGX 分布 + Smith 几何 + Schlick 菲涅尔），
// 单方向光 + 环境项，5 个材质纹理槽（albedo/normal/metallic-roughness/emissive/occlusion）。
// 仅依赖 PerFrame/PerScene/PerMaterial 三个 UBO，便于 MeshRenderer 用通用原语满足全部绑定。

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vTexCoord;
layout(location = 2) in vec3 vWorldPos;
layout(location = 3) in vec3 vNormal;
layout(location = 4) in vec3 vTangent;

layout(location = 0) out vec4 FragColor;

layout(std140, set = 0, binding = 0) uniform PerFrame {
    mat4 vp;
    mat4 view;
    vec4 camera_pos;     // xyz = world camera position
    vec4 foliage_wind;
    vec4 foliage_push;
};

layout(std140, set = 1, binding = 0) uniform PerScene {
    vec4 light_dir_and_enabled;    // xyz = direction TO light, w = lighting_enabled
    vec4 light_color_and_ambient;  // xyz = light color, w = ambient_intensity
    vec4 light_params;             // x = light_intensity
};

layout(std140, set = 2, binding = 0) uniform PerMaterial {
    vec4 albedo;        // xyz = base color, w = metallic
    vec4 roughness_ao;  // x = roughness, y = ao, z = normal_strength, w = alpha_cutoff
    vec4 emissive;      // xyz = emissive color, w = alpha_test (0/1)
    vec4 flags;         // x = has_normal, y = has_mr, z = has_emissive, w = has_occlusion
};

layout(set = 2, binding = 1) uniform sampler2D u_texture;                  // albedo  -> flat unit 0
layout(set = 2, binding = 2) uniform sampler2D u_normal_map;               // normal  -> flat unit 1
layout(set = 2, binding = 3) uniform sampler2D u_metallic_roughness_map;   // MR      -> flat unit 2
layout(set = 2, binding = 4) uniform sampler2D u_emissive_map;             // emissive-> flat unit 3
layout(set = 2, binding = 5) uniform sampler2D u_occlusion_map;            // AO      -> flat unit 4

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 1e-7);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 ApplyNormalMap(vec3 N, vec3 T, vec2 uv, float strength) {
    vec3 tn = texture(u_normal_map, uv).rgb * 2.0 - 1.0;
    tn.xy *= strength;
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * tn);
}

void main() {
    vec4 base_tex = texture(u_texture, vTexCoord);
    vec3 albedo_rgb = base_tex.rgb * albedo.xyz * vColor.rgb;
    float alpha = base_tex.a * vColor.a;

    if (emissive.w > 0.5 && alpha < roughness_ao.w) {
        discard;
    }

    float metallic = albedo.w;
    float roughness = clamp(roughness_ao.x, 0.04, 1.0);
    float ao = roughness_ao.y;
    if (flags.y > 0.5) {
        vec3 mr = texture(u_metallic_roughness_map, vTexCoord).rgb;
        roughness = clamp(mr.g * roughness_ao.x, 0.04, 1.0);
        metallic = mr.b * albedo.w;
    }
    if (flags.w > 0.5) {
        ao *= texture(u_occlusion_map, vTexCoord).r;
    }

    vec3 N = normalize(vNormal);
    if (flags.x > 0.5) {
        N = ApplyNormalMap(N, normalize(vTangent), vTexCoord, roughness_ao.z);
    }

    vec3 V = normalize(camera_pos.xyz - vWorldPos);
    vec3 F0 = mix(vec3(0.04), albedo_rgb, metallic);

    vec3 Lo = vec3(0.0);
    if (light_dir_and_enabled.w > 0.5) {
        vec3 L = normalize(light_dir_and_enabled.xyz);
        vec3 H = normalize(V + L);
        vec3 radiance = light_color_and_ambient.xyz * light_params.x;

        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denom = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 1e-4;
        vec3 specular = numerator / denom;

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
        float NdotL = max(dot(N, L), 0.0);
        Lo = (kD * albedo_rgb / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = light_color_and_ambient.www * albedo_rgb * ao;
    vec3 color = ambient + Lo;

    vec3 emissive_rgb = emissive.xyz;
    if (flags.z > 0.5) {
        emissive_rgb *= texture(u_emissive_map, vTexCoord).rgb;
    }
    color += emissive_rgb;

    // Reinhard tonemap + gamma，保证软渲下三后端一致的 LDR 输出。
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, alpha);
}
