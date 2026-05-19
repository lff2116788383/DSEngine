#version 430

const vec2 _676[16] = vec2[](vec2(-0.94201624393463134765625, -0.39906215667724609375), vec2(0.94558608531951904296875, -0.768907248973846435546875), vec2(-0.094184100627899169921875, -0.929388701915740966796875), vec2(0.34495937824249267578125, 0.29387760162353515625), vec2(-0.91588580608367919921875, 0.4577143192291259765625), vec2(-0.8154423236846923828125, -0.87912464141845703125), vec2(-0.38277542591094970703125, 0.2767684459686279296875), vec2(0.9748439788818359375, 0.7564837932586669921875), vec2(0.4432332515716552734375, -0.9751155376434326171875), vec2(0.5374298095703125, -0.473734200000762939453125), vec2(-0.2649691104888916015625, -0.418930232524871826171875), vec2(0.79197514057159423828125, 0.19090187549591064453125), vec2(-0.24188840389251708984375, 0.997065067291259765625), vec2(-0.8140995502471923828125, 0.91437590122222900390625), vec2(0.1998412609100341796875, 0.786413669586181640625), vec2(0.14383161067962646484375, -0.141007900238037109375));

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

layout(binding = 5, std140) uniform LightProbeData
{
    vec4 sh_coefficients[9];
    vec4 probe_params;
} _236;

layout(binding = 0, std140) uniform PerScene
{
    vec4 light_dir_and_enabled;
    vec4 light_color_and_ambient;
    vec4 light_params;
    vec4 cascade_splits;
    mat4 light_space_matrices[3];
} _834;

layout(binding = 10, std140) uniform SpotLightData
{
    mat4 u_spot_light_space_matrices[4];
} _1020;

layout(binding = 0, std140) uniform PerMaterial
{
    vec4 albedo;
    vec4 roughness_ao;
    vec4 emissive;
    vec4 flags;
    vec4 extra_params;
    vec4 extra_params2;
    vec4 toon_shadow_color;
    vec4 toon_params;
} _1260;

layout(binding = 0, std140) uniform PerFrame
{
    mat4 vp;
    mat4 view;
    vec4 camera_pos;
} _1280;

layout(binding = 16, std140) uniform TerrainParams
{
    float u_splat_enabled;
    float _tp_pad0;
    float _tp_pad1;
    float _tp_pad2;
    vec4 u_splat_tiling;
} _1300;

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
} _2340;

layout(binding = 4, std430) readonly buffer LightIndexSSBO
{
    uint light_indices[];
} _2432;

layout(binding = 1, std430) readonly buffer PointLightSSBO
{
    int u_point_light_count;
    int _pl_pad0;
    int _pl_pad1;
    int _pl_pad2;
    PointLight u_point_lights[];
} _2444;

layout(binding = 2, std430) readonly buffer SpotLightSSBO
{
    int u_spot_light_count;
    int _sl_pad0;
    int _sl_pad1;
    int _sl_pad2;
    SpotLight u_spot_lights[];
} _2626;

layout(binding = 2) uniform sampler2D u_normal_map;
layout(binding = 8) uniform samplerCube u_reflection_cubemap;
layout(binding = 9) uniform sampler2D u_brdf_lut;
layout(binding = 6) uniform sampler2DShadow u_shadow_maps[3];
layout(binding = 7) uniform sampler2D u_spot_shadow_maps[4];
layout(binding = 0) uniform samplerCube u_point_shadow_maps[4];
layout(binding = 11) uniform sampler2D u_splat_weight_map;
layout(binding = 12) uniform sampler2D u_splat_layer0;
layout(binding = 13) uniform sampler2D u_splat_layer1;
layout(binding = 14) uniform sampler2D u_splat_layer2;
layout(binding = 15) uniform sampler2D u_splat_layer3;
layout(binding = 1) uniform sampler2D u_texture;
layout(binding = 4) uniform sampler2D u_emissive_map;
layout(binding = 3) uniform sampler2D u_metallic_roughness_map;
layout(binding = 5) uniform sampler2D u_occlusion_map;

layout(location = 0) out vec4 FragColor;
layout(location = 1) in vec2 vTexCoord;
layout(location = 4) in mat3 vTBN;
layout(location = 2) in vec3 vFragPos;
layout(location = 0) in vec4 vColor;
layout(location = 3) in vec3 vNormal;
layout(location = 7) in vec3 vFragPosViewSpace;

vec2 ParallaxOcclusionMapping(vec2 uv, vec3 viewDirTS, float height_scale)
{
    float layerDepth = 0.0625;
    float currentLayerDepth = 0.0;
    vec2 P = (viewDirTS.xy / vec2(max(viewDirTS.z, 0.001000000047497451305389404296875))) * height_scale;
    vec2 deltaUV = P / vec2(16.0);
    vec2 curUV = uv;
    float curDepth = 1.0 - texture(u_normal_map, curUV).w;
    for (int i = 0; i < 16; i++)
    {
        if (currentLayerDepth >= curDepth)
        {
            break;
        }
        curUV -= deltaUV;
        curDepth = 1.0 - texture(u_normal_map, curUV).w;
        currentLayerDepth += layerDepth;
    }
    vec2 prevUV = curUV + deltaUV;
    float afterDepth = curDepth - currentLayerDepth;
    float beforeDepth = ((1.0 - texture(u_normal_map, prevUV).w) - currentLayerDepth) + layerDepth;
    float w = afterDepth / ((afterDepth - beforeDepth) + 9.9999997473787516355514526367188e-05);
    return mix(curUV, prevUV, vec2(w));
}

void OutputFragment(vec3 color, float alpha)
{
    if (_834.cascade_splits.w > 0.5)
    {
        float z = gl_FragCoord.z;
        float w = alpha * max(0.00999999977648258209228515625, 3000.0 * pow(1.0 - z, 3.0));
        if (_834.cascade_splits.w < 1.5)
        {
            FragColor = vec4((color * alpha) * w, alpha * w);
        }
        else
        {
            FragColor = vec4(0.0, 0.0, 0.0, alpha);
        }
        return;
    }
    FragColor = vec4(color, alpha);
}

float FindBlockerDepth(sampler2DShadow shadowMap, vec2 uv, float receiverDepth, float searchRadius)
{
    float blockerSum = 0.0;
    int blockerCount = 0;
    for (int i = 0; i < 16; i++)
    {
        vec2 sampleUV = uv + (_676[i] * searchRadius);
        vec3 _691 = vec3(sampleUV, receiverDepth);
        float vis = texture(shadowMap, vec3(_691.xy, _691.z));
        if (vis < 0.5)
        {
            float lo = 0.0;
            float hi = receiverDepth;
            for (int b = 0; b < 3; b++)
            {
                float mid = (lo + hi) * 0.5;
                vec3 _719 = vec3(sampleUV, mid);
                if (texture(shadowMap, vec3(_719.xy, _719.z)) < 0.5)
                {
                    hi = mid;
                }
                else
                {
                    lo = mid;
                }
            }
            blockerSum += ((lo + hi) * 0.5);
            blockerCount++;
        }
    }
    if (blockerCount == 0)
    {
        return -1.0;
    }
    return blockerSum / float(blockerCount);
}

float PCSS_Shadow(sampler2DShadow shadowMap, vec3 projCoords, float bias)
{
    vec2 texelSize = vec2(1.0) / vec2(textureSize(shadowMap, 0));
    float receiverDepth = projCoords.z - bias;
    vec2 param = projCoords.xy;
    float param_1 = receiverDepth;
    float param_2 = 0.008000000379979610443115234375;
    float avgBlockerDepth = FindBlockerDepth(shadowMap, param, param_1, param_2);
    if (avgBlockerDepth < 0.0)
    {
        return 1.0;
    }
    float penumbraWidth = (0.0040000001899898052215576171875 * (receiverDepth - avgBlockerDepth)) / max(avgBlockerDepth, 9.9999997473787516355514526367188e-05);
    float filterRadius = max(penumbraWidth, texelSize.x);
    float shadow = 0.0;
    for (int i = 0; i < 16; i++)
    {
        vec2 offset = _676[i] * filterRadius;
        vec3 _817 = vec3(projCoords.xy + offset, receiverDepth);
        shadow += texture(shadowMap, vec3(_817.xy, _817.z));
    }
    return shadow / 16.0;
}

float ShadowForCascade(int idx, vec3 fragPosWorldSpace, vec3 normal, vec3 lightDir)
{
    vec4 fragPosLightSpace = _834.light_space_matrices[idx] * vec4(fragPosWorldSpace, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / vec3(fragPosLightSpace.w);
    projCoords = (projCoords * 0.5) + vec3(0.5);
    if (projCoords.z > 1.0)
    {
        return 0.0;
    }
    bool _864 = projCoords.x < 0.0;
    bool _871;
    if (!_864)
    {
        _871 = projCoords.x > 1.0;
    }
    else
    {
        _871 = _864;
    }
    bool _878;
    if (!_871)
    {
        _878 = projCoords.y < 0.0;
    }
    else
    {
        _878 = _871;
    }
    bool _885;
    if (!_878)
    {
        _885 = projCoords.y > 1.0;
    }
    else
    {
        _885 = _878;
    }
    if (_885)
    {
        return 0.0;
    }
    float bias = max(0.004999999888241291046142578125 * (1.0 - dot(normal, lightDir)), 0.0005000000237487256526947021484375);
    vec3 param = projCoords;
    float param_1 = bias;
    float lit = PCSS_Shadow(u_shadow_maps[idx], param, param_1);
    return clamp((1.0 - lit) * _834.light_params.y, 0.0, 1.0);
}

float ShadowCalculation(vec3 fragPosWorldSpace, vec3 fragPosViewSpace, vec3 normal, vec3 lightDir)
{
    if (!(_834.light_params.z != 0.0))
    {
        return 0.0;
    }
    float viewDepth = abs(fragPosViewSpace.z);
    int cascadeIndex = 2;
    for (int i = 0; i < 2; i++)
    {
        if (viewDepth < _834.cascade_splits[uvec3(0u, 1u, 2u)[i]])
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
        float splitEnd = _834.cascade_splits[uvec3(0u, 1u, 2u)[cascadeIndex]];
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
    return clamp(shadow * _834.light_params.y, 0.0, 1.0);
}

float DistributionGGXAniso(vec3 N, vec3 H, vec3 T, vec3 B, float roughness, float aniso)
{
    float at = max(roughness * (1.0 + aniso), 0.001000000047497451305389404296875);
    float ab = max(roughness * (1.0 - aniso), 0.001000000047497451305389404296875);
    float TdotH = dot(T, H);
    float BdotH = dot(B, H);
    float NdotH = dot(N, H);
    float d = (((TdotH * TdotH) / (at * at)) + ((BdotH * BdotH) / (ab * ab))) + (NdotH * NdotH);
    return 1.0 / (((((3.1415927410125732421875 * at) * ab) * d) * d) + 9.9999997473787516355514526367188e-05);
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

vec3 SubsurfaceScattering(vec3 N, vec3 L, vec3 albedo, float sss, vec3 light_col, float li, vec3 tint)
{
    float wrap = 0.5 * sss;
    float NdotL_wrap = max(0.0, (dot(N, L) + wrap) / (1.0 + wrap));
    float NdotL_std = max(dot(N, L), 0.0);
    float diff = NdotL_wrap - NdotL_std;
    bvec3 _535 = bvec3(dot(tint, tint) > 0.0);
    vec3 sss_tint = vec3(_535.x ? tint.x : vec3(1.0, 0.3499999940395355224609375, 0.20000000298023223876953125).x, _535.y ? tint.y : vec3(1.0, 0.3499999940395355224609375, 0.20000000298023223876953125).y, _535.z ? tint.z : vec3(1.0, 0.3499999940395355224609375, 0.20000000298023223876953125).z);
    return (((albedo * sss_tint) * diff) * light_col) * li;
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
    float _1194;
    if ((currentDepth - bias) > closestDepth)
    {
        _1194 = _834.light_params.y;
    }
    else
    {
        _1194 = 0.0;
    }
    return _1194;
}

float SpotShadowCalculation(int shadowIndex, vec3 fragPosWorldSpace, vec3 normal, vec3 lightDir)
{
    if ((shadowIndex < 0) || (shadowIndex >= 4))
    {
        return 0.0;
    }
    vec4 fragPosLightSpace = _1020.u_spot_light_space_matrices[shadowIndex] * vec4(fragPosWorldSpace, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / vec3(max(fragPosLightSpace.w, 9.9999997473787516355514526367188e-05));
    projCoords = (projCoords * 0.5) + vec3(0.5);
    if (projCoords.z > 1.0)
    {
        return 0.0;
    }
    bool _1050 = projCoords.x < 0.0;
    bool _1057;
    if (!_1050)
    {
        _1057 = projCoords.x > 1.0;
    }
    else
    {
        _1057 = _1050;
    }
    bool _1064;
    if (!_1057)
    {
        _1064 = projCoords.y < 0.0;
    }
    else
    {
        _1064 = _1057;
    }
    bool _1071;
    if (!_1064)
    {
        _1071 = projCoords.y > 1.0;
    }
    else
    {
        _1071 = _1064;
    }
    if (_1071)
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
    return clamp(shadow * _834.light_params.y, 0.0, 1.0);
}

vec3 EvaluateSH(vec3 N)
{
    vec3 _346 = ((((((((_236.sh_coefficients[0].xyz * 0.2820949852466583251953125) + ((_236.sh_coefficients[1].xyz * 0.48860299587249755859375) * N.y)) + ((_236.sh_coefficients[2].xyz * 0.48860299587249755859375) * N.z)) + ((_236.sh_coefficients[3].xyz * 0.48860299587249755859375) * N.x)) + (((_236.sh_coefficients[4].xyz * 1.09254801273345947265625) * N.x) * N.y)) + (((_236.sh_coefficients[5].xyz * 1.09254801273345947265625) * N.y) * N.z)) + ((_236.sh_coefficients[6].xyz * 0.3153919875621795654296875) * (((3.0 * N.z) * N.z) - 1.0))) + (((_236.sh_coefficients[7].xyz * 1.09254801273345947265625) * N.x) * N.z)) + ((_236.sh_coefficients[8].xyz * 0.546274006366729736328125) * ((N.x * N.x) - (N.y * N.y)));
    vec3 result = _346;
    return max(result, vec3(0.0));
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + ((max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0));
}

vec3 SampleIBLSpecular(vec3 N, vec3 V, float roughness, vec3 F0)
{
    vec3 R = reflect(-V, N);
    float NdotV = max(dot(N, V), 0.0);
    float param = NdotV;
    vec3 param_1 = F0;
    float param_2 = roughness;
    vec3 F = FresnelSchlickRoughness(param, param_1, param_2);
    vec3 prefiltered = textureLod(u_reflection_cubemap, R, roughness * 4.0).xyz;
    vec2 brdf = texture(u_brdf_lut, vec2(NdotV, roughness)).xy;
    return prefiltered * ((F * brdf.x) + vec3(brdf.y));
}

void main()
{
    vec2 finalUV = vTexCoord;
    bool _1263 = _1260.extra_params2.x > 0.0;
    bool _1269;
    if (_1263)
    {
        _1269 = _1260.flags.x != 0.0;
    }
    else
    {
        _1269 = _1263;
    }
    if (_1269)
    {
        vec3 viewDirTS = transpose(vTBN) * normalize(_1280.camera_pos.xyz - vFragPos);
        vec2 param = vTexCoord;
        vec3 param_1 = viewDirTS;
        float param_2 = _1260.extra_params2.x;
        finalUV = ParallaxOcclusionMapping(param, param_1, param_2);
    }
    vec4 texColor;
    if (_1300.u_splat_enabled > 0.5)
    {
        vec4 w = texture(u_splat_weight_map, finalUV);
        float w_sum = ((w.x + w.y) + w.z) + w.w;
        if (w_sum > 0.001000000047497451305389404296875)
        {
            w /= vec4(w_sum);
        }
        vec3 c0 = texture(u_splat_layer0, finalUV * _1300.u_splat_tiling.x).xyz;
        vec3 c1 = texture(u_splat_layer1, finalUV * _1300.u_splat_tiling.y).xyz;
        vec3 c2 = texture(u_splat_layer2, finalUV * _1300.u_splat_tiling.z).xyz;
        vec3 c3 = texture(u_splat_layer3, finalUV * _1300.u_splat_tiling.w).xyz;
        texColor = vec4((((c0 * w.x) + (c1 * w.y)) + (c2 * w.z)) + (c3 * w.w), 1.0) * vColor;
    }
    else
    {
        texColor = texture(u_texture, finalUV) * vColor;
    }
    bool _1403 = _1260.emissive.w != 0.0;
    bool _1412;
    if (_1403)
    {
        _1412 = texColor.w < clamp(_1260.roughness_ao.w, 0.0, 1.0);
    }
    else
    {
        _1412 = _1403;
    }
    if (_1412)
    {
        discard;
    }
    vec3 N = vNormal;
    if (_1260.flags.x != 0.0)
    {
        vec3 normalMap = (texture(u_normal_map, finalUV).xyz * 2.0) - vec3(1.0);
        vec3 _1435 = normalMap;
        vec2 _1437 = _1435.xy * _1260.roughness_ao.z;
        normalMap.x = _1437.x;
        normalMap.y = _1437.y;
        N = normalize(vTBN * normalMap);
    }
    if (!(_834.light_dir_and_enabled.w != 0.0))
    {
        vec3 result = (texColor.xyz * vColor.xyz) * _1260.albedo.xyz;
        if (_1260.flags.z != 0.0)
        {
            result += (texture(u_emissive_map, finalUV).xyz * _1260.emissive.xyz);
        }
        vec3 param_3 = result;
        float param_4 = texColor.w * vColor.w;
        OutputFragment(param_3, param_4);
        return;
    }
    if (_834.light_params.w == 2.0)
    {
        vec3 L = normalize(-_834.light_dir_and_enabled.xyz);
        vec3 V_hl = normalize(_1280.camera_pos.xyz - vFragPos);
        vec3 R = reflect(_834.light_dir_and_enabled.xyz, N);
        float half_lambert = (dot(N, L) * 0.5) + 0.5;
        vec3 diffuse_color = ((texColor.xyz * vColor.xyz) * _1260.albedo.xyz) * half_lambert;
        float spec_brightness = pow(max(dot(R, V_hl), 0.0), 100.0);
        vec3 _1541;
        if (_1260.flags.y != 0.0)
        {
            _1541 = texture(u_metallic_roughness_map, finalUV).xyz;
        }
        else
        {
            _1541 = vec3(0.0);
        }
        vec3 spec_tex = _1541;
        vec3 specular_color = spec_tex * spec_brightness;
        vec3 param_5 = vFragPos;
        vec3 param_6 = vFragPosViewSpace;
        vec3 param_7 = N;
        vec3 param_8 = L;
        float shadow = ShadowCalculation(param_5, param_6, param_7, param_8);
        float shadow_multiplier = 1.0 - (shadow * 0.5);
        vec3 color = (diffuse_color + specular_color) * shadow_multiplier;
        vec3 param_9 = color;
        float param_10 = 1.0;
        OutputFragment(param_9, param_10);
        return;
    }
    if (_834.light_params.w == 4.0)
    {
        vec3 L_1 = normalize(-_834.light_dir_and_enabled.xyz);
        vec3 V_tn = normalize(_1280.camera_pos.xyz - vFragPos);
        vec3 H = normalize(L_1 + V_tn);
        float NdotL = (dot(N, L_1) * 0.5) + 0.5;
        float band1 = smoothstep(_1260.toon_shadow_color.w - _1260.toon_params.x, _1260.toon_shadow_color.w + _1260.toon_params.x, NdotL);
        float band2 = smoothstep(0.699999988079071044921875 - _1260.toon_params.x, 0.699999988079071044921875 + _1260.toon_params.x, NdotL);
        float cel = (band1 * 0.699999988079071044921875) + (band2 * 0.300000011920928955078125);
        vec3 baseColor = (texColor.xyz * vColor.xyz) * _1260.albedo.xyz;
        vec3 shadowColor = baseColor * _1260.toon_shadow_color.xyz;
        vec3 param_11 = vFragPos;
        vec3 param_12 = vFragPosViewSpace;
        vec3 param_13 = N;
        vec3 param_14 = L_1;
        float shadow_1 = ShadowCalculation(param_11, param_12, param_13, param_14);
        vec3 diffuse = mix(shadowColor, baseColor * _834.light_color_and_ambient.xyz, vec3(cel)) * (1.0 - shadow_1);
        float NdotH = max(dot(N, H), 0.0);
        float spec = step(_1260.toon_params.y, NdotH) * _1260.toon_params.z;
        vec3 specular = (_834.light_color_and_ambient.xyz * spec) * (1.0 - shadow_1);
        float rim = pow(1.0 - max(dot(N, V_tn), 0.0), 4.0) * _1260.toon_params.w;
        vec3 color_1 = (diffuse + specular) + vec3(rim);
        vec3 param_15 = color_1;
        float param_16 = texColor.w * vColor.w;
        OutputFragment(param_15, param_16);
        return;
    }
    if (_834.light_params.w == 5.0)
    {
        float wc_paper = _1260.toon_shadow_color.x;
        float wc_edge = _1260.toon_shadow_color.y;
        float wc_bleed = _1260.toon_shadow_color.z;
        float wc_pigment = max(_1260.toon_shadow_color.w, 0.100000001490116119384765625);
        vec3 L_2 = normalize(-_834.light_dir_and_enabled.xyz);
        vec3 V_wc = normalize(_1280.camera_pos.xyz - vFragPos);
        float NdotL_1 = (dot(N, L_2) * 0.5) + 0.5;
        vec3 baseColor_1 = (texColor.xyz * vColor.xyz) * _1260.albedo.xyz;
        float soft_band = smoothstep(0.25, 0.550000011920928955078125, NdotL_1);
        vec3 param_17 = vFragPos;
        vec3 param_18 = vFragPosViewSpace;
        vec3 param_19 = N;
        vec3 param_20 = L_2;
        float shadow_2 = ShadowCalculation(param_17, param_18, param_19, param_20);
        vec3 lit = (baseColor_1 * _834.light_color_and_ambient.xyz) * _834.light_params.x;
        vec3 shade = (baseColor_1 * vec3(0.449999988079071044921875, 0.4000000059604644775390625, 0.5)) * _834.light_color_and_ambient.w;
        vec3 diffuse_1 = mix(shade, lit, vec3(soft_band)) * (1.0 - (shadow_2 * 0.60000002384185791015625));
        float fresnel = 1.0 - max(dot(N, V_wc), 0.0);
        float edge_factor = pow(fresnel, 3.0) * wc_edge;
        diffuse_1 *= (1.0 - (edge_factor * 0.5));
        float paper_noise = fract(sin(dot(gl_FragCoord.xy * 0.00999999977648258209228515625, vec2(12.98980045318603515625, 78.233001708984375))) * 43758.546875);
        paper_noise = (paper_noise * 0.5) + 0.5;
        diffuse_1 = mix(diffuse_1, diffuse_1 * paper_noise, vec3(wc_paper * 0.300000011920928955078125));
        vec3 warm_shift = vec3(0.02999999932944774627685546875, -0.00999999977648258209228515625, -0.02999999932944774627685546875) * wc_bleed;
        diffuse_1 += (warm_shift * (1.0 - soft_band));
        diffuse_1 = pow(diffuse_1, vec3(1.0 / wc_pigment));
        vec3 param_21 = diffuse_1;
        float param_22 = texColor.w * vColor.w;
        OutputFragment(param_21, param_22);
        return;
    }
    if (_834.light_params.w == 3.0)
    {
        vec3 L_3 = normalize(-_834.light_dir_and_enabled.xyz);
        vec3 V_st = normalize(_1280.camera_pos.xyz - vFragPos);
        vec3 R_1 = reflect(_834.light_dir_and_enabled.xyz, N);
        float half_lambert_1 = (dot(N, L_3) * 0.5) + 0.5;
        vec3 diffuse_2 = ((_1260.albedo.xyz * half_lambert_1) * _834.light_color_and_ambient.xyz) * _834.light_params.x;
        float spec_power = max(_1260.roughness_ao.x, 1.0);
        vec3 spec_color = vec3(_1260.albedo.w);
        vec3 specular_1 = spec_color * pow(max(dot(R_1, V_st), 0.0), spec_power);
        vec3 emissive_val = _1260.emissive.xyz;
        vec3 material_color = (diffuse_2 + specular_1) + emissive_val;
        vec3 color_st = (material_color * texColor.xyz) * vColor.xyz;
        vec3 param_23 = vFragPos;
        vec3 param_24 = vFragPosViewSpace;
        vec3 param_25 = N;
        vec3 param_26 = L_3;
        float shadow_3 = ShadowCalculation(param_23, param_24, param_25, param_26);
        float shadow_multiplier_1 = 1.0 - (shadow_3 * 0.5);
        vec3 param_27 = color_st * shadow_multiplier_1;
        float param_28 = texColor.w * vColor.w;
        OutputFragment(param_27, param_28);
        return;
    }
    vec3 surface_albedo = pow((texColor.xyz * vColor.xyz) * _1260.albedo.xyz, vec3(2.2000000476837158203125));
    float metallic = clamp(_1260.albedo.w, 0.0, 1.0);
    float roughness = clamp(_1260.roughness_ao.x, 0.039999999105930328369140625, 1.0);
    float ao = max(_1260.roughness_ao.y, 0.0);
    vec3 surface_emissive = _1260.emissive.xyz;
    if (_1260.flags.y != 0.0)
    {
        vec4 mrSample = texture(u_metallic_roughness_map, finalUV);
        roughness = clamp(mrSample.y * _1260.roughness_ao.x, 0.039999999105930328369140625, 1.0);
        metallic = clamp(mrSample.z * _1260.albedo.w, 0.0, 1.0);
    }
    if (_1260.flags.w != 0.0)
    {
        ao *= texture(u_occlusion_map, finalUV).x;
    }
    if (_1260.flags.z != 0.0)
    {
        surface_emissive *= texture(u_emissive_map, finalUV).xyz;
    }
    vec3 V = normalize(_1280.camera_pos.xyz - vFragPos);
    vec3 F0 = vec3(0.039999999105930328369140625);
    F0 = mix(F0, surface_albedo, vec3(metallic));
    vec3 T = normalize(vTBN[0]);
    vec3 B = normalize(vTBN[1]);
    vec3 Lo = vec3(0.0);
    vec3 L_4 = normalize(-_834.light_dir_and_enabled.xyz);
    vec3 H_1 = normalize(V + L_4);
    float _2103;
    if (_1260.extra_params.w != 0.0)
    {
        vec3 param_29 = N;
        vec3 param_30 = H_1;
        vec3 param_31 = T;
        vec3 param_32 = B;
        float param_33 = roughness;
        float param_34 = _1260.extra_params.w;
        _2103 = DistributionGGXAniso(param_29, param_30, param_31, param_32, param_33, param_34);
    }
    else
    {
        vec3 param_35 = N;
        vec3 param_36 = H_1;
        float param_37 = roughness;
        _2103 = DistributionGGX(param_35, param_36, param_37);
    }
    float NDF = _2103;
    vec3 param_38 = N;
    vec3 param_39 = V;
    vec3 param_40 = L_4;
    float param_41 = roughness;
    float G = GeometrySmith(param_38, param_39, param_40, param_41);
    float param_42 = max(dot(H_1, V), 0.0);
    vec3 param_43 = F0;
    vec3 F = fresnelSchlick(param_42, param_43);
    vec3 numerator = F * (NDF * G);
    float denominator = ((4.0 * max(dot(N, V), 0.0)) * max(dot(N, L_4), 0.0)) + 9.9999997473787516355514526367188e-05;
    vec3 specular_2 = numerator / vec3(denominator);
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= (1.0 - metallic);
    float NdotL_2 = max(dot(N, L_4), 0.0);
    vec3 param_44 = vFragPos;
    vec3 param_45 = vFragPosViewSpace;
    vec3 param_46 = N;
    vec3 param_47 = L_4;
    float shadow_4 = ShadowCalculation(param_44, param_45, param_46, param_47);
    Lo += (((((((kD * surface_albedo) / vec3(3.1415927410125732421875)) + specular_2) * _834.light_color_and_ambient.xyz) * _834.light_params.x) * NdotL_2) * (1.0 - shadow_4));
    if (_1260.extra_params.x > 0.0)
    {
        vec3 param_48 = N;
        vec3 param_49 = L_4;
        vec3 param_50 = surface_albedo;
        float param_51 = _1260.extra_params.x;
        vec3 param_52 = _834.light_color_and_ambient.xyz;
        float param_53 = _834.light_params.x;
        vec3 param_54 = _1260.extra_params2.yzw;
        Lo += (SubsurfaceScattering(param_48, param_49, param_50, param_51, param_52, param_53, param_54) * (1.0 - shadow_4));
    }
    if (_1260.extra_params.y > 0.0)
    {
        float cc_r = max(_1260.extra_params.z, 0.039999999105930328369140625);
        vec3 param_55 = N;
        vec3 param_56 = H_1;
        float param_57 = cc_r;
        float NDF_cc = DistributionGGX(param_55, param_56, param_57);
        vec3 param_58 = N;
        vec3 param_59 = V;
        vec3 param_60 = L_4;
        float param_61 = cc_r;
        float G_cc = GeometrySmith(param_58, param_59, param_60, param_61);
        float param_62 = max(dot(H_1, V), 0.0);
        vec3 param_63 = vec3(0.039999999105930328369140625);
        vec3 F_cc = fresnelSchlick(param_62, param_63);
        vec3 spec_cc = (F_cc * (NDF_cc * G_cc)) / vec3(((4.0 * max(dot(N, V), 0.0)) * max(dot(N, L_4), 0.0)) + 9.9999997473787516355514526367188e-05);
        Lo += (((((spec_cc * _1260.extra_params.y) * NdotL_2) * _834.light_color_and_ambient.xyz) * _834.light_params.x) * (1.0 - shadow_4));
    }
    int cl_tx = int(gl_FragCoord.x) / 16;
    int cl_ty = int(gl_FragCoord.y) / 16;
    float cl_linear_z = max(-vFragPosViewSpace.z, 9.9999997473787516355514526367188e-05);
    float cl_log_ratio = log(_2340.cluster_far / max(_2340.cluster_near, 9.9999997473787516355514526367188e-05));
    int _2351;
    if (cl_log_ratio > 0.0)
    {
        _2351 = clamp(int((log(cl_linear_z / max(_2340.cluster_near, 9.9999997473787516355514526367188e-05)) / cl_log_ratio) * float(_2340.cluster_z_slices)), 0, int(_2340.cluster_z_slices) - 1);
    }
    else
    {
        _2351 = 0;
    }
    int cl_tz = _2351;
    int cl_idx = (((cl_tz * int(_2340.cluster_tiles_y)) + cl_ty) * int(_2340.cluster_tiles_x)) + cl_tx;
    int cl_total = (int(_2340.cluster_tiles_x) * int(_2340.cluster_tiles_y)) * int(_2340.cluster_z_slices);
    cl_idx = clamp(cl_idx, 0, max((cl_total - 1), 0));
    uint cl_offset = _2340.cluster_infos[cl_idx].offset;
    uint cl_point_count = _2340.cluster_infos[cl_idx].point_count;
    uint cl_spot_count = _2340.cluster_infos[cl_idx].spot_count;
    for (uint ci = 0u; ci < cl_point_count; ci++)
    {
        int i = int(_2432.light_indices[cl_offset + ci]);
        if (i >= _2444.u_point_light_count)
        {
            continue;
        }
        vec3 L_5 = normalize(_2444.u_point_lights[i].position - vFragPos);
        vec3 H_2 = normalize(V + L_5);
        float _distance = length(_2444.u_point_lights[i].position - vFragPos);
        float attenuation = clamp(1.0 - ((_distance * _distance) / (_2444.u_point_lights[i].radius * _2444.u_point_lights[i].radius)), 0.0, 1.0);
        attenuation *= attenuation;
        vec3 radiance = (_2444.u_point_lights[i].color * _2444.u_point_lights[i].intensity) * attenuation;
        vec3 param_64 = N;
        vec3 param_65 = H_2;
        float param_66 = roughness;
        float NDF_1 = DistributionGGX(param_64, param_65, param_66);
        vec3 param_67 = N;
        vec3 param_68 = V;
        vec3 param_69 = L_5;
        float param_70 = roughness;
        float G_1 = GeometrySmith(param_67, param_68, param_69, param_70);
        float param_71 = max(dot(H_2, V), 0.0);
        vec3 param_72 = F0;
        vec3 F_1 = fresnelSchlick(param_71, param_72);
        vec3 numerator_1 = F_1 * (NDF_1 * G_1);
        float denominator_1 = ((4.0 * max(dot(N, V), 0.0)) * max(dot(N, L_5), 0.0)) + 9.9999997473787516355514526367188e-05;
        vec3 specular_3 = numerator_1 / vec3(denominator_1);
        vec3 kS_1 = F_1;
        vec3 kD_1 = vec3(1.0) - kS_1;
        kD_1 *= (1.0 - metallic);
        float NdotL_3 = max(dot(N, L_5), 0.0);
        float point_shadow = 0.0;
        if (_2444.u_point_lights[i].cast_shadow != 0)
        {
            int param_73 = _2444.u_point_lights[i].shadow_index;
            vec3 param_74 = vFragPos;
            vec3 param_75 = _2444.u_point_lights[i].position;
            float param_76 = _2444.u_point_lights[i].radius;
            point_shadow = PointShadowCalculation(param_73, param_74, param_75, param_76);
        }
        Lo += ((((((kD_1 * surface_albedo) / vec3(3.1415927410125732421875)) + specular_3) * radiance) * NdotL_3) * (1.0 - point_shadow));
    }
    for (uint si = 0u; si < cl_spot_count; si++)
    {
        int i_1 = int(_2432.light_indices[(cl_offset + cl_point_count) + si]);
        if (i_1 >= _2626.u_spot_light_count)
        {
            continue;
        }
        vec3 L_6 = normalize(_2626.u_spot_lights[i_1].position - vFragPos);
        vec3 H_3 = normalize(V + L_6);
        float _distance_1 = length(_2626.u_spot_lights[i_1].position - vFragPos);
        float attenuation_1 = clamp(1.0 - ((_distance_1 * _distance_1) / (_2626.u_spot_lights[i_1].radius * _2626.u_spot_lights[i_1].radius)), 0.0, 1.0);
        attenuation_1 *= attenuation_1;
        vec3 spotDir = normalize(-_2626.u_spot_lights[i_1].direction);
        float theta = dot(L_6, spotDir);
        float outerCos = cos(radians(_2626.u_spot_lights[i_1].outer_cone));
        float innerCos = cos(radians(_2626.u_spot_lights[i_1].inner_cone));
        float epsilon = max(innerCos - outerCos, 9.9999997473787516355514526367188e-05);
        float cone = clamp((theta - outerCos) / epsilon, 0.0, 1.0);
        if (cone <= 0.0)
        {
            continue;
        }
        vec3 radiance_1 = ((_2626.u_spot_lights[i_1].color * _2626.u_spot_lights[i_1].intensity) * attenuation_1) * cone;
        vec3 param_77 = N;
        vec3 param_78 = H_3;
        float param_79 = roughness;
        float NDF_2 = DistributionGGX(param_77, param_78, param_79);
        vec3 param_80 = N;
        vec3 param_81 = V;
        vec3 param_82 = L_6;
        float param_83 = roughness;
        float G_2 = GeometrySmith(param_80, param_81, param_82, param_83);
        float param_84 = max(dot(H_3, V), 0.0);
        vec3 param_85 = F0;
        vec3 F_2 = fresnelSchlick(param_84, param_85);
        vec3 numerator_2 = F_2 * (NDF_2 * G_2);
        float denominator_2 = ((4.0 * max(dot(N, V), 0.0)) * max(dot(N, L_6), 0.0)) + 9.9999997473787516355514526367188e-05;
        vec3 specular_4 = numerator_2 / vec3(denominator_2);
        vec3 kS_2 = F_2;
        vec3 kD_2 = vec3(1.0) - kS_2;
        kD_2 *= (1.0 - metallic);
        float NdotL_4 = max(dot(N, L_6), 0.0);
        float spot_shadow = 0.0;
        if (_2626.u_spot_lights[i_1].cast_shadow != 0)
        {
            int param_86 = _2626.u_spot_lights[i_1].shadow_index;
            vec3 param_87 = vFragPos;
            vec3 param_88 = N;
            vec3 param_89 = L_6;
            spot_shadow = SpotShadowCalculation(param_86, param_87, param_88, param_89);
        }
        Lo += ((((((kD_2 * surface_albedo) / vec3(3.1415927410125732421875)) + specular_4) * radiance_1) * NdotL_4) * (1.0 - spot_shadow));
    }
    float param_90 = max(dot(N, V), 0.0);
    vec3 param_91 = F0;
    vec3 F_3 = fresnelSchlick(param_90, param_91);
    vec3 kS_ambient = F_3;
    vec3 kD_ambient = vec3(1.0) - kS_ambient;
    kD_ambient *= (1.0 - metallic);
    vec3 _2843;
    if (_236.probe_params.x > 0.5)
    {
        vec3 param_92 = N;
        _2843 = EvaluateSH(param_92);
    }
    else
    {
        _2843 = vec3(_834.light_color_and_ambient.w);
    }
    vec3 irradiance = _2843;
    vec3 diffuse_ambient = (kD_ambient * irradiance) * surface_albedo;
    vec3 _2864;
    if (_236.probe_params.y > 0.5)
    {
        vec3 param_93 = N;
        vec3 param_94 = V;
        float param_95 = roughness;
        vec3 param_96 = F0;
        _2864 = SampleIBLSpecular(param_93, param_94, param_95, param_96);
    }
    else
    {
        _2864 = (irradiance * F0) * (1.0 - roughness);
    }
    vec3 specular_ambient = _2864;
    vec3 ambient = (diffuse_ambient + specular_ambient) * ao;
    if (_1260.extra_params.y > 0.0)
    {
        float param_97 = max(dot(N, V), 0.0);
        vec3 param_98 = vec3(0.039999999105930328369140625);
        vec3 F_cc_amb = fresnelSchlick(param_97, param_98);
        ambient += ((((F_cc_amb * _1260.extra_params.y) * irradiance) * (1.0 - _1260.extra_params.z)) * 0.25);
    }
    vec3 color_2 = (ambient + Lo) + surface_emissive;
    vec3 param_99 = color_2;
    float param_100 = texColor.w * vColor.w;
    OutputFragment(param_99, param_100);
}

