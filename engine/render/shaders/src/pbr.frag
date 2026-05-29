#version 450

#extension GL_ARB_separate_shader_objects : enable

#extension GL_GOOGLE_include_directive : enable

// @VARIANTS: GPU_DRIVEN



layout(location = 0) in vec4 vColor;

layout(location = 1) in vec2 vTexCoord;

layout(location = 2) in vec3 vFragPos;

layout(location = 3) in vec3 vNormal;

layout(location = 4) in mat3 vTBN;

layout(location = 7) in vec3 vFragPosViewSpace;

#ifdef GPU_DRIVEN

layout(location = 8) flat in uint v_material_id;

#endif



layout(location = 0) out vec4 FragColor;



#include "includes/uniforms.glsl"

#include "includes/brdf.glsl"

#include "includes/lighting_utils.glsl"

#include "includes/effects.glsl"

#include "includes/shadow.glsl"

#include "includes/output.glsl"



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

        vec3 base_color = texColor.rgb * u_material_albedo;

        vec3 diffuse_color = base_color * u_light_color *

            (half_lambert * u_light_intensity + u_ambient_intensity * 0.5);

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

        float wc_paper    = _toon_shadow_color_vec4.x;

        float wc_edge     = _toon_shadow_color_vec4.y;

        float wc_bleed    = _toon_shadow_color_vec4.z;

        float wc_pigment  = max(_toon_shadow_color_vec4.w, 0.1);



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



    // Face SDF shading mode (light_params.w == 6.0)

    // Uses a precomputed SDF texture to create artist-controllable face shadow boundaries.

    // The SDF is sampled using the light direction projected onto the face tangent plane.

    if (light_params.w == 6.0) {

        vec3 L = normalize(-u_light_direction);

        vec3 V_face = normalize(u_camera_pos - vFragPos);



        // Face tangent plane: TBN[0] = right, TBN[1] = up (from vertex data)

        vec3 face_right = normalize(vTBN[0]);

        vec3 face_up = normalize(vTBN[1]);



        // Project light direction onto face plane to get SDF sample coordinates

        float light_dot_right = dot(L, face_right);

        float light_dot_up = dot(L, face_up);



        // SDF UV: remap light_dot_right from [-1,1] to [0,1]

        // Flip X based on which side of the face the light is on

        float sdf_u = light_dot_right * 0.5 + 0.5;

        float sdf_v = vTexCoord.y;  // Use original V for vertical position



        // Sample the face SDF map (grayscale: 0=shadow, 1=lit)

        float sdf_value = texture(u_texture, vec2(sdf_u, sdf_v)).r;



        // Smooth threshold based on light angle

        float threshold = 0.5;

        float softness = u_toon_shadow_softness > 0.0 ? u_toon_shadow_softness : 0.05;

        float face_lit = smoothstep(threshold - softness, threshold + softness, sdf_value);



        // Apply directional shadow

        float shadow = ShadowCalculation(vFragPos, vFragPosViewSpace, N, L);



        vec3 baseColor = texColor.rgb * vColor.rgb * u_material_albedo;

        vec3 shadowColor = baseColor * vec3(u_toon_shadow_color);

        vec3 color = mix(shadowColor, baseColor * u_light_color * u_light_intensity, face_lit);

        color *= (1.0 - shadow * 0.5);



        // Rim light

        float rim = pow(1.0 - max(dot(N, V_face), 0.0), 4.0) * u_toon_rim_strength;

        color += u_light_color * rim;



        OutputFragment(color, texColor.a * vColor.a);

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



    // ── Snow cover blending ──

    if (u_snow_coverage > 0.001) {

        float snow_dot = max(N.y, 0.0);

        float snow_mask = pow(

            smoothstep(u_snow_normal_threshold, 1.0, snow_dot),

            u_snow_edge_sharpness);

        float snow_factor = snow_mask * u_snow_coverage;

        vec3 snow_albedo_linear = pow(u_snow_params.xyz, vec3(2.2));

        surface_albedo = mix(surface_albedo, snow_albedo_linear, snow_factor);

        roughness = mix(roughness, u_snow_params.w, snow_factor);

        metallic = mix(metallic, 0.0, snow_factor);

    }



    // ── Wet surface: darkens albedo, reduces roughness ──

    float wetness = camera_pos.w;  // global_wetness packed in camera_pos.w

    if (wetness > 0.001) {

        float wet_factor = clamp(wetness * max(N.y, 0.0), 0.0, 1.0);

        surface_albedo *= mix(1.0, 0.6, wet_factor);

        roughness = mix(roughness, roughness * 0.3, wet_factor);

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



    // SSS mask: encode sss_strength into alpha for screen-space SSS post-process

    float alpha_out = (u_sss_strength > 0.0) ? u_sss_strength : texColor.a * vColor.a;

    OutputFragment(color, alpha_out);

}
