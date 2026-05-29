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
