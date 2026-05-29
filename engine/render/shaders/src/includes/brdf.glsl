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



float DistributionGGXAniso(vec3 N, vec3 H, vec3 T, vec3 B, float roughness, float aniso) {

    float at = max(roughness * (1.0 + aniso), 0.001);

    float ab = max(roughness * (1.0 - aniso), 0.001);

    float TdotH = dot(T, H);

    float BdotH = dot(B, H);

    float NdotH = dot(N, H);

    float d = TdotH*TdotH/(at*at) + BdotH*BdotH/(ab*ab) + NdotH*NdotH;

    return 1.0 / (PI * at * ab * d * d + 0.0001);

}
