#version 430

struct ClusterInfoEntry
{
    uint offset;
    uint point_count;
    uint spot_count;
    uint _pad;
};

struct PointLight
{
    vec3 color;
    float intensity;
    vec3 position;
    float radius;
    int cast_shadow;
    int shadow_index;
    vec2 _pad;
};

struct SpotLight
{
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

layout(binding = 0, std140) uniform PerScene
{
    vec4 light_dir_and_enabled;
    vec4 light_color_and_ambient;
    vec4 light_params;
    vec4 cascade_splits;
    mat4 light_space_matrices[3];
} _245;

layout(binding = 10, std140) uniform SpotLightData
{
    mat4 u_spot_light_space_matrices[4];
} _438;

layout(binding = 0, std140) uniform PerMaterial
{
    vec4 albedo;
    vec4 roughness_ao;
    vec4 emissive;
    vec4 flags;
} _640;

layout(binding = 0, std140) uniform PerFrame
{
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
} _769;

layout(binding = 3, std430) readonly buffer ClusterInfoSSBO
{
    uint cluster_tiles_x;
    uint cluster_tiles_y;
    uint cluster_z_slices;
    float cluster_near;
    float cluster_far;
    uint _ci_pad0;
    uint _ci_pad1;
    uint _ci_pad2;
    ClusterInfoEntry cluster_infos[];
} _1178;

layout(binding = 4, std430) readonly buffer LightIndexSSBO
{
    uint light_indices[];
} _1271;

layout(binding = 1, std430) readonly buffer PointLightSSBO
{
    int u_point_light_count;
    int _pl_pad0;
    int _pl_pad1;
    int _pl_pad2;
    PointLight u_point_lights[];
} _1283;

layout(binding = 2, std430) readonly buffer SpotLightSSBO
{
    int u_spot_light_count;
    int _sl_pad0;
    int _sl_pad1;
    int _sl_pad2;
    SpotLight u_spot_lights[];
} _1466;

layout(binding = 6) uniform sampler2DShadow u_shadow_maps[3];
layout(binding = 7) uniform sampler2D u_spot_shadow_maps[4];
layout(binding = 0) uniform samplerCube u_point_shadow_maps[4];
layout(binding = 1) uniform sampler2D u_texture;
layout(binding = 2) uniform sampler2D u_normal_map;
layout(binding = 4) uniform sampler2D u_emissive_map;
layout(binding = 3) uniform sampler2D u_metallic_roughness_map;
layout(binding = 5) uniform sampler2D u_occlusion_map;

layout(location = 1) in vec2 vTexCoord;
layout(location = 3) in vec3 vNormal;
layout(location = 4) in mat3 vTBN;
layout(location = 0) in vec4 vColor;
layout(location = 0) out vec4 FragColor;
layout(location = 2) in vec3 vFragPos;
layout(location = 7) in vec3 vFragPosViewSpace;

float SampleShadowPCF(sampler2DShadow shadowMap, vec3 proj_coords, float bias)
{
    float shadow = 0.0;
    vec2 texel_size = vec2(1.0) / vec2(textureSize(shadowMap, 0));
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            vec3 _223 = vec3(proj_coords.xy + (vec2(float(x), float(y)) * texel_size), proj_coords.z - bias);
            shadow += texture(shadowMap, vec3(_223.xy, _223.z));
        }
    }
    return shadow / 9.0;
}

float ShadowForCascade(int idx, vec3 fragPosWorldSpace, vec3 normal, vec3 lightDir)
{
    vec4 fragPosLightSpace = _245.light_space_matrices[idx] * vec4(fragPosWorldSpace, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / vec3(fragPosLightSpace.w);
    vec3 _264 = projCoords;
    vec2 _269 = (_264.xy * 0.5) + vec2(0.5);
    projCoords.x = _269.x;
    projCoords.y = _269.y;
    if (projCoords.z > 1.0)
    {
        return 0.0;
    }
    bool _284 = projCoords.x < 0.0;
    bool _291;
    if (!_284)
    {
        _291 = projCoords.x > 1.0;
    }
    else
    {
        _291 = _284;
    }
    bool _298;
    if (!_291)
    {
        _298 = projCoords.y < 0.0;
    }
    else
    {
        _298 = _291;
    }
    bool _305;
    if (!_298)
    {
        _305 = projCoords.y > 1.0;
    }
    else
    {
        _305 = _298;
    }
    if (_305)
    {
        return 0.0;
    }
    float bias = max(0.004999999888241291046142578125 * (1.0 - dot(normal, lightDir)), 0.0005000000237487256526947021484375);
    vec3 param = projCoords;
    float param_1 = bias;
    float lit = SampleShadowPCF(u_shadow_maps[idx], param, param_1);
    return 1.0 - lit;
}

float ShadowCalculation(vec3 fragPosWorldSpace, vec3 fragPosViewSpace, vec3 normal, vec3 lightDir)
{
    if (!(_245.light_params.z != 0.0))
    {
        return 0.0;
    }
    float viewDepth = abs(fragPosViewSpace.z);
    int cascadeIndex = 2;
    for (int i = 0; i < 2; i++)
    {
        if (viewDepth < _245.cascade_splits[uvec3(0u, 1u, 2u)[i]])
        {
            cascadeIndex = i;
            break;
        }
    }
    int param = cascadeIndex;
    vec3 param_1 = fragPosWorldSpace;
    vec3 param_2 = normal;
    vec3 param_3 = lightDir;
    float shadow = ShadowForCascade(param, param_1, param_2, param_3);
    if (cascadeIndex < 2)
    {
        float splitEnd = _245.cascade_splits[uvec3(0u, 1u, 2u)[cascadeIndex]];
        float blendStart = splitEnd * 0.800000011920928955078125;
        if (viewDepth > blendStart)
        {
            float blendFactor = smoothstep(blendStart, splitEnd, viewDepth);
            int param_4 = cascadeIndex + 1;
            vec3 param_5 = fragPosWorldSpace;
            vec3 param_6 = normal;
            vec3 param_7 = lightDir;
            float nextShadow = ShadowForCascade(param_4, param_5, param_6, param_7);
            shadow = mix(shadow, nextShadow, blendFactor);
        }
    }
    return clamp(shadow * _245.light_params.y, 0.0, 1.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0)) + 1.0;
    denom = (3.1415927410125732421875 * denom) * denom;
    return nom / max(denom, 1.0000000116860974230803549289703e-07);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float nom = NdotV;
    float denom = (NdotV * (1.0 - k)) + k;
    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float param = NdotV;
    float param_1 = roughness;
    float ggx2 = GeometrySchlickGGX(param, param_1);
    float param_2 = NdotL;
    float param_3 = roughness;
    float ggx1 = GeometrySchlickGGX(param_2, param_3);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + ((vec3(1.0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0));
}

float PointShadowCalculation(int shadowIndex, vec3 fragPosWorldSpace, vec3 lightPos, float lightRadius)
{
    if ((shadowIndex < 0) || (shadowIndex >= 4))
    {
        return 0.0;
    }
    vec3 fragToLight = fragPosWorldSpace - lightPos;
    float currentDepth = length(fragToLight);
    if (currentDepth >= lightRadius)
    {
        return 0.0;
    }
    float closestDepth = texture(u_point_shadow_maps[shadowIndex], fragToLight).x * lightRadius;
    float bias = 0.0500000007450580596923828125;
    float _622;
    if ((currentDepth - bias) > closestDepth)
    {
        _622 = _245.light_params.y;
    }
    else
    {
        _622 = 0.0;
    }
    return _622;
}

float SpotShadowCalculation(int shadowIndex, vec3 fragPosWorldSpace, vec3 normal, vec3 lightDir)
{
    if ((shadowIndex < 0) || (shadowIndex >= 4))
    {
        return 0.0;
    }
    vec4 fragPosLightSpace = _438.u_spot_light_space_matrices[shadowIndex] * vec4(fragPosWorldSpace, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / vec3(max(fragPosLightSpace.w, 9.9999997473787516355514526367188e-05));
    vec3 _457 = projCoords;
    vec2 _461 = (_457.xy * 0.5) + vec2(0.5);
    projCoords.x = _461.x;
    projCoords.y = _461.y;
    if (projCoords.z > 1.0)
    {
        return 0.0;
    }
    bool _474 = projCoords.x < 0.0;
    bool _481;
    if (!_474)
    {
        _481 = projCoords.x > 1.0;
    }
    else
    {
        _481 = _474;
    }
    bool _488;
    if (!_481)
    {
        _488 = projCoords.y < 0.0;
    }
    else
    {
        _488 = _481;
    }
    bool _495;
    if (!_488)
    {
        _495 = projCoords.y > 1.0;
    }
    else
    {
        _495 = _488;
    }
    if (_495)
    {
        return 0.0;
    }
    float currentDepth = projCoords.z;
    float bias = max(0.0030000000260770320892333984375 * (1.0 - dot(normal, lightDir)), 0.0005000000237487256526947021484375);
    float shadow = 0.0;
    vec2 texelSize = vec2(1.0) / vec2(textureSize(u_spot_shadow_maps[shadowIndex], 0));
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float pcfDepth = texture(u_spot_shadow_maps[shadowIndex], projCoords.xy + (vec2(float(x), float(y)) * texelSize)).x;
            shadow += float((currentDepth - bias) > pcfDepth);
        }
    }
    shadow /= 9.0;
    return clamp(shadow * _245.light_params.y, 0.0, 1.0);
}

void main()
{
    vec4 texColor = texture(u_texture, vTexCoord);
    bool _643 = _640.emissive.w != 0.0;
    bool _652;
    if (_643)
    {
        _652 = texColor.w < clamp(_640.roughness_ao.w, 0.0, 1.0);
    }
    else
    {
        _652 = _643;
    }
    if (_652)
    {
        discard;
    }
    vec3 N = vNormal;
    if (_640.flags.x != 0.0)
    {
        vec3 normalMap = texture(u_normal_map, vTexCoord).xyz;
        normalMap = (normalMap * 2.0) - vec3(1.0);
        vec3 _678 = normalMap;
        vec2 _680 = _678.xy * _640.roughness_ao.z;
        normalMap.x = _680.x;
        normalMap.y = _680.y;
        N = normalize(vTBN * normalMap);
    }
    if (!(_245.light_dir_and_enabled.w != 0.0))
    {
        vec3 result = (texColor.xyz * vColor.xyz) * _640.albedo.xyz;
        if (_640.flags.z != 0.0)
        {
            result += (texture(u_emissive_map, vTexCoord).xyz * _640.emissive.xyz);
        }
        if (_245.light_params.w == 0.0)
        {
            result /= (result + vec3(1.0));
            result = pow(result, vec3(0.4545454680919647216796875));
        }
        FragColor = vec4(result, texColor.w * vColor.w);
        return;
    }
    if (_245.light_params.w == 2.0)
    {
        vec3 L = normalize(-_245.light_dir_and_enabled.xyz);
        vec3 V_hl = normalize(_769.camera_pos.xyz - vFragPos);
        vec3 R = reflect(_245.light_dir_and_enabled.xyz, N);
        float half_lambert = (dot(N, L) * 0.5) + 0.5;
        vec3 diffuse_color = ((texColor.xyz * vColor.xyz) * _640.albedo.xyz) * half_lambert;
        float spec_brightness = pow(max(dot(R, V_hl), 0.0), 100.0);
        vec3 _812;
        if (_640.flags.y != 0.0)
        {
            _812 = texture(u_metallic_roughness_map, vTexCoord).xyz;
        }
        else
        {
            _812 = vec3(0.0);
        }
        vec3 spec_tex = _812;
        vec3 specular_color = spec_tex * spec_brightness;
        vec3 param = vFragPos;
        vec3 param_1 = vFragPosViewSpace;
        vec3 param_2 = N;
        vec3 param_3 = L;
        float shadow = ShadowCalculation(param, param_1, param_2, param_3);
        float shadow_multiplier = 1.0 - (shadow * 0.5);
        vec3 color = (diffuse_color + specular_color) * shadow_multiplier;
        FragColor = vec4(color, 1.0);
        return;
    }
    if (_245.light_params.w == 3.0)
    {
        vec3 L_1 = normalize(-_245.light_dir_and_enabled.xyz);
        vec3 V_st = normalize(_769.camera_pos.xyz - vFragPos);
        vec3 R_1 = reflect(_245.light_dir_and_enabled.xyz, N);
        float half_lambert_1 = (dot(N, L_1) * 0.5) + 0.5;
        vec3 diffuse = ((_640.albedo.xyz * half_lambert_1) * _245.light_color_and_ambient.xyz) * _245.light_params.x;
        float spec_power = max(_640.roughness_ao.x, 1.0);
        vec3 spec_color = vec3(_640.albedo.w);
        vec3 specular = spec_color * pow(max(dot(R_1, V_st), 0.0), spec_power);
        vec3 emissive_val = _640.emissive.xyz;
        vec3 material_color = (diffuse + specular) + emissive_val;
        vec3 color_st = (material_color * texColor.xyz) * vColor.xyz;
        vec3 param_4 = vFragPos;
        vec3 param_5 = vFragPosViewSpace;
        vec3 param_6 = N;
        vec3 param_7 = L_1;
        float shadow_1 = ShadowCalculation(param_4, param_5, param_6, param_7);
        float shadow_multiplier_1 = 1.0 - (shadow_1 * 0.5);
        FragColor = vec4(color_st * shadow_multiplier_1, texColor.w * vColor.w);
        return;
    }
    vec3 surface_albedo = pow((texColor.xyz * vColor.xyz) * _640.albedo.xyz, vec3(2.2000000476837158203125));
    float metallic = clamp(_640.albedo.w, 0.0, 1.0);
    float roughness = clamp(_640.roughness_ao.x, 0.039999999105930328369140625, 1.0);
    float ao = max(_640.roughness_ao.y, 0.0);
    vec3 surface_emissive = _640.emissive.xyz;
    if (_640.flags.y != 0.0)
    {
        vec4 mrSample = texture(u_metallic_roughness_map, vTexCoord);
        roughness = clamp(mrSample.y * _640.roughness_ao.x, 0.039999999105930328369140625, 1.0);
        metallic = clamp(mrSample.z * _640.albedo.w, 0.0, 1.0);
    }
    if (_640.flags.w != 0.0)
    {
        ao *= texture(u_occlusion_map, vTexCoord).x;
    }
    if (_640.flags.z != 0.0)
    {
        surface_emissive *= texture(u_emissive_map, vTexCoord).xyz;
    }
    vec3 V = normalize(_769.camera_pos.xyz - vFragPos);
    vec3 F0 = vec3(0.039999999105930328369140625);
    F0 = mix(F0, surface_albedo, vec3(metallic));
    vec3 Lo = vec3(0.0);
    vec3 L_2 = normalize(-_245.light_dir_and_enabled.xyz);
    vec3 H = normalize(V + L_2);
    vec3 param_8 = N;
    vec3 param_9 = H;
    float param_10 = roughness;
    float NDF = DistributionGGX(param_8, param_9, param_10);
    vec3 param_11 = N;
    vec3 param_12 = V;
    vec3 param_13 = L_2;
    float param_14 = roughness;
    float G = GeometrySmith(param_11, param_12, param_13, param_14);
    float param_15 = max(dot(H, V), 0.0);
    vec3 param_16 = F0;
    vec3 F = fresnelSchlick(param_15, param_16);
    vec3 numerator = F * (NDF * G);
    float denominator = ((4.0 * max(dot(N, V), 0.0)) * max(dot(N, L_2), 0.0)) + 9.9999997473787516355514526367188e-05;
    vec3 specular_1 = numerator / vec3(denominator);
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= (1.0 - metallic);
    float NdotL = max(dot(N, L_2), 0.0);
    vec3 param_17 = vFragPos;
    vec3 param_18 = vFragPosViewSpace;
    vec3 param_19 = N;
    vec3 param_20 = L_2;
    float shadow_2 = ShadowCalculation(param_17, param_18, param_19, param_20);
    Lo += (((((((kD * surface_albedo) / vec3(3.1415927410125732421875)) + specular_1) * _245.light_color_and_ambient.xyz) * _245.light_params.x) * NdotL) * (1.0 - shadow_2));
    int cl_tx = int(gl_FragCoord.x) / 16;
    int cl_ty = int(gl_FragCoord.y) / 16;
    float cl_linear_z = max(-vFragPosViewSpace.z, 9.9999997473787516355514526367188e-05);
    float cl_log_ratio = log(_1178.cluster_far / max(_1178.cluster_near, 9.9999997473787516355514526367188e-05));
    int _1189;
    if (cl_log_ratio > 0.0)
    {
        _1189 = clamp(int((log(cl_linear_z / max(_1178.cluster_near, 9.9999997473787516355514526367188e-05)) / cl_log_ratio) * float(_1178.cluster_z_slices)), 0, int(_1178.cluster_z_slices) - 1);
    }
    else
    {
        _1189 = 0;
    }
    int cl_tz = _1189;
    int cl_idx = (((cl_tz * int(_1178.cluster_tiles_y)) + cl_ty) * int(_1178.cluster_tiles_x)) + cl_tx;
    int cl_total = (int(_1178.cluster_tiles_x) * int(_1178.cluster_tiles_y)) * int(_1178.cluster_z_slices);
    cl_idx = clamp(cl_idx, 0, max((cl_total - 1), 0));
    uint cl_offset = _1178.cluster_infos[cl_idx].offset;
    uint cl_point_count = _1178.cluster_infos[cl_idx].point_count;
    uint cl_spot_count = _1178.cluster_infos[cl_idx].spot_count;
    for (uint ci = 0u; ci < cl_point_count; ci++)
    {
        int i = int(_1271.light_indices[cl_offset + ci]);
        if (i >= _1283.u_point_light_count)
        {
            continue;
        }
        vec3 L_3 = normalize(_1283.u_point_lights[i].position - vFragPos);
        vec3 H_1 = normalize(V + L_3);
        float _distance = length(_1283.u_point_lights[i].position - vFragPos);
        float attenuation = clamp(1.0 - ((_distance * _distance) / (_1283.u_point_lights[i].radius * _1283.u_point_lights[i].radius)), 0.0, 1.0);
        attenuation *= attenuation;
        vec3 radiance = (_1283.u_point_lights[i].color * _1283.u_point_lights[i].intensity) * attenuation;
        vec3 param_21 = N;
        vec3 param_22 = H_1;
        float param_23 = roughness;
        float NDF_1 = DistributionGGX(param_21, param_22, param_23);
        vec3 param_24 = N;
        vec3 param_25 = V;
        vec3 param_26 = L_3;
        float param_27 = roughness;
        float G_1 = GeometrySmith(param_24, param_25, param_26, param_27);
        float param_28 = max(dot(H_1, V), 0.0);
        vec3 param_29 = F0;
        vec3 F_1 = fresnelSchlick(param_28, param_29);
        vec3 numerator_1 = F_1 * (NDF_1 * G_1);
        float denominator_1 = ((4.0 * max(dot(N, V), 0.0)) * max(dot(N, L_3), 0.0)) + 9.9999997473787516355514526367188e-05;
        vec3 specular_2 = numerator_1 / vec3(denominator_1);
        vec3 kS_1 = F_1;
        vec3 kD_1 = vec3(1.0) - kS_1;
        kD_1 *= (1.0 - metallic);
        float NdotL_1 = max(dot(N, L_3), 0.0);
        float point_shadow = 0.0;
        if (_1283.u_point_lights[i].cast_shadow != 0)
        {
            int param_30 = _1283.u_point_lights[i].shadow_index;
            vec3 param_31 = vFragPos;
            vec3 param_32 = _1283.u_point_lights[i].position;
            float param_33 = _1283.u_point_lights[i].radius;
            point_shadow = PointShadowCalculation(param_30, param_31, param_32, param_33);
        }
        Lo += ((((((kD_1 * surface_albedo) / vec3(3.1415927410125732421875)) + specular_2) * radiance) * NdotL_1) * (1.0 - point_shadow));
    }
    for (uint si = 0u; si < cl_spot_count; si++)
    {
        int i_1 = int(_1271.light_indices[(cl_offset + cl_point_count) + si]);
        if (i_1 >= _1466.u_spot_light_count)
        {
            continue;
        }
        vec3 L_4 = normalize(_1466.u_spot_lights[i_1].position - vFragPos);
        vec3 H_2 = normalize(V + L_4);
        float _distance_1 = length(_1466.u_spot_lights[i_1].position - vFragPos);
        float attenuation_1 = clamp(1.0 - ((_distance_1 * _distance_1) / (_1466.u_spot_lights[i_1].radius * _1466.u_spot_lights[i_1].radius)), 0.0, 1.0);
        attenuation_1 *= attenuation_1;
        vec3 spotDir = normalize(-_1466.u_spot_lights[i_1].direction);
        float theta = dot(L_4, spotDir);
        float outerCos = cos(radians(_1466.u_spot_lights[i_1].outer_cone));
        float innerCos = cos(radians(_1466.u_spot_lights[i_1].inner_cone));
        float epsilon = max(innerCos - outerCos, 9.9999997473787516355514526367188e-05);
        float cone = clamp((theta - outerCos) / epsilon, 0.0, 1.0);
        if (cone <= 0.0)
        {
            continue;
        }
        vec3 radiance_1 = ((_1466.u_spot_lights[i_1].color * _1466.u_spot_lights[i_1].intensity) * attenuation_1) * cone;
        vec3 param_34 = N;
        vec3 param_35 = H_2;
        float param_36 = roughness;
        float NDF_2 = DistributionGGX(param_34, param_35, param_36);
        vec3 param_37 = N;
        vec3 param_38 = V;
        vec3 param_39 = L_4;
        float param_40 = roughness;
        float G_2 = GeometrySmith(param_37, param_38, param_39, param_40);
        float param_41 = max(dot(H_2, V), 0.0);
        vec3 param_42 = F0;
        vec3 F_2 = fresnelSchlick(param_41, param_42);
        vec3 numerator_2 = F_2 * (NDF_2 * G_2);
        float denominator_2 = ((4.0 * max(dot(N, V), 0.0)) * max(dot(N, L_4), 0.0)) + 9.9999997473787516355514526367188e-05;
        vec3 specular_3 = numerator_2 / vec3(denominator_2);
        vec3 kS_2 = F_2;
        vec3 kD_2 = vec3(1.0) - kS_2;
        kD_2 *= (1.0 - metallic);
        float NdotL_2 = max(dot(N, L_4), 0.0);
        float spot_shadow = 0.0;
        if (_1466.u_spot_lights[i_1].cast_shadow != 0)
        {
            int param_43 = _1466.u_spot_lights[i_1].shadow_index;
            vec3 param_44 = vFragPos;
            vec3 param_45 = N;
            vec3 param_46 = L_4;
            spot_shadow = SpotShadowCalculation(param_43, param_44, param_45, param_46);
        }
        Lo += ((((((kD_2 * surface_albedo) / vec3(3.1415927410125732421875)) + specular_3) * radiance_1) * NdotL_2) * (1.0 - spot_shadow));
    }
    float param_47 = max(dot(N, V), 0.0);
    vec3 param_48 = F0;
    vec3 F_3 = fresnelSchlick(param_47, param_48);
    vec3 kS_ambient = F_3;
    vec3 kD_ambient = vec3(1.0) - kS_ambient;
    kD_ambient *= (1.0 - metallic);
    vec3 irradiance = vec3(_245.light_color_and_ambient.w);
    vec3 diffuse_ambient = irradiance * surface_albedo;
    vec3 specular_ambient = (irradiance * F0) * (1.0 - roughness);
    vec3 ambient = ((kD_ambient * diffuse_ambient) + specular_ambient) * ao;
    vec3 color_1 = (ambient + Lo) + surface_emissive;
    color_1 /= (color_1 + vec3(1.0));
    color_1 = pow(color_1, vec3(0.4545454680919647216796875));
    FragColor = vec4(color_1, texColor.w * vColor.w);
}

