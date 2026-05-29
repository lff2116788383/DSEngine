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
