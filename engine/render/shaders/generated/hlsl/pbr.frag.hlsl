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

static const float2 _676[16] = { float2(-0.94201624393463134765625f, -0.39906215667724609375f), float2(0.94558608531951904296875f, -0.768907248973846435546875f), float2(-0.094184100627899169921875f, -0.929388701915740966796875f), float2(0.34495937824249267578125f, 0.29387760162353515625f), float2(-0.91588580608367919921875f, 0.4577143192291259765625f), float2(-0.8154423236846923828125f, -0.87912464141845703125f), float2(-0.38277542591094970703125f, 0.2767684459686279296875f), float2(0.9748439788818359375f, 0.7564837932586669921875f), float2(0.4432332515716552734375f, -0.9751155376434326171875f), float2(0.5374298095703125f, -0.473734200000762939453125f), float2(-0.2649691104888916015625f, -0.418930232524871826171875f), float2(0.79197514057159423828125f, 0.19090187549591064453125f), float2(-0.24188840389251708984375f, 0.997065067291259765625f), float2(-0.8140995502471923828125f, 0.91437590122222900390625f), float2(0.1998412609100341796875f, 0.786413669586181640625f), float2(0.14383161067962646484375f, -0.141007900238037109375f) };

cbuffer LightProbeData : register(b2)
{
    float4 _236_sh_coefficients[9] : packoffset(c0);
    float4 _236_probe_params : packoffset(c9);
};

cbuffer PerScene : register(b1)
{
    float4 _834_light_dir_and_enabled : packoffset(c0);
    float4 _834_light_color_and_ambient : packoffset(c1);
    float4 _834_light_params : packoffset(c2);
    float4 _834_cascade_splits : packoffset(c3);
    row_major float4x4 _834_light_space_matrices[3] : packoffset(c4);
};

cbuffer SpotLightData : register(b4)
{
    row_major float4x4 _1020_u_spot_light_space_matrices[4] : packoffset(c0);
};

cbuffer PerMaterial : register(b3)
{
    float4 _1260_albedo : packoffset(c0);
    float4 _1260_roughness_ao : packoffset(c1);
    float4 _1260_emissive : packoffset(c2);
    float4 _1260_flags : packoffset(c3);
    float4 _1260_extra_params : packoffset(c4);
    float4 _1260_extra_params2 : packoffset(c5);
    float4 _1260_toon_shadow_color : packoffset(c6);
    float4 _1260_toon_params : packoffset(c7);
};

cbuffer PerFrame : register(b0)
{
    row_major float4x4 _1280_vp : packoffset(c0);
    row_major float4x4 _1280_view : packoffset(c4);
    float4 _1280_camera_pos : packoffset(c8);
};

cbuffer TerrainParams : register(b5)
{
    float _1300_u_splat_enabled : packoffset(c0);
    float _1300_tp_pad0 : packoffset(c0.y);
    float _1300_tp_pad1 : packoffset(c0.z);
    float _1300_tp_pad2 : packoffset(c0.w);
    float4 _1300_u_splat_tiling : packoffset(c1);
};

ByteAddressBuffer _2340 : register(t19);
ByteAddressBuffer _2432 : register(t20);
ByteAddressBuffer _2444 : register(t17);
ByteAddressBuffer _2626 : register(t18);
Texture2D<float4> u_normal_map : register(t1);
SamplerState _u_normal_map_sampler : register(s1);
TextureCube<float4> u_reflection_cubemap : register(t7);
SamplerState _u_reflection_cubemap_sampler : register(s7);
Texture2D<float4> u_brdf_lut : register(t8);
SamplerState _u_brdf_lut_sampler : register(s8);
Texture2D<float4> u_shadow_maps[3] : register(t5);
SamplerComparisonState _u_shadow_maps_sampler[3] : register(s5);
Texture2D<float4> u_spot_shadow_maps[4] : register(t6);
SamplerState _u_spot_shadow_maps_sampler[4] : register(s6);
TextureCube<float4> u_point_shadow_maps[4] : register(t14);
SamplerState _u_point_shadow_maps_sampler[4] : register(s14);
Texture2D<float4> u_splat_weight_map : register(t9);
SamplerState _u_splat_weight_map_sampler : register(s9);
Texture2D<float4> u_splat_layer0 : register(t10);
SamplerState _u_splat_layer0_sampler : register(s10);
Texture2D<float4> u_splat_layer1 : register(t11);
SamplerState _u_splat_layer1_sampler : register(s11);
Texture2D<float4> u_splat_layer2 : register(t12);
SamplerState _u_splat_layer2_sampler : register(s12);
Texture2D<float4> u_splat_layer3 : register(t13);
SamplerState _u_splat_layer3_sampler : register(s13);
Texture2D<float4> u_texture : register(t0);
SamplerState _u_texture_sampler : register(s0);
Texture2D<float4> u_emissive_map : register(t3);
SamplerState _u_emissive_map_sampler : register(s3);
Texture2D<float4> u_metallic_roughness_map : register(t2);
SamplerState _u_metallic_roughness_map_sampler : register(s2);
Texture2D<float4> u_occlusion_map : register(t4);
SamplerState _u_occlusion_map_sampler : register(s4);

static float4 gl_FragCoord;
static float4 FragColor;
static float2 vTexCoord;
static float3x3 vTBN;
static float3 vFragPos;
static float4 vColor;
static float3 vNormal;
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

float2 ParallaxOcclusionMapping(float2 uv, float3 viewDirTS, float height_scale)
{
    float layerDepth = 0.0625f;
    float currentLayerDepth = 0.0f;
    float2 P = (viewDirTS.xy / max(viewDirTS.z, 0.001000000047497451305389404296875f).xx) * height_scale;
    float2 deltaUV = P / 16.0f.xx;
    float2 curUV = uv;
    float curDepth = 1.0f - u_normal_map.Sample(_u_normal_map_sampler, curUV).w;
    for (int i = 0; i < 16; i++)
    {
        if (currentLayerDepth >= curDepth)
        {
            break;
        }
        curUV -= deltaUV;
        curDepth = 1.0f - u_normal_map.Sample(_u_normal_map_sampler, curUV).w;
        currentLayerDepth += layerDepth;
    }
    float2 prevUV = curUV + deltaUV;
    float afterDepth = curDepth - currentLayerDepth;
    float beforeDepth = ((1.0f - u_normal_map.Sample(_u_normal_map_sampler, prevUV).w) - currentLayerDepth) + layerDepth;
    float w = afterDepth / ((afterDepth - beforeDepth) + 9.9999997473787516355514526367188e-05f);
    return lerp(curUV, prevUV, w.xx);
}

void OutputFragment(float3 color, float alpha)
{
    if (_834_cascade_splits.w > 0.5f)
    {
        float z = gl_FragCoord.z;
        float w = alpha * max(0.00999999977648258209228515625f, 3000.0f * pow(1.0f - z, 3.0f));
        if (_834_cascade_splits.w < 1.5f)
        {
            FragColor = float4((color * alpha) * w, alpha * w);
        }
        else
        {
            FragColor = float4(0.0f, 0.0f, 0.0f, alpha);
        }
        return;
    }
    FragColor = float4(color, alpha);
}

float FindBlockerDepth(Texture2D<float4> shadowMap, SamplerComparisonState _shadowMap_sampler, float2 uv, float receiverDepth, float searchRadius)
{
    float blockerSum = 0.0f;
    int blockerCount = 0;
    for (int i = 0; i < 16; i++)
    {
        float2 sampleUV = uv + (_676[i] * searchRadius);
        float3 _691 = float3(sampleUV, receiverDepth);
        float vis = shadowMap.SampleCmp(_shadowMap_sampler, _691.xy, _691.z);
        if (vis < 0.5f)
        {
            float lo = 0.0f;
            float hi = receiverDepth;
            for (int b = 0; b < 3; b++)
            {
                float mid = (lo + hi) * 0.5f;
                float3 _719 = float3(sampleUV, mid);
                if (shadowMap.SampleCmp(_shadowMap_sampler, _719.xy, _719.z) < 0.5f)
                {
                    hi = mid;
                }
                else
                {
                    lo = mid;
                }
            }
            blockerSum += ((lo + hi) * 0.5f);
            blockerCount++;
        }
    }
    if (blockerCount == 0)
    {
        return -1.0f;
    }
    return blockerSum / float(blockerCount);
}

float PCSS_Shadow(Texture2D<float4> shadowMap, SamplerComparisonState _shadowMap_sampler, float3 projCoords, float bias)
{
    uint _756_dummy_parameter;
    float2 texelSize = 1.0f.xx / float2(int2(spvTextureSize(shadowMap, uint(0), _756_dummy_parameter)));
    float receiverDepth = projCoords.z - bias;
    float2 param = projCoords.xy;
    float param_1 = receiverDepth;
    float param_2 = 0.008000000379979610443115234375f;
    float avgBlockerDepth = FindBlockerDepth(shadowMap, _shadowMap_sampler, param, param_1, param_2);
    if (avgBlockerDepth < 0.0f)
    {
        return 1.0f;
    }
    float penumbraWidth = (0.0040000001899898052215576171875f * (receiverDepth - avgBlockerDepth)) / max(avgBlockerDepth, 9.9999997473787516355514526367188e-05f);
    float filterRadius = max(penumbraWidth, texelSize.x);
    float shadow = 0.0f;
    for (int i = 0; i < 16; i++)
    {
        float2 offset = _676[i] * filterRadius;
        float3 _817 = float3(projCoords.xy + offset, receiverDepth);
        shadow += shadowMap.SampleCmp(_shadowMap_sampler, _817.xy, _817.z);
    }
    return shadow / 16.0f;
}

float ShadowForCascade(int idx, float3 fragPosWorldSpace, float3 normal, float3 lightDir)
{
    float4 fragPosLightSpace = mul(float4(fragPosWorldSpace, 1.0f), _834_light_space_matrices[idx]);
    float3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w.xxx;
    projCoords = (projCoords * 0.5f) + 0.5f.xxx;
    if (projCoords.z > 1.0f)
    {
        return 0.0f;
    }
    bool _864 = projCoords.x < 0.0f;
    bool _871;
    if (!_864)
    {
        _871 = projCoords.x > 1.0f;
    }
    else
    {
        _871 = _864;
    }
    bool _878;
    if (!_871)
    {
        _878 = projCoords.y < 0.0f;
    }
    else
    {
        _878 = _871;
    }
    bool _885;
    if (!_878)
    {
        _885 = projCoords.y > 1.0f;
    }
    else
    {
        _885 = _878;
    }
    if (_885)
    {
        return 0.0f;
    }
    float bias = max(0.004999999888241291046142578125f * (1.0f - dot(normal, lightDir)), 0.0005000000237487256526947021484375f);
    float3 param = projCoords;
    float param_1 = bias;
    float lit = PCSS_Shadow(u_shadow_maps[idx], _u_shadow_maps_sampler[idx], param, param_1);
    return clamp((1.0f - lit) * _834_light_params.y, 0.0f, 1.0f);
}

float ShadowCalculation(float3 fragPosWorldSpace, float3 fragPosViewSpace, float3 normal, float3 lightDir)
{
    if (!(_834_light_params.z != 0.0f))
    {
        return 0.0f;
    }
    float viewDepth = abs(fragPosViewSpace.z);
    int cascadeIndex = 2;
    for (int i = 0; i < 2; i++)
    {
        if (viewDepth < _834_cascade_splits[uint3(0u, 1u, 2u)[i]])
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
        float splitEnd = _834_cascade_splits[uint3(0u, 1u, 2u)[cascadeIndex]];
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
    return clamp(shadow * _834_light_params.y, 0.0f, 1.0f);
}

float DistributionGGXAniso(float3 N, float3 H, float3 T, float3 B, float roughness, float aniso)
{
    float at = max(roughness * (1.0f + aniso), 0.001000000047497451305389404296875f);
    float ab = max(roughness * (1.0f - aniso), 0.001000000047497451305389404296875f);
    float TdotH = dot(T, H);
    float BdotH = dot(B, H);
    float NdotH = dot(N, H);
    float d = (((TdotH * TdotH) / (at * at)) + ((BdotH * BdotH) / (ab * ab))) + (NdotH * NdotH);
    return 1.0f / (((((3.1415927410125732421875f * at) * ab) * d) * d) + 9.9999997473787516355514526367188e-05f);
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

float3 SubsurfaceScattering(float3 N, float3 L, float3 albedo, float sss, float3 light_col, float li, float3 tint)
{
    float wrap = 0.5f * sss;
    float NdotL_wrap = max(0.0f, (dot(N, L) + wrap) / (1.0f + wrap));
    float NdotL_std = max(dot(N, L), 0.0f);
    float diff = NdotL_wrap - NdotL_std;
    bool3 _535 = (dot(tint, tint) > 0.0f).xxx;
    float3 sss_tint = float3(_535.x ? tint.x : float3(1.0f, 0.3499999940395355224609375f, 0.20000000298023223876953125f).x, _535.y ? tint.y : float3(1.0f, 0.3499999940395355224609375f, 0.20000000298023223876953125f).y, _535.z ? tint.z : float3(1.0f, 0.3499999940395355224609375f, 0.20000000298023223876953125f).z);
    return (((albedo * sss_tint) * diff) * light_col) * li;
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
    float _1194;
    if ((currentDepth - bias) > closestDepth)
    {
        _1194 = _834_light_params.y;
    }
    else
    {
        _1194 = 0.0f;
    }
    return _1194;
}

float SpotShadowCalculation(int shadowIndex, float3 fragPosWorldSpace, float3 normal, float3 lightDir)
{
    if ((shadowIndex < 0) || (shadowIndex >= 4))
    {
        return 0.0f;
    }
    float4 fragPosLightSpace = mul(float4(fragPosWorldSpace, 1.0f), _1020_u_spot_light_space_matrices[shadowIndex]);
    float3 projCoords = fragPosLightSpace.xyz / max(fragPosLightSpace.w, 9.9999997473787516355514526367188e-05f).xxx;
    projCoords = (projCoords * 0.5f) + 0.5f.xxx;
    if (projCoords.z > 1.0f)
    {
        return 0.0f;
    }
    bool _1050 = projCoords.x < 0.0f;
    bool _1057;
    if (!_1050)
    {
        _1057 = projCoords.x > 1.0f;
    }
    else
    {
        _1057 = _1050;
    }
    bool _1064;
    if (!_1057)
    {
        _1064 = projCoords.y < 0.0f;
    }
    else
    {
        _1064 = _1057;
    }
    bool _1071;
    if (!_1064)
    {
        _1071 = projCoords.y > 1.0f;
    }
    else
    {
        _1071 = _1064;
    }
    if (_1071)
    {
        return 0.0f;
    }
    float currentDepth = projCoords.z;
    float bias = max(0.0030000000260770320892333984375f * (1.0f - dot(normal, lightDir)), 0.0005000000237487256526947021484375f);
    float shadow = 0.0f;
    uint _1095_dummy_parameter;
    float2 texelSize = 1.0f.xx / float2(int2(spvTextureSize(u_spot_shadow_maps[shadowIndex], uint(0), _1095_dummy_parameter)));
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            float pcfDepth = u_spot_shadow_maps[shadowIndex].Sample(_u_spot_shadow_maps_sampler[shadowIndex], projCoords.xy + (float2(float(x), float(y)) * texelSize)).x;
            shadow += float((currentDepth - bias) > pcfDepth);
        }
    }
    shadow /= 9.0f;
    return clamp(shadow * _834_light_params.y, 0.0f, 1.0f);
}

float3 EvaluateSH(float3 N)
{
    float3 _346 = ((((((((_236_sh_coefficients[0].xyz * 0.2820949852466583251953125f) + ((_236_sh_coefficients[1].xyz * 0.48860299587249755859375f) * N.y)) + ((_236_sh_coefficients[2].xyz * 0.48860299587249755859375f) * N.z)) + ((_236_sh_coefficients[3].xyz * 0.48860299587249755859375f) * N.x)) + (((_236_sh_coefficients[4].xyz * 1.09254801273345947265625f) * N.x) * N.y)) + (((_236_sh_coefficients[5].xyz * 1.09254801273345947265625f) * N.y) * N.z)) + ((_236_sh_coefficients[6].xyz * 0.3153919875621795654296875f) * (((3.0f * N.z) * N.z) - 1.0f))) + (((_236_sh_coefficients[7].xyz * 1.09254801273345947265625f) * N.x) * N.z)) + ((_236_sh_coefficients[8].xyz * 0.546274006366729736328125f) * ((N.x * N.x) - (N.y * N.y)));
    float3 result = _346;
    return max(result, 0.0f.xxx);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + ((max((1.0f - roughness).xxx, F0) - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f));
}

float3 SampleIBLSpecular(float3 N, float3 V, float roughness, float3 F0)
{
    float3 R = reflect(-V, N);
    float NdotV = max(dot(N, V), 0.0f);
    float param = NdotV;
    float3 param_1 = F0;
    float param_2 = roughness;
    float3 F = FresnelSchlickRoughness(param, param_1, param_2);
    float3 prefiltered = u_reflection_cubemap.SampleLevel(_u_reflection_cubemap_sampler, R, roughness * 4.0f).xyz;
    float2 brdf = u_brdf_lut.Sample(_u_brdf_lut_sampler, float2(NdotV, roughness)).xy;
    return prefiltered * ((F * brdf.x) + brdf.y.xxx);
}

void frag_main()
{
    float2 finalUV = vTexCoord;
    bool _1263 = _1260_extra_params2.x > 0.0f;
    bool _1269;
    if (_1263)
    {
        _1269 = _1260_flags.x != 0.0f;
    }
    else
    {
        _1269 = _1263;
    }
    if (_1269)
    {
        float3 viewDirTS = mul(normalize(_1280_camera_pos.xyz - vFragPos), transpose(vTBN));
        float2 param = vTexCoord;
        float3 param_1 = viewDirTS;
        float param_2 = _1260_extra_params2.x;
        finalUV = ParallaxOcclusionMapping(param, param_1, param_2);
    }
    float4 texColor;
    if (_1300_u_splat_enabled > 0.5f)
    {
        float4 w = u_splat_weight_map.Sample(_u_splat_weight_map_sampler, finalUV);
        float w_sum = ((w.x + w.y) + w.z) + w.w;
        if (w_sum > 0.001000000047497451305389404296875f)
        {
            w /= w_sum.xxxx;
        }
        float3 c0 = u_splat_layer0.Sample(_u_splat_layer0_sampler, finalUV * _1300_u_splat_tiling.x).xyz;
        float3 c1 = u_splat_layer1.Sample(_u_splat_layer1_sampler, finalUV * _1300_u_splat_tiling.y).xyz;
        float3 c2 = u_splat_layer2.Sample(_u_splat_layer2_sampler, finalUV * _1300_u_splat_tiling.z).xyz;
        float3 c3 = u_splat_layer3.Sample(_u_splat_layer3_sampler, finalUV * _1300_u_splat_tiling.w).xyz;
        texColor = float4((((c0 * w.x) + (c1 * w.y)) + (c2 * w.z)) + (c3 * w.w), 1.0f) * vColor;
    }
    else
    {
        texColor = u_texture.Sample(_u_texture_sampler, finalUV) * vColor;
    }
    bool _1403 = _1260_emissive.w != 0.0f;
    bool _1412;
    if (_1403)
    {
        _1412 = texColor.w < clamp(_1260_roughness_ao.w, 0.0f, 1.0f);
    }
    else
    {
        _1412 = _1403;
    }
    if (_1412)
    {
        discard;
    }
    float3 N = vNormal;
    if (_1260_flags.x != 0.0f)
    {
        float3 normalMap = (u_normal_map.Sample(_u_normal_map_sampler, finalUV).xyz * 2.0f) - 1.0f.xxx;
        float3 _1435 = normalMap;
        float2 _1437 = _1435.xy * _1260_roughness_ao.z;
        normalMap.x = _1437.x;
        normalMap.y = _1437.y;
        N = normalize(mul(normalMap, vTBN));
    }
    if (!(_834_light_dir_and_enabled.w != 0.0f))
    {
        float3 result = (texColor.xyz * vColor.xyz) * _1260_albedo.xyz;
        if (_1260_flags.z != 0.0f)
        {
            result += (u_emissive_map.Sample(_u_emissive_map_sampler, finalUV).xyz * _1260_emissive.xyz);
        }
        float3 param_3 = result;
        float param_4 = texColor.w * vColor.w;
        OutputFragment(param_3, param_4);
        return;
    }
    if (_834_light_params.w == 2.0f)
    {
        float3 L = normalize(-_834_light_dir_and_enabled.xyz);
        float3 V_hl = normalize(_1280_camera_pos.xyz - vFragPos);
        float3 R = reflect(_834_light_dir_and_enabled.xyz, N);
        float half_lambert = (dot(N, L) * 0.5f) + 0.5f;
        float3 diffuse_color = ((texColor.xyz * vColor.xyz) * _1260_albedo.xyz) * half_lambert;
        float spec_brightness = pow(max(dot(R, V_hl), 0.0f), 100.0f);
        float3 _1541;
        if (_1260_flags.y != 0.0f)
        {
            _1541 = u_metallic_roughness_map.Sample(_u_metallic_roughness_map_sampler, finalUV).xyz;
        }
        else
        {
            _1541 = 0.0f.xxx;
        }
        float3 spec_tex = _1541;
        float3 specular_color = spec_tex * spec_brightness;
        float3 param_5 = vFragPos;
        float3 param_6 = vFragPosViewSpace;
        float3 param_7 = N;
        float3 param_8 = L;
        float shadow = ShadowCalculation(param_5, param_6, param_7, param_8);
        float shadow_multiplier = 1.0f - (shadow * 0.5f);
        float3 color = (diffuse_color + specular_color) * shadow_multiplier;
        float3 param_9 = color;
        float param_10 = 1.0f;
        OutputFragment(param_9, param_10);
        return;
    }
    if (_834_light_params.w == 4.0f)
    {
        float3 L_1 = normalize(-_834_light_dir_and_enabled.xyz);
        float3 V_tn = normalize(_1280_camera_pos.xyz - vFragPos);
        float3 H = normalize(L_1 + V_tn);
        float NdotL = (dot(N, L_1) * 0.5f) + 0.5f;
        float band1 = smoothstep(_1260_toon_shadow_color.w - _1260_toon_params.x, _1260_toon_shadow_color.w + _1260_toon_params.x, NdotL);
        float band2 = smoothstep(0.699999988079071044921875f - _1260_toon_params.x, 0.699999988079071044921875f + _1260_toon_params.x, NdotL);
        float cel = (band1 * 0.699999988079071044921875f) + (band2 * 0.300000011920928955078125f);
        float3 baseColor = (texColor.xyz * vColor.xyz) * _1260_albedo.xyz;
        float3 shadowColor = baseColor * _1260_toon_shadow_color.xyz;
        float3 param_11 = vFragPos;
        float3 param_12 = vFragPosViewSpace;
        float3 param_13 = N;
        float3 param_14 = L_1;
        float shadow_1 = ShadowCalculation(param_11, param_12, param_13, param_14);
        float3 diffuse = lerp(shadowColor, baseColor * _834_light_color_and_ambient.xyz, cel.xxx) * (1.0f - shadow_1);
        float NdotH = max(dot(N, H), 0.0f);
        float spec = step(_1260_toon_params.y, NdotH) * _1260_toon_params.z;
        float3 specular = (_834_light_color_and_ambient.xyz * spec) * (1.0f - shadow_1);
        float rim = pow(1.0f - max(dot(N, V_tn), 0.0f), 4.0f) * _1260_toon_params.w;
        float3 color_1 = (diffuse + specular) + rim.xxx;
        float3 param_15 = color_1;
        float param_16 = texColor.w * vColor.w;
        OutputFragment(param_15, param_16);
        return;
    }
    if (_834_light_params.w == 5.0f)
    {
        float wc_paper = _1260_toon_shadow_color.x;
        float wc_edge = _1260_toon_shadow_color.y;
        float wc_bleed = _1260_toon_shadow_color.z;
        float wc_pigment = max(_1260_toon_shadow_color.w, 0.100000001490116119384765625f);
        float3 L_2 = normalize(-_834_light_dir_and_enabled.xyz);
        float3 V_wc = normalize(_1280_camera_pos.xyz - vFragPos);
        float NdotL_1 = (dot(N, L_2) * 0.5f) + 0.5f;
        float3 baseColor_1 = (texColor.xyz * vColor.xyz) * _1260_albedo.xyz;
        float soft_band = smoothstep(0.25f, 0.550000011920928955078125f, NdotL_1);
        float3 param_17 = vFragPos;
        float3 param_18 = vFragPosViewSpace;
        float3 param_19 = N;
        float3 param_20 = L_2;
        float shadow_2 = ShadowCalculation(param_17, param_18, param_19, param_20);
        float3 lit = (baseColor_1 * _834_light_color_and_ambient.xyz) * _834_light_params.x;
        float3 shade = (baseColor_1 * float3(0.449999988079071044921875f, 0.4000000059604644775390625f, 0.5f)) * _834_light_color_and_ambient.w;
        float3 diffuse_1 = lerp(shade, lit, soft_band.xxx) * (1.0f - (shadow_2 * 0.60000002384185791015625f));
        float fresnel = 1.0f - max(dot(N, V_wc), 0.0f);
        float edge_factor = pow(fresnel, 3.0f) * wc_edge;
        diffuse_1 *= (1.0f - (edge_factor * 0.5f));
        float paper_noise = frac(sin(dot(gl_FragCoord.xy * 0.00999999977648258209228515625f, float2(12.98980045318603515625f, 78.233001708984375f))) * 43758.546875f);
        paper_noise = (paper_noise * 0.5f) + 0.5f;
        diffuse_1 = lerp(diffuse_1, diffuse_1 * paper_noise, (wc_paper * 0.300000011920928955078125f).xxx);
        float3 warm_shift = float3(0.02999999932944774627685546875f, -0.00999999977648258209228515625f, -0.02999999932944774627685546875f) * wc_bleed;
        diffuse_1 += (warm_shift * (1.0f - soft_band));
        diffuse_1 = pow(diffuse_1, (1.0f / wc_pigment).xxx);
        float3 param_21 = diffuse_1;
        float param_22 = texColor.w * vColor.w;
        OutputFragment(param_21, param_22);
        return;
    }
    if (_834_light_params.w == 3.0f)
    {
        float3 L_3 = normalize(-_834_light_dir_and_enabled.xyz);
        float3 V_st = normalize(_1280_camera_pos.xyz - vFragPos);
        float3 R_1 = reflect(_834_light_dir_and_enabled.xyz, N);
        float half_lambert_1 = (dot(N, L_3) * 0.5f) + 0.5f;
        float3 diffuse_2 = ((_1260_albedo.xyz * half_lambert_1) * _834_light_color_and_ambient.xyz) * _834_light_params.x;
        float spec_power = max(_1260_roughness_ao.x, 1.0f);
        float3 spec_color = _1260_albedo.w.xxx;
        float3 specular_1 = spec_color * pow(max(dot(R_1, V_st), 0.0f), spec_power);
        float3 emissive_val = _1260_emissive.xyz;
        float3 material_color = (diffuse_2 + specular_1) + emissive_val;
        float3 color_st = (material_color * texColor.xyz) * vColor.xyz;
        float3 param_23 = vFragPos;
        float3 param_24 = vFragPosViewSpace;
        float3 param_25 = N;
        float3 param_26 = L_3;
        float shadow_3 = ShadowCalculation(param_23, param_24, param_25, param_26);
        float shadow_multiplier_1 = 1.0f - (shadow_3 * 0.5f);
        float3 param_27 = color_st * shadow_multiplier_1;
        float param_28 = texColor.w * vColor.w;
        OutputFragment(param_27, param_28);
        return;
    }
    float3 surface_albedo = pow((texColor.xyz * vColor.xyz) * _1260_albedo.xyz, 2.2000000476837158203125f.xxx);
    float metallic = clamp(_1260_albedo.w, 0.0f, 1.0f);
    float roughness = clamp(_1260_roughness_ao.x, 0.039999999105930328369140625f, 1.0f);
    float ao = max(_1260_roughness_ao.y, 0.0f);
    float3 surface_emissive = _1260_emissive.xyz;
    if (_1260_flags.y != 0.0f)
    {
        float4 mrSample = u_metallic_roughness_map.Sample(_u_metallic_roughness_map_sampler, finalUV);
        roughness = clamp(mrSample.y * _1260_roughness_ao.x, 0.039999999105930328369140625f, 1.0f);
        metallic = clamp(mrSample.z * _1260_albedo.w, 0.0f, 1.0f);
    }
    if (_1260_flags.w != 0.0f)
    {
        ao *= u_occlusion_map.Sample(_u_occlusion_map_sampler, finalUV).x;
    }
    if (_1260_flags.z != 0.0f)
    {
        surface_emissive *= u_emissive_map.Sample(_u_emissive_map_sampler, finalUV).xyz;
    }
    float3 V = normalize(_1280_camera_pos.xyz - vFragPos);
    float3 F0 = 0.039999999105930328369140625f.xxx;
    F0 = lerp(F0, surface_albedo, metallic.xxx);
    float3 T = normalize(vTBN[0]);
    float3 B = normalize(vTBN[1]);
    float3 Lo = 0.0f.xxx;
    float3 L_4 = normalize(-_834_light_dir_and_enabled.xyz);
    float3 H_1 = normalize(V + L_4);
    float _2103;
    if (_1260_extra_params.w != 0.0f)
    {
        float3 param_29 = N;
        float3 param_30 = H_1;
        float3 param_31 = T;
        float3 param_32 = B;
        float param_33 = roughness;
        float param_34 = _1260_extra_params.w;
        _2103 = DistributionGGXAniso(param_29, param_30, param_31, param_32, param_33, param_34);
    }
    else
    {
        float3 param_35 = N;
        float3 param_36 = H_1;
        float param_37 = roughness;
        _2103 = DistributionGGX(param_35, param_36, param_37);
    }
    float NDF = _2103;
    float3 param_38 = N;
    float3 param_39 = V;
    float3 param_40 = L_4;
    float param_41 = roughness;
    float G = GeometrySmith(param_38, param_39, param_40, param_41);
    float param_42 = max(dot(H_1, V), 0.0f);
    float3 param_43 = F0;
    float3 F = fresnelSchlick(param_42, param_43);
    float3 numerator = F * (NDF * G);
    float denominator = ((4.0f * max(dot(N, V), 0.0f)) * max(dot(N, L_4), 0.0f)) + 9.9999997473787516355514526367188e-05f;
    float3 specular_2 = numerator / denominator.xxx;
    float3 kS = F;
    float3 kD = 1.0f.xxx - kS;
    kD *= (1.0f - metallic);
    float NdotL_2 = max(dot(N, L_4), 0.0f);
    float3 param_44 = vFragPos;
    float3 param_45 = vFragPosViewSpace;
    float3 param_46 = N;
    float3 param_47 = L_4;
    float shadow_4 = ShadowCalculation(param_44, param_45, param_46, param_47);
    Lo += (((((((kD * surface_albedo) / 3.1415927410125732421875f.xxx) + specular_2) * _834_light_color_and_ambient.xyz) * _834_light_params.x) * NdotL_2) * (1.0f - shadow_4));
    if (_1260_extra_params.x > 0.0f)
    {
        float3 param_48 = N;
        float3 param_49 = L_4;
        float3 param_50 = surface_albedo;
        float param_51 = _1260_extra_params.x;
        float3 param_52 = _834_light_color_and_ambient.xyz;
        float param_53 = _834_light_params.x;
        float3 param_54 = _1260_extra_params2.yzw;
        Lo += (SubsurfaceScattering(param_48, param_49, param_50, param_51, param_52, param_53, param_54) * (1.0f - shadow_4));
    }
    if (_1260_extra_params.y > 0.0f)
    {
        float cc_r = max(_1260_extra_params.z, 0.039999999105930328369140625f);
        float3 param_55 = N;
        float3 param_56 = H_1;
        float param_57 = cc_r;
        float NDF_cc = DistributionGGX(param_55, param_56, param_57);
        float3 param_58 = N;
        float3 param_59 = V;
        float3 param_60 = L_4;
        float param_61 = cc_r;
        float G_cc = GeometrySmith(param_58, param_59, param_60, param_61);
        float param_62 = max(dot(H_1, V), 0.0f);
        float3 param_63 = 0.039999999105930328369140625f.xxx;
        float3 F_cc = fresnelSchlick(param_62, param_63);
        float3 spec_cc = (F_cc * (NDF_cc * G_cc)) / (((4.0f * max(dot(N, V), 0.0f)) * max(dot(N, L_4), 0.0f)) + 9.9999997473787516355514526367188e-05f).xxx;
        Lo += (((((spec_cc * _1260_extra_params.y) * NdotL_2) * _834_light_color_and_ambient.xyz) * _834_light_params.x) * (1.0f - shadow_4));
    }
    int cl_tx = int(gl_FragCoord.x) / 16;
    int cl_ty = int(gl_FragCoord.y) / 16;
    float cl_linear_z = max(-vFragPosViewSpace.z, 9.9999997473787516355514526367188e-05f);
    float cl_log_ratio = log(asfloat(_2340.Load(16)) / max(asfloat(_2340.Load(12)), 9.9999997473787516355514526367188e-05f));
    int _2351;
    if (cl_log_ratio > 0.0f)
    {
        _2351 = clamp(int((log(cl_linear_z / max(asfloat(_2340.Load(12)), 9.9999997473787516355514526367188e-05f)) / cl_log_ratio) * float(_2340.Load(8))), 0, int(_2340.Load(8)) - 1);
    }
    else
    {
        _2351 = 0;
    }
    int cl_tz = _2351;
    int cl_idx = (((cl_tz * int(_2340.Load(4))) + cl_ty) * int(_2340.Load(0))) + cl_tx;
    int cl_total = (int(_2340.Load(0)) * int(_2340.Load(4))) * int(_2340.Load(8));
    cl_idx = clamp(cl_idx, 0, max((cl_total - 1), 0));
    uint cl_offset = _2340.Load(cl_idx * 16 + 32);
    uint cl_point_count = _2340.Load(cl_idx * 16 + 36);
    uint cl_spot_count = _2340.Load(cl_idx * 16 + 40);
    for (uint ci = 0u; ci < cl_point_count; ci++)
    {
        int i = int(_2432.Load((cl_offset + ci) * 4 + 0));
        if (i >= int(_2444.Load(0)))
        {
            continue;
        }
        float3 L_5 = normalize(asfloat(_2444.Load3(i * 48 + 32)) - vFragPos);
        float3 H_2 = normalize(V + L_5);
        float _distance = length(asfloat(_2444.Load3(i * 48 + 32)) - vFragPos);
        float attenuation = clamp(1.0f - ((_distance * _distance) / (asfloat(_2444.Load(i * 48 + 44)) * asfloat(_2444.Load(i * 48 + 44)))), 0.0f, 1.0f);
        attenuation *= attenuation;
        float3 radiance = (asfloat(_2444.Load3(i * 48 + 16)) * asfloat(_2444.Load(i * 48 + 28))) * attenuation;
        float3 param_64 = N;
        float3 param_65 = H_2;
        float param_66 = roughness;
        float NDF_1 = DistributionGGX(param_64, param_65, param_66);
        float3 param_67 = N;
        float3 param_68 = V;
        float3 param_69 = L_5;
        float param_70 = roughness;
        float G_1 = GeometrySmith(param_67, param_68, param_69, param_70);
        float param_71 = max(dot(H_2, V), 0.0f);
        float3 param_72 = F0;
        float3 F_1 = fresnelSchlick(param_71, param_72);
        float3 numerator_1 = F_1 * (NDF_1 * G_1);
        float denominator_1 = ((4.0f * max(dot(N, V), 0.0f)) * max(dot(N, L_5), 0.0f)) + 9.9999997473787516355514526367188e-05f;
        float3 specular_3 = numerator_1 / denominator_1.xxx;
        float3 kS_1 = F_1;
        float3 kD_1 = 1.0f.xxx - kS_1;
        kD_1 *= (1.0f - metallic);
        float NdotL_3 = max(dot(N, L_5), 0.0f);
        float point_shadow = 0.0f;
        if (int(_2444.Load(i * 48 + 48)) != 0)
        {
            int param_73 = int(_2444.Load(i * 48 + 52));
            float3 param_74 = vFragPos;
            float3 param_75 = asfloat(_2444.Load3(i * 48 + 32));
            float param_76 = asfloat(_2444.Load(i * 48 + 44));
            point_shadow = PointShadowCalculation(param_73, param_74, param_75, param_76);
        }
        Lo += ((((((kD_1 * surface_albedo) / 3.1415927410125732421875f.xxx) + specular_3) * radiance) * NdotL_3) * (1.0f - point_shadow));
    }
    for (uint si = 0u; si < cl_spot_count; si++)
    {
        int i_1 = int(_2432.Load(((cl_offset + cl_point_count) + si) * 4 + 0));
        if (i_1 >= int(_2626.Load(0)))
        {
            continue;
        }
        float3 L_6 = normalize(asfloat(_2626.Load3(i_1 * 64 + 32)) - vFragPos);
        float3 H_3 = normalize(V + L_6);
        float _distance_1 = length(asfloat(_2626.Load3(i_1 * 64 + 32)) - vFragPos);
        float attenuation_1 = clamp(1.0f - ((_distance_1 * _distance_1) / (asfloat(_2626.Load(i_1 * 64 + 44)) * asfloat(_2626.Load(i_1 * 64 + 44)))), 0.0f, 1.0f);
        attenuation_1 *= attenuation_1;
        float3 spotDir = normalize(-asfloat(_2626.Load3(i_1 * 64 + 48)));
        float theta = dot(L_6, spotDir);
        float outerCos = cos(radians(asfloat(_2626.Load(i_1 * 64 + 64))));
        float innerCos = cos(radians(asfloat(_2626.Load(i_1 * 64 + 60))));
        float epsilon = max(innerCos - outerCos, 9.9999997473787516355514526367188e-05f);
        float cone = clamp((theta - outerCos) / epsilon, 0.0f, 1.0f);
        if (cone <= 0.0f)
        {
            continue;
        }
        float3 radiance_1 = ((asfloat(_2626.Load3(i_1 * 64 + 16)) * asfloat(_2626.Load(i_1 * 64 + 28))) * attenuation_1) * cone;
        float3 param_77 = N;
        float3 param_78 = H_3;
        float param_79 = roughness;
        float NDF_2 = DistributionGGX(param_77, param_78, param_79);
        float3 param_80 = N;
        float3 param_81 = V;
        float3 param_82 = L_6;
        float param_83 = roughness;
        float G_2 = GeometrySmith(param_80, param_81, param_82, param_83);
        float param_84 = max(dot(H_3, V), 0.0f);
        float3 param_85 = F0;
        float3 F_2 = fresnelSchlick(param_84, param_85);
        float3 numerator_2 = F_2 * (NDF_2 * G_2);
        float denominator_2 = ((4.0f * max(dot(N, V), 0.0f)) * max(dot(N, L_6), 0.0f)) + 9.9999997473787516355514526367188e-05f;
        float3 specular_4 = numerator_2 / denominator_2.xxx;
        float3 kS_2 = F_2;
        float3 kD_2 = 1.0f.xxx - kS_2;
        kD_2 *= (1.0f - metallic);
        float NdotL_4 = max(dot(N, L_6), 0.0f);
        float spot_shadow = 0.0f;
        if (int(_2626.Load(i_1 * 64 + 68)) != 0)
        {
            int param_86 = int(_2626.Load(i_1 * 64 + 72));
            float3 param_87 = vFragPos;
            float3 param_88 = N;
            float3 param_89 = L_6;
            spot_shadow = SpotShadowCalculation(param_86, param_87, param_88, param_89);
        }
        Lo += ((((((kD_2 * surface_albedo) / 3.1415927410125732421875f.xxx) + specular_4) * radiance_1) * NdotL_4) * (1.0f - spot_shadow));
    }
    float param_90 = max(dot(N, V), 0.0f);
    float3 param_91 = F0;
    float3 F_3 = fresnelSchlick(param_90, param_91);
    float3 kS_ambient = F_3;
    float3 kD_ambient = 1.0f.xxx - kS_ambient;
    kD_ambient *= (1.0f - metallic);
    float3 _2843;
    if (_236_probe_params.x > 0.5f)
    {
        float3 param_92 = N;
        _2843 = EvaluateSH(param_92);
    }
    else
    {
        _2843 = _834_light_color_and_ambient.w.xxx;
    }
    float3 irradiance = _2843;
    float3 diffuse_ambient = (kD_ambient * irradiance) * surface_albedo;
    float3 _2864;
    if (_236_probe_params.y > 0.5f)
    {
        float3 param_93 = N;
        float3 param_94 = V;
        float param_95 = roughness;
        float3 param_96 = F0;
        _2864 = SampleIBLSpecular(param_93, param_94, param_95, param_96);
    }
    else
    {
        _2864 = (irradiance * F0) * (1.0f - roughness);
    }
    float3 specular_ambient = _2864;
    float3 ambient = (diffuse_ambient + specular_ambient) * ao;
    if (_1260_extra_params.y > 0.0f)
    {
        float param_97 = max(dot(N, V), 0.0f);
        float3 param_98 = 0.039999999105930328369140625f.xxx;
        float3 F_cc_amb = fresnelSchlick(param_97, param_98);
        ambient += ((((F_cc_amb * _1260_extra_params.y) * irradiance) * (1.0f - _1260_extra_params.z)) * 0.25f);
    }
    float3 color_2 = (ambient + Lo) + surface_emissive;
    float3 param_99 = color_2;
    float param_100 = texColor.w * vColor.w;
    OutputFragment(param_99, param_100);
}

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    vTexCoord = stage_input.vTexCoord;
    vTBN = stage_input.vTBN;
    vFragPos = stage_input.vFragPos;
    vColor = stage_input.vColor;
    vNormal = stage_input.vNormal;
    vFragPosViewSpace = stage_input.vFragPosViewSpace;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.FragColor = FragColor;
    return stage_output;
}
