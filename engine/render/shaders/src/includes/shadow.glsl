float SampleShadowPCF(sampler2DShadow shadowMap, vec3 proj_coords, float bias) {

    float shadow = 0.0;

    vec2 texel_size = 1.0 / vec2(textureSize(shadowMap, 0));

    for (int x = -1; x <= 1; ++x)

        for (int y = -1; y <= 1; ++y)

            shadow += textureLod(shadowMap, vec3(proj_coords.xy

                      + vec2(x, y) * texel_size, proj_coords.z - bias), 0.0);

    return shadow / 9.0;

}



// --- PCSS (Percentage Closer Soft Shadows) ---



const int PCSS_NUM_SAMPLES = 32;

const vec2 kPoissonDisk[32] = vec2[](

    vec2(-0.94201624, -0.39906216),  vec2( 0.94558609, -0.76890725),

    vec2(-0.09418410, -0.92938870),  vec2( 0.34495938,  0.29387760),

    vec2(-0.91588581,  0.45771432),  vec2(-0.81544232, -0.87912464),

    vec2(-0.38277543,  0.27676845),  vec2( 0.97484398,  0.75648379),

    vec2( 0.44323325, -0.97511554),  vec2( 0.53742981, -0.47373420),

    vec2(-0.26496911, -0.41893023),  vec2( 0.79197514,  0.19090188),

    vec2(-0.24188840,  0.99706507),  vec2(-0.81409955,  0.91437590),

    vec2( 0.19984126,  0.78641367),  vec2( 0.14383161, -0.14100790),

    vec2(-0.61712016,  0.26906489),  vec2( 0.67523849, -0.20158166),

    vec2(-0.34893128,  0.73267590),  vec2( 0.16578432, -0.64535880),

    vec2( 0.85168291,  0.47469401),  vec2(-0.72731376, -0.44166890),

    vec2( 0.07624037,  0.44547778),  vec2(-0.47761092, -0.72846854),

    vec2( 0.61271930,  0.63369298),  vec2(-0.88889541,  0.10980070),

    vec2( 0.38613200, -0.35800546),  vec2(-0.17724770,  0.13285880),

    vec2( 0.50873750,  0.03464730),  vec2(-0.55399650, -0.16547160),

    vec2( 0.29741450,  0.94322680),  vec2(-0.04580817, -0.33139882)

);



const float PCSS_LIGHT_SIZE    = 0.004;

const float PCSS_SEARCH_RADIUS = 0.008;

const int   PCSS_BLOCKER_SEARCH_STEPS = 3;



// 用 sampler2DShadow 的比较结果 + 二分法近似遮挡体深度。

// texture(shadowMap, vec3(uv, ref)) 在 ref <= stored_depth 时返回 1.0，否则 0.0。

// 二分搜索 [0, receiverDepth] 中的比较翻转点即为 stored_depth（blocker depth）。

float FindBlockerDepth(sampler2DShadow shadowMap, vec2 uv, float receiverDepth,

                       float searchRadius) {

    float blockerSum = 0.0;

    int   blockerCount = 0;

    for (int i = 0; i < PCSS_NUM_SAMPLES; ++i) {

        vec2 sampleUV = uv + kPoissonDisk[i] * searchRadius;

        float vis = textureLod(shadowMap, vec3(sampleUV, receiverDepth), 0.0);

        if (vis < 0.5) {

            float lo = 0.0, hi = receiverDepth;

            for (int b = 0; b < PCSS_BLOCKER_SEARCH_STEPS; ++b) {

                float mid = (lo + hi) * 0.5;

                if (textureLod(shadowMap, vec3(sampleUV, mid), 0.0) < 0.5)

                    hi = mid;

                else

                    lo = mid;

            }

            blockerSum += (lo + hi) * 0.5;

            blockerCount++;

        }

    }

    if (blockerCount == 0) return -1.0;

    return blockerSum / float(blockerCount);

}



float PCSS_Shadow(sampler2DShadow shadowMap, vec3 projCoords, float bias) {

    vec2 texelSize = 1.0 / vec2(textureSize(shadowMap, 0));

    float receiverDepth = projCoords.z - bias;



    // Step 1: Blocker search — 找遮挡体平均深度

    float avgBlockerDepth = FindBlockerDepth(shadowMap, projCoords.xy, receiverDepth,

                                              PCSS_SEARCH_RADIUS);

    if (avgBlockerDepth < 0.0) return 1.0;



    // Step 2: 半影宽度 = lightSize * (dReceiver - dBlocker) / dBlocker

    float penumbraWidth = PCSS_LIGHT_SIZE * (receiverDepth - avgBlockerDepth)

                          / max(avgBlockerDepth, 0.0001);

    float filterRadius = max(penumbraWidth, texelSize.x);



    // Step 3: 可变核 Poisson PCF

    float shadow = 0.0;

    for (int i = 0; i < PCSS_NUM_SAMPLES; ++i) {

        vec2 offset = kPoissonDisk[i] * filterRadius;

        shadow += textureLod(shadowMap, vec3(projCoords.xy + offset, receiverDepth), 0.0);

    }

    return shadow / float(PCSS_NUM_SAMPLES);

}



float ShadowForCascade(int idx, vec3 fragPosWorldSpace, vec3 normal, vec3 lightDir) {

    vec4 fragPosLightSpace = light_space_matrices[idx] * vec4(fragPosWorldSpace, 1.0);

    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0) return 0.0;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) return 0.0;

    // Atlas UV transform: map cascade-local [0,1] to atlas sub-region

    projCoords.xy = projCoords.xy * shadow_atlas_regions[idx].xy + shadow_atlas_regions[idx].zw;

    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);

    float lit = PCSS_Shadow(u_shadow_atlas, projCoords, bias);

    // 仅返回归一化遮挡量 [0,1]，u_shadow_strength 由 ShadowCalculation 统一缩放，避免双重乘算

    return clamp(1.0 - lit, 0.0, 1.0);

}



float ShadowCalculation(vec3 fragPosWorldSpace, vec3 fragPosViewSpace, vec3 normal, vec3 lightDir) {

    if (!u_receive_shadow) return 0.0;

    float viewDepth = abs(fragPosViewSpace.z);

    int cascadeIndex = CSM_CASCADES - 1;

    for (int i = 0; i < CSM_CASCADES - 1; ++i) {

        if (viewDepth < u_cascade_splits[i]) {

            cascadeIndex = i;

            break;

        }

    }

    float shadow = ShadowForCascade(cascadeIndex, fragPosWorldSpace, normal, lightDir);



    // 级联边界 smoothstep 混合：在当前级联范围末尾 20% 区域混合到下一级

    if (cascadeIndex < CSM_CASCADES - 1) {

        float splitEnd = u_cascade_splits[cascadeIndex];

        float blendStart = splitEnd * 0.8;

        if (viewDepth > blendStart) {

            float blendFactor = smoothstep(blendStart, splitEnd, viewDepth);

            float nextShadow = ShadowForCascade(cascadeIndex + 1, fragPosWorldSpace, normal, lightDir);

            shadow = mix(shadow, nextShadow, blendFactor);

        }

    }



    return clamp(shadow * u_shadow_strength, 0.0, 1.0);

}



float SpotShadowCalculation(int shadowIndex, vec3 fragPosWorldSpace, vec3 normal, vec3 lightDir) {

    if (shadowIndex < 0 || shadowIndex >= 4) return 0.0;

    vec4 fragPosLightSpace = u_spot_light_space_matrices[shadowIndex] * vec4(fragPosWorldSpace, 1.0);

    vec3 projCoords = fragPosLightSpace.xyz / max(fragPosLightSpace.w, 0.0001);

    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0) return 0.0;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) return 0.0;

    float currentDepth = projCoords.z;

    float bias = max(0.003 * (1.0 - dot(normal, lightDir)), 0.0005);

    float shadow = 0.0;

    vec2 texelSize;

    if (shadowIndex == 0)      texelSize = 1.0 / vec2(textureSize(u_spot_shadow_maps[0], 0));

    else if (shadowIndex == 1) texelSize = 1.0 / vec2(textureSize(u_spot_shadow_maps[1], 0));

    else if (shadowIndex == 2) texelSize = 1.0 / vec2(textureSize(u_spot_shadow_maps[2], 0));

    else                       texelSize = 1.0 / vec2(textureSize(u_spot_shadow_maps[3], 0));

    for (int x = -1; x <= 1; ++x) {

        for (int y = -1; y <= 1; ++y) {

            float pcfDepth;

            if (shadowIndex == 0)      pcfDepth = textureLod(u_spot_shadow_maps[0], projCoords.xy + vec2(x, y) * texelSize, 0.0).r;

            else if (shadowIndex == 1) pcfDepth = textureLod(u_spot_shadow_maps[1], projCoords.xy + vec2(x, y) * texelSize, 0.0).r;

            else if (shadowIndex == 2) pcfDepth = textureLod(u_spot_shadow_maps[2], projCoords.xy + vec2(x, y) * texelSize, 0.0).r;

            else                       pcfDepth = textureLod(u_spot_shadow_maps[3], projCoords.xy + vec2(x, y) * texelSize, 0.0).r;

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

    float closestDepth;

    if (shadowIndex == 0)      closestDepth = textureLod(u_point_shadow_maps[0], fragToLight, 0.0).r * lightRadius;

    else if (shadowIndex == 1) closestDepth = textureLod(u_point_shadow_maps[1], fragToLight, 0.0).r * lightRadius;

    else if (shadowIndex == 2) closestDepth = textureLod(u_point_shadow_maps[2], fragToLight, 0.0).r * lightRadius;

    else                       closestDepth = textureLod(u_point_shadow_maps[3], fragToLight, 0.0).r * lightRadius;

    float bias = 0.05;

    return (currentDepth - bias) > closestDepth ? u_shadow_strength : 0.0;

}
