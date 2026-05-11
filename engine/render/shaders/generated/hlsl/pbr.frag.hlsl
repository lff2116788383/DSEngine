struct ClusterInfoEntry
{
    uint offset;
    uint point_count;
    uint spot_count;
    uint _pad;
};

struct PointLight
{
    float3 color;
    float intensity;
    float3 position;
    float radius;
    int cast_shadow;
    int shadow_index;
    float2 _pad;
};

struct SpotLight
{
    float3 color;
    float intensity;
    float3 position;
    float radius;
    float3 direction;
    float inner_cone;
    float outer_cone;
    int cast_shadow;
    int shadow_index;
    float _pad;
};

cbuffer PerScene : register(b0)
{
    float4 _245_light_dir_and_enabled : packoffset(c0);
    float4 _245_light_color_and_ambient : packoffset(c1);
    float4 _245_light_params : packoffset(c2);
    float4 _245_cascade_splits : packoffset(c3);
    row_major float4x4 _245_light_space_matrices[3] : packoffset(c4);
};

cbuffer SpotLightData : register(b10)
{
    row_major float4x4 _438_u_spot_light_space_matrices[4] : packoffset(c0);
};

cbuffer PerMaterial : register(b0)
{
    float4 _640_albedo : packoffset(c0);
    float4 _640_roughness_ao : packoffset(c1);
    float4 _640_emissive : packoffset(c2);
    float4 _640_flags : packoffset(c3);
};

cbuffer PerFrame : register(b0)
{
    row_major float4x4 _769_vp : packoffset(c0);
    row_major float4x4 _769_view : packoffset(c4);
    float4 _769_camera_pos : packoffset(c8);
};

ByteAddressBuffer _1178 : register(t19);
ByteAddressBuffer _1271 : register(t20);
ByteAddressBuffer _1283 : register(t17);
ByteAddressBuffer _1466 : register(t18);
Texture2D<float4> u_shadow_maps[3] : register(t6);
SamplerComparisonState _u_shadow_maps_sampler[3] : register(s6);
Texture2D<float4> u_spot_shadow_maps[4] : register(t7);
SamplerState _u_spot_shadow_maps_sampler[4] : register(s7);
TextureCube<float4> u_point_shadow_maps[4] : register(t0);
SamplerState _u_point_shadow_maps_sampler[4] : register(s0);
Texture2D<float4> u_texture : register(t1);
SamplerState _u_texture_sampler : register(s1);
Texture2D<float4> u_normal_map : register(t2);
SamplerState _u_normal_map_sampler : register(s2);
Texture2D<float4> u_emissive_map : register(t4);
SamplerState _u_emissive_map_sampler : register(s4);
Texture2D<float4> u_metallic_roughness_map : register(t3);
SamplerState _u_metallic_roughness_map_sampler : register(s3);
Texture2D<float4> u_occlusion_map : register(t5);
SamplerState _u_occlusion_map_sampler : register(s5);

static float4 gl_FragCoord;
static float2 vTexCoord;
static float3 vNormal;
static float3x3 vTBN;
static float4 vColor;
static float4 FragColor;
static float3 vFragPos;
static float3 vFragPosViewSpace;

struct SPIRV_Cross_Input
{
    float4 vColor : TEXCOORD0;
    float2 vTexCoord : TEXCOORD1;
    float3 vFragPos : TEXCOORD2;
    float3 vNormal : TEXCOORD3;
    float3x3 vTBN : TEXCOORD4;
    float3 vFragPosViewSpace : TEXCOORD7;
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 FragColor : SV_Target0;
};

uint2 spvTextureSize(Texture2D<float4> Tex, uint Level, out uint Param)
{
    uint2 ret;
    Tex.GetDimensions(Level, ret.x, ret.y, Param);
    return ret;
}

float SampleShadowPCF(Texture2D<float4> shadowMap, SamplerComparisonState _shadowMap_sampler, float3 proj_coords, float bias)
{
    float shadow = 0.0f;
    uint _181_dummy_parameter;
    float2 texel_size = 1.0f.xx / float2(int2(spvTextureSize(shadowMap, uint(0), _181_dummy_parameter)));
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float3 _223 = float3(proj_coords.xy + (float2(float(x), float(y)) * texel_size), proj_coords.z - bias);
            shadow += shadowMap.SampleCmp(_shadowMap_sampler, _223.xy, _223.z);
        }
    }
    return shadow / 9.0f;
}

float ShadowForCascade(int idx, float3 fragPosWorldSpace, float3 normal, float3 lightDir)
{
    float4 fragPosLightSpace = mul(float4(fragPosWorldSpace, 1.0f), _245_light_space_matrices[idx]);
    float3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w.xxx;
    float3 _264 = projCoords;
    float2 _269 = (_264.xy * 0.5f) + 0.5f.xx;
    projCoords.x = _269.x;
    projCoords.y = _269.y;
    if (projCoords.z > 1.0f)
    {
        return 0.0f;
    }
    bool _284 = projCoords.x < 0.0f;
    bool _291;
    if (!_284)
    {
        _291 = projCoords.x > 1.0f;
    }
    else
    {
        _291 = _284;
    }
    bool _298;
    if (!_291)
    {
        _298 = projCoords.y < 0.0f;
    }
    else
    {
        _298 = _291;
    }
    bool _305;
    if (!_298)
    {
        _305 = projCoords.y > 1.0f;
    }
    else
    {
        _305 = _298;
    }
    if (_305)
    {
        return 0.0f;
    }
    float bias = max(0.004999999888241291046142578125f * (1.0f - dot(normal, lightDir)), 0.0005000000237487256526947021484375f);
    float3 param = projCoords;
    float param_1 = bias;
    float lit = SampleShadowPCF(u_shadow_maps[idx], _u_shadow_maps_sampler[idx], param, param_1);
    return 1.0f - lit;
}

float ShadowCalculation(float3 fragPosWorldSpace, float3 fragPosViewSpace, float3 normal, float3 lightDir)
{
    if (!(_245_light_params.z != 0.0f))
    {
        return 0.0f;
    }
    float viewDepth = abs(fragPosViewSpace.z);
    int cascadeIndex = 2;
    for (int i = 0; i < 2; i++)
    {
        if (viewDepth < _245_cascade_splits[uint3(0u, 1u, 2u)[i]])
        {
            cascadeIndex = i;
            break;
        }
    }
    int param = cascadeIndex;
    float3 param_1 = fragPosWorldSpace;
    float3 param_2 = normal;
    float3 param_3 = lightDir;
    float shadow = ShadowForCascade(param, param_1, param_2, param_3);
    if (cascadeIndex < 2)
    {
        float splitEnd = _245_cascade_splits[uint3(0u, 1u, 2u)[cascadeIndex]];
        float blendStart = splitEnd * 0.800000011920928955078125f;
        if (viewDepth > blendStart)
        {
            float blendFactor = smoothstep(blendStart, splitEnd, viewDepth);
            int param_4 = cascadeIndex + 1;
            float3 param_5 = fragPosWorldSpace;
            float3 param_6 = normal;
            float3 param_7 = lightDir;
            float nextShadow = ShadowForCascade(param_4, param_5, param_6, param_7);
            shadow = lerp(shadow, nextShadow, blendFactor);
        }
    }
    return clamp(shadow * _245_light_params.y, 0.0f, 1.0f);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0f)) + 1.0f;
    denom = (3.1415927410125732421875f * denom) * denom;
    return nom / max(denom, 1.0000000116860974230803549289703e-07f);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    float nom = NdotV;
    float denom = (NdotV * (1.0f - k)) + k;
    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float param = NdotV;
    float param_1 = roughness;
    float ggx2 = GeometrySchlickGGX(param, param_1);
    float param_2 = NdotL;
    float param_3 = roughness;
    float ggx1 = GeometrySchlickGGX(param_2, param_3);
    return ggx1 * ggx2;
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + ((1.0f.xxx - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f));
}

float PointShadowCalculation(int shadowIndex, float3 fragPosWorldSpace, float3 lightPos, float lightRadius)
{
    if ((shadowIndex < 0) || (shadowIndex >= 4))
    {
        return 0.0f;
    }
    float3 fragToLight = fragPosWorldSpace - lightPos;
    float currentDepth = length(fragToLight);
    if (currentDepth >= lightRadius)
    {
        return 0.0f;
    }
    float closestDepth = u_point_shadow_maps[shadowIndex].Sample(_u_point_shadow_maps_sampler[shadowIndex], fragToLight).x * lightRadius;
    float bias = 0.0500000007450580596923828125f;
    float _622;
    if ((currentDepth - bias) > closestDepth)
    {
        _622 = _245_light_params.y;
    }
    else
    {
        _622 = 0.0f;
    }
    return _622;
}

float SpotShadowCalculation(int shadowIndex, float3 fragPosWorldSpace, float3 normal, float3 lightDir)
{
    if ((shadowIndex < 0) || (shadowIndex >= 4))
    {
        return 0.0f;
    }
    float4 fragPosLightSpace = mul(float4(fragPosWorldSpace, 1.0f), _438_u_spot_light_space_matrices[shadowIndex]);
    float3 projCoords = fragPosLightSpace.xyz / max(fragPosLightSpace.w, 9.9999997473787516355514526367188e-05f).xxx;
    float3 _457 = projCoords;
    float2 _461 = (_457.xy * 0.5f) + 0.5f.xx;
    projCoords.x = _461.x;
    projCoords.y = _461.y;
    if (projCoords.z > 1.0f)
    {
        return 0.0f;
    }
    bool _474 = projCoords.x < 0.0f;
    bool _481;
    if (!_474)
    {
        _481 = projCoords.x > 1.0f;
    }
    else
    {
        _481 = _474;
    }
    bool _488;
    if (!_481)
    {
        _488 = projCoords.y < 0.0f;
    }
    else
    {
        _488 = _481;
    }
    bool _495;
    if (!_488)
    {
        _495 = projCoords.y > 1.0f;
    }
    else
    {
        _495 = _488;
    }
    if (_495)
    {
        return 0.0f;
    }
    float currentDepth = projCoords.z;
    float bias = max(0.0030000000260770320892333984375f * (1.0f - dot(normal, lightDir)), 0.0005000000237487256526947021484375f);
    float shadow = 0.0f;
    uint _522_dummy_parameter;
    float2 texelSize = 1.0f.xx / float2(int2(spvTextureSize(u_spot_shadow_maps[shadowIndex], uint(0), _522_dummy_parameter)));
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float pcfDepth = u_spot_shadow_maps[shadowIndex].Sample(_u_spot_shadow_maps_sampler[shadowIndex], projCoords.xy + (float2(float(x), float(y)) * texelSize)).x;
            shadow += float((currentDepth - bias) > pcfDepth);
        }
    }
    shadow /= 9.0f;
    return clamp(shadow * _245_light_params.y, 0.0f, 1.0f);
}

void frag_main()
{
    float4 texColor = u_texture.Sample(_u_texture_sampler, vTexCoord);
    bool _643 = _640_emissive.w != 0.0f;
    bool _652;
    if (_643)
    {
        _652 = texColor.w < clamp(_640_roughness_ao.w, 0.0f, 1.0f);
    }
    else
    {
        _652 = _643;
    }
    if (_652)
    {
        discard;
    }
    float3 N = vNormal;
    if (_640_flags.x != 0.0f)
    {
        float3 normalMap = u_normal_map.Sample(_u_normal_map_sampler, vTexCoord).xyz;
        normalMap = (normalMap * 2.0f) - 1.0f.xxx;
        float3 _678 = normalMap;
        float2 _680 = _678.xy * _640_roughness_ao.z;
        normalMap.x = _680.x;
        normalMap.y = _680.y;
        N = normalize(mul(normalMap, vTBN));
    }
    if (!(_245_light_dir_and_enabled.w != 0.0f))
    {
        float3 result = (texColor.xyz * vColor.xyz) * _640_albedo.xyz;
        if (_640_flags.z != 0.0f)
        {
            result += (u_emissive_map.Sample(_u_emissive_map_sampler, vTexCoord).xyz * _640_emissive.xyz);
        }
        if (_245_light_params.w == 0.0f)
        {
            result /= (result + 1.0f.xxx);
            result = pow(result, 0.4545454680919647216796875f.xxx);
        }
        FragColor = float4(result, texColor.w * vColor.w);
        return;
    }
    if (_245_light_params.w == 2.0f)
    {
        float3 L = normalize(-_245_light_dir_and_enabled.xyz);
        float3 V_hl = normalize(_769_camera_pos.xyz - vFragPos);
        float3 R = reflect(_245_light_dir_and_enabled.xyz, N);
        float half_lambert = (dot(N, L) * 0.5f) + 0.5f;
        float3 diffuse_color = ((texColor.xyz * vColor.xyz) * _640_albedo.xyz) * half_lambert;
        float spec_brightness = pow(max(dot(R, V_hl), 0.0f), 100.0f);
        float3 _812;
        if (_640_flags.y != 0.0f)
        {
            _812 = u_metallic_roughness_map.Sample(_u_metallic_roughness_map_sampler, vTexCoord).xyz;
        }
        else
        {
            _812 = 0.0f.xxx;
        }
        float3 spec_tex = _812;
        float3 specular_color = spec_tex * spec_brightness;
        float3 param = vFragPos;
        float3 param_1 = vFragPosViewSpace;
        float3 param_2 = N;
        float3 param_3 = L;
        float shadow = ShadowCalculation(param, param_1, param_2, param_3);
        float shadow_multiplier = 1.0f - (shadow * 0.5f);
        float3 color = (diffuse_color + specular_color) * shadow_multiplier;
        FragColor = float4(color, 1.0f);
        return;
    }
    if (_245_light_params.w == 3.0f)
    {
        float3 L_1 = normalize(-_245_light_dir_and_enabled.xyz);
        float3 V_st = normalize(_769_camera_pos.xyz - vFragPos);
        float3 R_1 = reflect(_245_light_dir_and_enabled.xyz, N);
        float half_lambert_1 = (dot(N, L_1) * 0.5f) + 0.5f;
        float3 diffuse = ((_640_albedo.xyz * half_lambert_1) * _245_light_color_and_ambient.xyz) * _245_light_params.x;
        float spec_power = max(_640_roughness_ao.x, 1.0f);
        float3 spec_color = _640_albedo.w.xxx;
        float3 specular = spec_color * pow(max(dot(R_1, V_st), 0.0f), spec_power);
        float3 emissive_val = _640_emissive.xyz;
        float3 material_color = (diffuse + specular) + emissive_val;
        float3 color_st = (material_color * texColor.xyz) * vColor.xyz;
        float3 param_4 = vFragPos;
        float3 param_5 = vFragPosViewSpace;
        float3 param_6 = N;
        float3 param_7 = L_1;
        float shadow_1 = ShadowCalculation(param_4, param_5, param_6, param_7);
        float shadow_multiplier_1 = 1.0f - (shadow_1 * 0.5f);
        FragColor = float4(color_st * shadow_multiplier_1, texColor.w * vColor.w);
        return;
    }
    float3 surface_albedo = pow((texColor.xyz * vColor.xyz) * _640_albedo.xyz, 2.2000000476837158203125f.xxx);
    float metallic = clamp(_640_albedo.w, 0.0f, 1.0f);
    float roughness = clamp(_640_roughness_ao.x, 0.039999999105930328369140625f, 1.0f);
    float ao = max(_640_roughness_ao.y, 0.0f);
    float3 surface_emissive = _640_emissive.xyz;
    if (_640_flags.y != 0.0f)
    {
        float4 mrSample = u_metallic_roughness_map.Sample(_u_metallic_roughness_map_sampler, vTexCoord);
        roughness = clamp(mrSample.y * _640_roughness_ao.x, 0.039999999105930328369140625f, 1.0f);
        metallic = clamp(mrSample.z * _640_albedo.w, 0.0f, 1.0f);
    }
    if (_640_flags.w != 0.0f)
    {
        ao *= u_occlusion_map.Sample(_u_occlusion_map_sampler, vTexCoord).x;
    }
    if (_640_flags.z != 0.0f)
    {
        surface_emissive *= u_emissive_map.Sample(_u_emissive_map_sampler, vTexCoord).xyz;
    }
    float3 V = normalize(_769_camera_pos.xyz - vFragPos);
    float3 F0 = 0.039999999105930328369140625f.xxx;
    F0 = lerp(F0, surface_albedo, metallic.xxx);
    float3 Lo = 0.0f.xxx;
    float3 L_2 = normalize(-_245_light_dir_and_enabled.xyz);
    float3 H = normalize(V + L_2);
    float3 param_8 = N;
    float3 param_9 = H;
    float param_10 = roughness;
    float NDF = DistributionGGX(param_8, param_9, param_10);
    float3 param_11 = N;
    float3 param_12 = V;
    float3 param_13 = L_2;
    float param_14 = roughness;
    float G = GeometrySmith(param_11, param_12, param_13, param_14);
    float param_15 = max(dot(H, V), 0.0f);
    float3 param_16 = F0;
    float3 F = fresnelSchlick(param_15, param_16);
    float3 numerator = F * (NDF * G);
    float denominator = ((4.0f * max(dot(N, V), 0.0f)) * max(dot(N, L_2), 0.0f)) + 9.9999997473787516355514526367188e-05f;
    float3 specular_1 = numerator / denominator.xxx;
    float3 kS = F;
    float3 kD = 1.0f.xxx - kS;
    kD *= (1.0f - metallic);
    float NdotL = max(dot(N, L_2), 0.0f);
    float3 param_17 = vFragPos;
    float3 param_18 = vFragPosViewSpace;
    float3 param_19 = N;
    float3 param_20 = L_2;
    float shadow_2 = ShadowCalculation(param_17, param_18, param_19, param_20);
    Lo += (((((((kD * surface_albedo) / 3.1415927410125732421875f.xxx) + specular_1) * _245_light_color_and_ambient.xyz) * _245_light_params.x) * NdotL) * (1.0f - shadow_2));
    int cl_tx = int(gl_FragCoord.x) / 16;
    int cl_ty = int(gl_FragCoord.y) / 16;
    float cl_linear_z = max(-vFragPosViewSpace.z, 9.9999997473787516355514526367188e-05f);
    float cl_log_ratio = log(asfloat(_1178.Load(16)) / max(asfloat(_1178.Load(12)), 9.9999997473787516355514526367188e-05f));
    int _1189;
    if (cl_log_ratio > 0.0f)
    {
        _1189 = clamp(int((log(cl_linear_z / max(asfloat(_1178.Load(12)), 9.9999997473787516355514526367188e-05f)) / cl_log_ratio) * float(_1178.Load(8))), 0, int(_1178.Load(8)) - 1);
    }
    else
    {
        _1189 = 0;
    }
    int cl_tz = _1189;
    int cl_idx = (((cl_tz * int(_1178.Load(4))) + cl_ty) * int(_1178.Load(0))) + cl_tx;
    int cl_total = (int(_1178.Load(0)) * int(_1178.Load(4))) * int(_1178.Load(8));
    cl_idx = clamp(cl_idx, 0, max((cl_total - 1), 0));
    uint cl_offset = _1178.Load(cl_idx * 16 + 32);
    uint cl_point_count = _1178.Load(cl_idx * 16 + 36);
    uint cl_spot_count = _1178.Load(cl_idx * 16 + 40);
    for (uint ci = 0u; ci < cl_point_count; ci++)
    {
        int i = int(_1271.Load((cl_offset + ci) * 4 + 0));
        if (i >= int(_1283.Load(0)))
        {
            continue;
        }
        float3 L_3 = normalize(asfloat(_1283.Load3(i * 48 + 32)) - vFragPos);
        float3 H_1 = normalize(V + L_3);
        float _distance = length(asfloat(_1283.Load3(i * 48 + 32)) - vFragPos);
        float attenuation = clamp(1.0f - ((_distance * _distance) / (asfloat(_1283.Load(i * 48 + 44)) * asfloat(_1283.Load(i * 48 + 44)))), 0.0f, 1.0f);
        attenuation *= attenuation;
        float3 radiance = (asfloat(_1283.Load3(i * 48 + 16)) * asfloat(_1283.Load(i * 48 + 28))) * attenuation;
        float3 param_21 = N;
        float3 param_22 = H_1;
        float param_23 = roughness;
        float NDF_1 = DistributionGGX(param_21, param_22, param_23);
        float3 param_24 = N;
        float3 param_25 = V;
        float3 param_26 = L_3;
        float param_27 = roughness;
        float G_1 = GeometrySmith(param_24, param_25, param_26, param_27);
        float param_28 = max(dot(H_1, V), 0.0f);
        float3 param_29 = F0;
        float3 F_1 = fresnelSchlick(param_28, param_29);
        float3 numerator_1 = F_1 * (NDF_1 * G_1);
        float denominator_1 = ((4.0f * max(dot(N, V), 0.0f)) * max(dot(N, L_3), 0.0f)) + 9.9999997473787516355514526367188e-05f;
        float3 specular_2 = numerator_1 / denominator_1.xxx;
        float3 kS_1 = F_1;
        float3 kD_1 = 1.0f.xxx - kS_1;
        kD_1 *= (1.0f - metallic);
        float NdotL_1 = max(dot(N, L_3), 0.0f);
        float point_shadow = 0.0f;
        if (int(_1283.Load(i * 48 + 48)) != 0)
        {
            int param_30 = int(_1283.Load(i * 48 + 52));
            float3 param_31 = vFragPos;
            float3 param_32 = asfloat(_1283.Load3(i * 48 + 32));
            float param_33 = asfloat(_1283.Load(i * 48 + 44));
            point_shadow = PointShadowCalculation(param_30, param_31, param_32, param_33);
        }
        Lo += ((((((kD_1 * surface_albedo) / 3.1415927410125732421875f.xxx) + specular_2) * radiance) * NdotL_1) * (1.0f - point_shadow));
    }
    for (uint si = 0u; si < cl_spot_count; si++)
    {
        int i_1 = int(_1271.Load(((cl_offset + cl_point_count) + si) * 4 + 0));
        if (i_1 >= int(_1466.Load(0)))
        {
            continue;
        }
        float3 L_4 = normalize(asfloat(_1466.Load3(i_1 * 64 + 32)) - vFragPos);
        float3 H_2 = normalize(V + L_4);
        float _distance_1 = length(asfloat(_1466.Load3(i_1 * 64 + 32)) - vFragPos);
        float attenuation_1 = clamp(1.0f - ((_distance_1 * _distance_1) / (asfloat(_1466.Load(i_1 * 64 + 44)) * asfloat(_1466.Load(i_1 * 64 + 44)))), 0.0f, 1.0f);
        attenuation_1 *= attenuation_1;
        float3 spotDir = normalize(-asfloat(_1466.Load3(i_1 * 64 + 48)));
        float theta = dot(L_4, spotDir);
        float outerCos = cos(radians(asfloat(_1466.Load(i_1 * 64 + 64))));
        float innerCos = cos(radians(asfloat(_1466.Load(i_1 * 64 + 60))));
        float epsilon = max(innerCos - outerCos, 9.9999997473787516355514526367188e-05f);
        float cone = clamp((theta - outerCos) / epsilon, 0.0f, 1.0f);
        if (cone <= 0.0f)
        {
            continue;
        }
        float3 radiance_1 = ((asfloat(_1466.Load3(i_1 * 64 + 16)) * asfloat(_1466.Load(i_1 * 64 + 28))) * attenuation_1) * cone;
        float3 param_34 = N;
        float3 param_35 = H_2;
        float param_36 = roughness;
        float NDF_2 = DistributionGGX(param_34, param_35, param_36);
        float3 param_37 = N;
        float3 param_38 = V;
        float3 param_39 = L_4;
        float param_40 = roughness;
        float G_2 = GeometrySmith(param_37, param_38, param_39, param_40);
        float param_41 = max(dot(H_2, V), 0.0f);
        float3 param_42 = F0;
        float3 F_2 = fresnelSchlick(param_41, param_42);
        float3 numerator_2 = F_2 * (NDF_2 * G_2);
        float denominator_2 = ((4.0f * max(dot(N, V), 0.0f)) * max(dot(N, L_4), 0.0f)) + 9.9999997473787516355514526367188e-05f;
        float3 specular_3 = numerator_2 / denominator_2.xxx;
        float3 kS_2 = F_2;
        float3 kD_2 = 1.0f.xxx - kS_2;
        kD_2 *= (1.0f - metallic);
        float NdotL_2 = max(dot(N, L_4), 0.0f);
        float spot_shadow = 0.0f;
        if (int(_1466.Load(i_1 * 64 + 68)) != 0)
        {
            int param_43 = int(_1466.Load(i_1 * 64 + 72));
            float3 param_44 = vFragPos;
            float3 param_45 = N;
            float3 param_46 = L_4;
            spot_shadow = SpotShadowCalculation(param_43, param_44, param_45, param_46);
        }
        Lo += ((((((kD_2 * surface_albedo) / 3.1415927410125732421875f.xxx) + specular_3) * radiance_1) * NdotL_2) * (1.0f - spot_shadow));
    }
    float param_47 = max(dot(N, V), 0.0f);
    float3 param_48 = F0;
    float3 F_3 = fresnelSchlick(param_47, param_48);
    float3 kS_ambient = F_3;
    float3 kD_ambient = 1.0f.xxx - kS_ambient;
    kD_ambient *= (1.0f - metallic);
    float3 irradiance = _245_light_color_and_ambient.w.xxx;
    float3 diffuse_ambient = irradiance * surface_albedo;
    float3 specular_ambient = (irradiance * F0) * (1.0f - roughness);
    float3 ambient = ((kD_ambient * diffuse_ambient) + specular_ambient) * ao;
    float3 color_1 = (ambient + Lo) + surface_emissive;
    color_1 /= (color_1 + 1.0f.xxx);
    color_1 = pow(color_1, 0.4545454680919647216796875f.xxx);
    FragColor = float4(color_1, texColor.w * vColor.w);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    vTexCoord = stage_input.vTexCoord;
    vNormal = stage_input.vNormal;
    vTBN = stage_input.vTBN;
    vColor = stage_input.vColor;
    vFragPos = stage_input.vFragPos;
    vFragPosViewSpace = stage_input.vFragPosViewSpace;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
