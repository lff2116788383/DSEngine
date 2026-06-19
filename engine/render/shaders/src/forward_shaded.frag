#version 450
#extension GL_ARB_separate_shader_objects : enable
// B2c-1: 自包含「高级 shading」forward 片元着色器。
// 复用 forward_pbr.vert（世界空间顶点 + vp）。在 forward_pbr.frag 基础上扩展 PerMaterial UBO，
// 支持 shading_mode 0/2/3/4/5/6（PBR / HalfLambert-Skin / HalfLambert-Static / Toon / Watercolor /
// FaceSDF）+ SSS / clearcoat / anisotropy / POM / alpha-test / double-sided。
// 单方向光（无 shadow map / 点光，留给后续步骤）。着色 math 移植自生产 pbr.frag。
// 仅依赖 PerFrame/PerScene/PerMaterial 三个 UBO + 5 纹理槽，便于 MeshRenderer 用通用原语满足全部绑定。

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
    vec4 light_dir_and_enabled;    // xyz = direction TO light (L), w = lighting_enabled
    vec4 light_color_and_ambient;  // xyz = light color, w = ambient_intensity
    vec4 light_params;             // x = light_intensity
};

layout(std140, set = 2, binding = 0) uniform PerMaterial {
    vec4 albedo;        // xyz = base color, w = metallic
    vec4 roughness_ao;  // x = roughness, y = ao, z = normal_strength, w = alpha_cutoff
    vec4 emissive;      // xyz = emissive color, w = alpha_test (0/1)
    vec4 flags;         // x = has_normal, y = has_mr, z = has_emissive, w = has_occlusion
    vec4 mode_params;   // x = shading_mode, y = double_sided, z = anisotropy, w = pom_height_scale
    vec4 sss;           // xyz = sss_tint, w = sss_strength
    vec4 clearcoat;     // x = clear_coat, y = clear_coat_roughness
    vec4 toon_shadow;   // xyz = toon_shadow_color, w = toon_shadow_threshold
    vec4 toon_params;   // x = toon_shadow_softness, y = toon_specular_size, z = toon_specular_strength, w = toon_rim_strength
    vec4 watercolor;    // x = paper_strength, y = edge_darkening, z = color_bleed, w = pigment_density
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
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return a2 / max(denom, 1e-7);
}

float DistributionGGXAniso(vec3 N, vec3 H, vec3 T, vec3 B, float roughness, float aniso) {
    float at = max(roughness * (1.0 + aniso), 0.001);
    float ab = max(roughness * (1.0 - aniso), 0.001);
    float TdotH = dot(T, H);
    float BdotH = dot(B, H);
    float NdotH = dot(N, H);
    float d = TdotH * TdotH / (at * at) + BdotH * BdotH / (ab * ab) + NdotH * NdotH;
    return 1.0 / (PI * at * ab * d * d + 0.0001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
           GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 SubsurfaceScattering(vec3 N, vec3 L, vec3 alb, float sss_s, vec3 light_col, float li, vec3 tint) {
    float wrap = 0.5 * sss_s;
    float wrapped = max(0.0, (dot(N, L) + wrap) / (1.0 + wrap));
    float diff = wrapped - max(dot(N, L), 0.0);
    vec3 sss_tint = (dot(tint, tint) > 0.0) ? tint : vec3(1.0, 0.35, 0.2);
    return alb * sss_tint * diff * light_col * li;
}

// POM：以 u_normal_map.a 通道为高度（与生产 effects.glsl 一致）。
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

void main() {
    int shading_mode = int(mode_params.x + 0.5);
    bool double_sided = mode_params.y > 0.5;
    float anisotropy = mode_params.z;
    float pom_height_scale = mode_params.w;
    bool has_normal = flags.x > 0.5;
    bool has_mr = flags.y > 0.5;
    bool has_emissive = flags.z > 0.5;
    bool has_occlusion = flags.w > 0.5;

    // 几何法线（双面：背面翻转），并构建 TBN。
    vec3 Ng = normalize(vNormal);
    if (double_sided && !gl_FrontFacing) Ng = -Ng;
    vec3 T = normalize(vTangent - dot(vTangent, Ng) * Ng);
    vec3 B = cross(Ng, T);
    mat3 TBN = mat3(T, B, Ng);

    vec3 V = normalize(camera_pos.xyz - vWorldPos);

    // POM：视差偏移 UV（须有法线/高度贴图）。
    vec2 finalUV = vTexCoord;
    if (pom_height_scale > 0.0 && has_normal) {
        vec3 viewDirTS = transpose(TBN) * V;
        finalUV = ParallaxOcclusionMapping(vTexCoord, viewDirTS, pom_height_scale);
    }

    vec4 texColor = texture(u_texture, finalUV) * vColor;
    float albedo_alpha = texColor.a;

    // alpha-test。
    if (emissive.w > 0.5 && texColor.a < clamp(roughness_ao.w, 0.0, 1.0)) discard;

    // 法线贴图。
    vec3 N = Ng;
    if (has_normal) {
        vec3 nm = texture(u_normal_map, finalUV).rgb * 2.0 - 1.0;
        nm.xy *= roughness_ao.z;
        N = normalize(TBN * nm);
    }

    vec3 light_color = light_color_and_ambient.xyz;
    float ambient = light_color_and_ambient.w;
    float light_intensity = light_params.x;
    bool lighting_enabled = light_dir_and_enabled.w > 0.5;
    vec3 L = normalize(light_dir_and_enabled.xyz);   // direction TO light

    vec3 color;
    float out_alpha = texColor.a;

    if (!lighting_enabled) {
        // Unlit。
        color = texColor.rgb * albedo.xyz;
        if (has_emissive) color += texture(u_emissive_map, finalUV).rgb * emissive.xyz;
    } else if (shading_mode == 2) {
        // Half-Lambert (KF skin)。
        vec3 R = reflect(-L, N);
        float hl = dot(N, L) * 0.5 + 0.5;
        vec3 base_color = texColor.rgb * albedo.xyz;
        vec3 diffuse_color = base_color * light_color * (hl * light_intensity + ambient * 0.5);
        float spec_b = pow(max(dot(R, V), 0.0), 100.0);
        vec3 spec_tex = has_mr ? texture(u_metallic_roughness_map, finalUV).rgb : vec3(0.0);
        color = diffuse_color + spec_tex * spec_b;
        out_alpha = 1.0;
    } else if (shading_mode == 3) {
        // Half-Lambert STATIC (KF default)。
        vec3 R = reflect(-L, N);
        float hl = dot(N, L) * 0.5 + 0.5;
        vec3 diffuse = albedo.xyz * hl * light_color * light_intensity;
        float spec_power = max(roughness_ao.x, 1.0);
        vec3 spec_color = vec3(albedo.w);
        vec3 specular = spec_color * pow(max(dot(R, V), 0.0), spec_power);
        vec3 material_color = diffuse + specular + emissive.xyz;
        color = material_color * texColor.rgb;
    } else if (shading_mode == 4) {
        // Toon / Cel。
        vec3 H = normalize(L + V);
        float NdotL = dot(N, L) * 0.5 + 0.5;
        float soft = toon_params.x;
        float band1 = smoothstep(toon_shadow.w - soft, toon_shadow.w + soft, NdotL);
        float band2 = smoothstep(0.7 - soft, 0.7 + soft, NdotL);
        float cel = band1 * 0.7 + band2 * 0.3;
        vec3 baseColor = texColor.rgb * albedo.xyz;
        vec3 shadowColor = baseColor * toon_shadow.xyz;
        vec3 diffuse = mix(shadowColor, baseColor * light_color, cel);
        float spec = step(toon_params.y, max(dot(N, H), 0.0)) * toon_params.z;
        vec3 specular = light_color * spec;
        float rim = pow(1.0 - max(dot(N, V), 0.0), 4.0) * toon_params.w;
        color = diffuse + specular + vec3(rim);
    } else if (shading_mode == 5) {
        // Watercolor。
        float wc_paper = watercolor.x;
        float wc_edge = watercolor.y;
        float wc_bleed = watercolor.z;
        float wc_pigment = max(watercolor.w, 0.1);
        float NdotL = dot(N, L) * 0.5 + 0.5;
        vec3 baseColor = texColor.rgb * albedo.xyz;
        float soft_band = smoothstep(0.25, 0.55, NdotL);
        vec3 lit = baseColor * light_color * light_intensity;
        vec3 shade = baseColor * vec3(0.45, 0.4, 0.5) * ambient;
        vec3 diffuse = mix(shade, lit, soft_band);
        float fresnel = 1.0 - max(dot(N, V), 0.0);
        float edge_factor = pow(fresnel, 3.0) * wc_edge;
        diffuse *= (1.0 - edge_factor * 0.5);
        float paper_noise = fract(sin(dot(gl_FragCoord.xy * 0.01, vec2(12.9898, 78.233))) * 43758.5453);
        paper_noise = paper_noise * 0.5 + 0.5;
        diffuse = mix(diffuse, diffuse * paper_noise, wc_paper * 0.3);
        diffuse += vec3(0.03, -0.01, -0.03) * wc_bleed * (1.0 - soft_band);
        diffuse = pow(max(diffuse, vec3(0.0)), vec3(1.0 / wc_pigment));
        color = diffuse;
    } else if (shading_mode == 6) {
        // Face SDF。SDF 灰度图存于 albedo 槽（u_texture），按光方向投影到面切线平面采样。
        float light_dot_right = dot(L, normalize(TBN[0]));
        float sdf_u = light_dot_right * 0.5 + 0.5;
        float sdf_v = vTexCoord.y;
        float sdf_value = texture(u_texture, vec2(sdf_u, sdf_v)).r;
        float softness = toon_params.x > 0.0 ? toon_params.x : 0.05;
        float face_lit = smoothstep(0.5 - softness, 0.5 + softness, sdf_value);
        vec3 baseColor = albedo.xyz * vColor.rgb;
        vec3 shadowColor = baseColor * toon_shadow.xyz;
        color = mix(shadowColor, baseColor * light_color * light_intensity, face_lit);
        float rim = pow(1.0 - max(dot(N, V), 0.0), 4.0) * toon_params.w;
        color += light_color * rim;
    } else {
        // 默认 PBR（Cook-Torrance）+ SSS / clearcoat / anisotropy。
        vec3 surface_albedo = pow(texColor.rgb * albedo.xyz, vec3(2.2));
        float metallic = clamp(albedo.w, 0.0, 1.0);
        float roughness = clamp(roughness_ao.x, 0.04, 1.0);
        float ao = max(roughness_ao.y, 0.0);
        vec3 surface_emissive = emissive.xyz;
        if (has_mr) {
            vec4 mr = texture(u_metallic_roughness_map, finalUV);
            roughness = clamp(mr.g * roughness_ao.x, 0.04, 1.0);
            metallic = clamp(mr.b * albedo.w, 0.0, 1.0);
        }
        if (has_occlusion) ao *= texture(u_occlusion_map, finalUV).r;
        if (has_emissive) surface_emissive *= texture(u_emissive_map, finalUV).rgb;

        vec3 F0 = mix(vec3(0.04), surface_albedo, metallic);
        vec3 H = normalize(V + L);
        float NDF = (anisotropy != 0.0)
            ? DistributionGGXAniso(N, H, T, B, roughness, anisotropy)
            : DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 specular = (NDF * G * F) /
                        (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
        float NdotL = max(dot(N, L), 0.0);
        vec3 Lo = (kD * surface_albedo / PI + specular) * light_color * light_intensity * NdotL;

        if (sss.w > 0.0)
            Lo += SubsurfaceScattering(N, L, surface_albedo, sss.w, light_color, light_intensity, sss.xyz);

        if (clearcoat.x > 0.0) {
            float cc_r = max(clearcoat.y, 0.04);
            float NDF_cc = DistributionGGX(N, H, cc_r);
            float G_cc = GeometrySmith(N, V, L, cc_r);
            vec3 F_cc = FresnelSchlick(max(dot(H, V), 0.0), vec3(0.04));
            vec3 spec_cc = (NDF_cc * G_cc * F_cc) /
                           (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
            Lo += spec_cc * clearcoat.x * NdotL * light_color * light_intensity;
        }

        vec3 ambient_term = vec3(ambient) * surface_albedo * ao;
        color = ambient_term + Lo + surface_emissive;
    }

    // Reinhard tonemap + gamma（与 forward_pbr.frag 一致，保证软渲下三后端 LDR 输出一致）。
    color = color / (color + vec3(1.0));
    color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));
    FragColor = vec4(color, out_alpha);
}
