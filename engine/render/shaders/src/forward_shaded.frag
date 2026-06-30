#version 450
#extension GL_ARB_separate_shader_objects : enable
// B2c-1: 自包含「高级 shading」forward 片元着色器。
// 复用 forward_pbr.vert（世界空间顶点 + vp）。在 forward_pbr.frag 基础上扩展 PerMaterial UBO，
// 支持 shading_mode 0/2/3/4/5/6（PBR / HalfLambert-Skin / HalfLambert-Static / Toon / Watercolor /
// FaceSDF）+ SSS / clearcoat / anisotropy / POM / alpha-test / double-sided。
// 方向光 + clustered 点光 + CSM 方向光阴影（手动 PCF 采样深度纹理）。着色 math 移植自生产 pbr.frag。
// 依赖 PerFrame/PerScene/PerMaterial 等 UBO + 纹理槽，便于 MeshRenderer 用通用原语满足全部绑定。

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
    vec4 light_params;             // x = light_intensity, y = shadow_strength, z = receive_shadow(>0.5)
    vec4 cascade_splits;           // xyz = view-space split distances per CSM cascade
    mat4 light_space_matrices[3];  // 各级联 shadow-sample 矩阵（含 shadow_sample_correction）
    vec4 shadow_atlas_regions[3];  // xy = UV scale, zw = UV offset（atlas 内子区域）
    mat4 u_spot_light_space_matrices[4];  // 聚光灯 light-space 矩阵（Final-Feat-8；点光用 cube 距离，无需矩阵）
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

// B2c-2: clustered 点光（≤64，UBO fallback）。布局与 ubo_types.h PointLightsUBO（binding=3）一致：
//   PointLightEntry = { vec3 color; float intensity; vec3 position; float radius; int cast_shadow; int shadow_index; vec2 _pad; } = 48B
struct FwdPointLight {
    vec3 color;        float intensity;
    vec3 position;     float radius;
    int  cast_shadow;  int   shadow_index;
    vec2 _pad;
};
layout(std140, set = 3, binding = 0) uniform PointLightUBO {
    int u_point_light_count;
    int _plpad0, _plpad1, _plpad2;
    FwdPointLight u_point_lights[64];
};

// Final-Feat-4: 聚光灯 SpotLight（≤64，UBO fallback）。布局与 ubo_types.h SpotLightEntry（64B）一致：
//   SpotLightEntry = { vec3 color; float intensity; vec3 position; float radius;
//                      vec3 direction; float inner_cone; float outer_cone;
//                      int cast_shadow; int shadow_index; float _pad; } = 64B
// 置于 set=7/binding=1（排序在 set6 之后）→ 通用原语契约 slot 7：
//   GL glBindBufferBase(...,7)，DX11 register(b7)，Vulkan 第 8 个 UBO descriptor。
// 与顶点级 SSBO（set7.b0）描述符类型不同，三后端互不冲突。
struct FwdSpotLight {
    vec3 color;        float intensity;
    vec3 position;     float radius;
    vec3 direction;    float inner_cone;   // 内锥半角（度）
    float outer_cone;                       // 外锥半角（度）
    int  cast_shadow;  int shadow_index;
    float _pad;
};
layout(std140, set = 7, binding = 1) uniform FwdSpotLightUBO {
    int u_spot_light_count;
    int _slpad0, _slpad1, _slpad2;
    FwdSpotLight u_spot_lights[64];
};

layout(set = 2, binding = 1) uniform sampler2D u_texture;                  // albedo  -> flat unit 0
layout(set = 2, binding = 2) uniform sampler2D u_normal_map;               // normal  -> flat unit 1
layout(set = 2, binding = 3) uniform sampler2D u_metallic_roughness_map;   // MR      -> flat unit 2
layout(set = 2, binding = 4) uniform sampler2D u_emissive_map;             // emissive-> flat unit 3
layout(set = 2, binding = 5) uniform sampler2D u_occlusion_map;            // AO      -> flat unit 4

// B2c-3: 地形 splatmap（权重图 + 4 layer）。binding 11-15 排序在上述 5 槽之后，
// 故通用原语 flat 纹理单元为 5/6/7/8/9（BindTexture slot 同号），不挪动已有 0-4 槽。
layout(set = 2, binding = 11) uniform sampler2D u_splat_weight_map;        // flat unit 5
layout(set = 2, binding = 12) uniform sampler2D u_splat_layer0;            // flat unit 6
layout(set = 2, binding = 13) uniform sampler2D u_splat_layer1;            // flat unit 7
layout(set = 2, binding = 14) uniform sampler2D u_splat_layer2;            // flat unit 8
layout(set = 2, binding = 15) uniform sampler2D u_splat_layer3;            // flat unit 9

// B2c-3: 地形参数 UBO。置于 set=4（紧接 PointLightUBO set=3 之后），使三后端
// 契约 slot 一致为 4：DX11 register(b4) / Vulkan 排序第 5 个 UBO / GL binding point 4。
layout(std140, set = 4, binding = 0) uniform TerrainParams {
    float u_splat_enabled;          // >0.5 = splatmap 混合 4 layer
    float u_snow_coverage;          // [0,1] 积雪覆盖率（0=关）
    float u_snow_normal_threshold;  // N.y 阈值
    float u_snow_edge_sharpness;    // 边缘锐利度（pow 指数）
    vec4  u_splat_tiling;           // 每 layer UV tiling
    vec4  u_snow_params;            // xyz = 雪面反照率, w = 雪面粗糙度
};

// B2c-5: 全局光照。LightProbeData（SH L2 间接漫反射）置 set=5 → 契约 slot 5；
// DDGIParams（探针体网格参数）置 set=6 → 契约 slot 6（DX11 b5/b6、Vulkan 排序第 6/7、GL 绑定点 5/6）。
layout(std140, set = 5, binding = 0) uniform FwdLightProbe {
    vec4 sh_coefficients[9];   // SH L2 系数（xyz 有效）
    vec4 probe_params;         // x = sh_enabled（>0.5 启用 SH 间接漫反射）
};
layout(std140, set = 6, binding = 0) uniform FwdDDGI {
    vec4  u_ddgi_origin;       // xyz = 网格最小角世界坐标, w = ddgi_enabled（>0.5）
    vec4  u_ddgi_spacing;      // xyz = 探针间距, w = gi_intensity
    ivec4 u_ddgi_resolution;   // xyz = 各轴探针数, w = irradiance_texels（含 1px 边界）
    vec4  u_ddgi_misc;         // x = normal_bias
};
// DDGI irradiance atlas：binding 19 排序在 PBR(1-5)/splat(11-15) 之后 → flat unit 10。
layout(set = 2, binding = 19) uniform sampler2D u_ddgi_irradiance_atlas;   // flat unit 10

// CSM 方向光 shadow atlas：binding 20 排序在 DDGI(19) 之后 → flat unit 11。
// 用普通 sampler2D 采样深度 .r 并手动比较（同 SpotShadowCalculation 思路），
// 避免依赖硬件比较采样器，可用通用 BindTexture 原语绑定深度纹理。
layout(set = 2, binding = 20) uniform sampler2D u_shadow_atlas;            // flat unit 11

// Final-Feat-8: 聚光灯 shadow map（sampler2D，binding 21-24 排序在 u_shadow_atlas(20) 之后 → flat unit 12-15）。
// 点光 shadow cube（samplerCube，binding 25-28 → flat unit 16-19）。均用单独采样器 + if-ladder 索引，
// 避免 sampler 数组在三后端 flat-unit 反射上的歧义；未用槽位绑定默认白色 2D/cube 深度纹理（采样得 1.0 → 无阴影）。
layout(set = 2, binding = 21) uniform sampler2D u_spot_shadow_map_0;        // flat unit 12
layout(set = 2, binding = 22) uniform sampler2D u_spot_shadow_map_1;        // flat unit 13
layout(set = 2, binding = 23) uniform sampler2D u_spot_shadow_map_2;        // flat unit 14
layout(set = 2, binding = 24) uniform sampler2D u_spot_shadow_map_3;        // flat unit 15
layout(set = 2, binding = 25) uniform samplerCube u_point_shadow_cube_0;    // flat unit 16
layout(set = 2, binding = 26) uniform samplerCube u_point_shadow_cube_1;    // flat unit 17
layout(set = 2, binding = 27) uniform samplerCube u_point_shadow_cube_2;    // flat unit 18
layout(set = 2, binding = 28) uniform samplerCube u_point_shadow_cube_3;    // flat unit 19

// Lightmap: binding 29 -> flat unit 20. Baked irradiance (RGB HDR) sampled with UV * st_offset.
layout(set = 2, binding = 29) uniform sampler2D u_lightmap;                 // flat unit 20

// Lightmap parameters encoded in toon_params.w (unused by toon shading when mode != 3):
//   watercolor.w reused as lightmap_enabled flag (>0.5 = use lightmap)
//   mode_params.w reused as pom_height_scale (not conflicting — lightmap uses st_offset in TerrainParams)
// Lightmap ST is encoded in u_splat_tiling.zw (when splat not enabled, these are free).
// To avoid excessive UBO changes, lightmap_enabled = (flags.w > 1.5) means:
//   flags.w == 0: no occlusion map, no lightmap
//   flags.w == 1: has occlusion map, no lightmap
//   flags.w == 2: has lightmap (replaces ambient)

const float PI = 3.14159265359;
const int CSM_CASCADES = 3;

// SH L2 求值（与 includes/lighting_utils.glsl EvaluateSH 一致），返回方向 N 的间接漫反射辐照度。
vec3 EvaluateSH(vec3 N) {
    vec3 r = sh_coefficients[0].xyz * 0.282095
           + sh_coefficients[1].xyz * 0.488603 * N.y
           + sh_coefficients[2].xyz * 0.488603 * N.z
           + sh_coefficients[3].xyz * 0.488603 * N.x
           + sh_coefficients[4].xyz * 1.092548 * N.x * N.y
           + sh_coefficients[5].xyz * 1.092548 * N.y * N.z
           + sh_coefficients[6].xyz * 0.315392 * (3.0 * N.z * N.z - 1.0)
           + sh_coefficients[7].xyz * 1.092548 * N.x * N.z
           + sh_coefficients[8].xyz * 0.546274 * (N.x * N.x - N.y * N.y);
    return max(r, vec3(0.0));
}

// 八面体编码（与 gi/ddgi_types.h OctEncode 一致）：单位方向 → [0,1]^2。
vec2 OctEncode(vec3 n) {
    n /= (abs(n.x) + abs(n.y) + abs(n.z));
    vec2 oct = n.xy;
    if (n.z < 0.0) {
        oct = (vec2(1.0) - abs(vec2(n.y, n.x)))
              * vec2(n.x >= 0.0 ? 1.0 : -1.0, n.y >= 0.0 ? 1.0 : -1.0);
    }
    return oct * 0.5 + 0.5;
}

// DDGI：在 worldPos 处按法线方向对 8 个邻近探针做三线性混合采样 irradiance atlas。
// atlas 排列与 DDGIVolumeConfig::IrradianceAtlasSize/ProbeIrradianceOffset 一致：
//   probes_per_row = res.x*res.z，每探针占 texels^2（含 1px 边界），col=x+z*res.x、row=y。
vec3 SampleDDGI(vec3 worldPos, vec3 N) {
    ivec3 res = u_ddgi_resolution.xyz;
    int texels = u_ddgi_resolution.w;
    if (res.x <= 0 || res.y <= 0 || res.z <= 0 || texels <= 2) return vec3(0.0);

    vec3 biased = worldPos + N * u_ddgi_misc.x;
    vec3 gridF = (biased - u_ddgi_origin.xyz) / max(u_ddgi_spacing.xyz, vec3(1e-4));
    gridF = clamp(gridF, vec3(0.0), vec3(res) - vec3(1.0));
    ivec3 base = ivec3(floor(gridF));
    vec3 fr = gridF - vec3(base);

    vec2 octUV = OctEncode(normalize(N));
    int probes_per_row = res.x * res.z;
    vec2 atlasSize = vec2(float(probes_per_row * texels), float(res.y * texels));
    vec2 interior = octUV * float(texels - 2) + 1.0;   // 落在 [1, texels-1] 内部，避开边界

    vec3 sum = vec3(0.0);
    float wsum = 0.0;
    for (int i = 0; i < 8; ++i) {
        ivec3 off = ivec3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
        ivec3 c = clamp(base + off, ivec3(0), res - ivec3(1));
        vec3 t = vec3(off) * fr + (vec3(1.0) - vec3(off)) * (vec3(1.0) - fr);
        float w = t.x * t.y * t.z;
        int col = c.x + c.z * res.x;
        vec2 pix = vec2(float(col * texels), float(c.y * texels)) + interior;
        sum += texture(u_ddgi_irradiance_atlas, pix / atlasSize).rgb * w;
        wsum += w;
    }
    return (wsum > 1e-5) ? sum / wsum : vec3(0.0);
}

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

// Final-Feat-8: 聚光灯阴影。投影到 spot light space → atlas-less 单图 3x3 PCF 手动深度比较。
// 返回 阴影量*shadow_strength [0,1]（与生产 shadow.glsl SpotShadowCalculation 同语义）。
// shadow_index<0（无阴影图）时返回 0；未用采样器槽绑白纹理（深度=1 → 不产生阴影）。
float SpotShadow(int shadowIndex, vec3 worldPos, vec3 N, vec3 L) {
    if (shadowIndex < 0 || shadowIndex >= 4) return 0.0;
    vec4 lp = u_spot_light_space_matrices[shadowIndex] * vec4(worldPos, 1.0);
    if (lp.w <= 1e-5) return 0.0;
    vec3 pc = lp.xyz / lp.w;
    pc = pc * 0.5 + 0.5;
    if (pc.z > 1.0) return 0.0;
    if (pc.x < 0.0 || pc.x > 1.0 || pc.y < 0.0 || pc.y > 1.0) return 0.0;
    float bias = max(0.003 * (1.0 - dot(N, L)), 0.0005);
    float cur = pc.z;
    vec2 texel;
    if (shadowIndex == 0)      texel = 1.0 / vec2(textureSize(u_spot_shadow_map_0, 0));
    else if (shadowIndex == 1) texel = 1.0 / vec2(textureSize(u_spot_shadow_map_1, 0));
    else if (shadowIndex == 2) texel = 1.0 / vec2(textureSize(u_spot_shadow_map_2, 0));
    else                       texel = 1.0 / vec2(textureSize(u_spot_shadow_map_3, 0));
    float occ = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 uv = pc.xy + vec2(x, y) * texel;
            float d;
            if (shadowIndex == 0)      d = textureLod(u_spot_shadow_map_0, uv, 0.0).r;
            else if (shadowIndex == 1) d = textureLod(u_spot_shadow_map_1, uv, 0.0).r;
            else if (shadowIndex == 2) d = textureLod(u_spot_shadow_map_2, uv, 0.0).r;
            else                       d = textureLod(u_spot_shadow_map_3, uv, 0.0).r;
            occ += (cur - bias) > d ? 1.0 : 0.0;
        }
    }
    return clamp(occ / 9.0 * light_params.y, 0.0, 1.0);
}

// Final-Feat-8: 点光阴影。cube 距离深度比较（与生产 shadow.glsl PointShadowCalculation 同语义）。
// 存储深度归一化到 [0,1]（× radius 还原）；shadow_index<0 或片元超出半径时返回 0。
float PointShadow(int shadowIndex, vec3 worldPos, vec3 lightPos, float radius) {
    if (shadowIndex < 0 || shadowIndex >= 4) return 0.0;
    vec3 toFrag = worldPos - lightPos;
    float cur = length(toFrag);
    if (cur >= radius) return 0.0;
    float closest;
    if (shadowIndex == 0)      closest = textureLod(u_point_shadow_cube_0, toFrag, 0.0).r * radius;
    else if (shadowIndex == 1) closest = textureLod(u_point_shadow_cube_1, toFrag, 0.0).r * radius;
    else if (shadowIndex == 2) closest = textureLod(u_point_shadow_cube_2, toFrag, 0.0).r * radius;
    else                       closest = textureLod(u_point_shadow_cube_3, toFrag, 0.0).r * radius;
    float bias = 0.05;
    return (cur - bias) > closest ? light_params.y : 0.0;
}

// 点光源 Cook-Torrance 贡献（与生产 pbr.frag 点光循环一致：平方反比半径衰减）。
vec3 PointLightsLo(vec3 N, vec3 V, vec3 world_pos, vec3 surface_albedo,
                   float roughness, float metallic, vec3 F0) {
    vec3 sum = vec3(0.0);
    for (int i = 0; i < u_point_light_count; ++i) {
        vec3 d = u_point_lights[i].position - world_pos;
        float dist = length(d);
        vec3 L = d / max(dist, 1e-4);
        float r = u_point_lights[i].radius;
        float atten = clamp(1.0 - (dist * dist) / (r * r + 1e-4), 0.0, 1.0);
        atten *= atten;
        // Final-Feat-8: 点光阴影衰减（cast_shadow 且 shadow_index>=0 时生效）。
        float psh = (u_point_lights[i].cast_shadow != 0)
            ? PointShadow(u_point_lights[i].shadow_index, world_pos, u_point_lights[i].position, r) : 0.0;
        vec3 radiance = u_point_lights[i].color * u_point_lights[i].intensity * atten * (1.0 - psh);
        vec3 H = normalize(V + L);
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 specular = (NDF * G * F) /
                        (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
        float NdotL = max(dot(N, L), 0.0);
        sum += (kD * surface_albedo / PI + specular) * radiance * NdotL;
    }
    return sum;
}

// 聚光灯 Cook-Torrance 贡献（点光衰减 + 锥角 inner/outer 平滑衰减；与生产 pbr.frag 聚光循环一致）。
// direction 为光线传播方向（光源→场景），故 L·(-dir) 衡量片元是否落在锥内。
vec3 SpotLightsLo(vec3 N, vec3 V, vec3 world_pos, vec3 surface_albedo,
                  float roughness, float metallic, vec3 F0) {
    vec3 sum = vec3(0.0);
    for (int i = 0; i < u_spot_light_count; ++i) {
        vec3 d = u_spot_lights[i].position - world_pos;
        float dist = length(d);
        vec3 L = d / max(dist, 1e-4);
        float r = u_spot_lights[i].radius;
        float atten = clamp(1.0 - (dist * dist) / (r * r + 1e-4), 0.0, 1.0);
        atten *= atten;
        // 锥角衰减：theta = L·spotDir（spotDir 指向光源）。
        vec3 spotDir = normalize(-u_spot_lights[i].direction);
        float theta = dot(L, spotDir);
        float innerCos = cos(radians(u_spot_lights[i].inner_cone));
        float outerCos = cos(radians(u_spot_lights[i].outer_cone));
        float cone = clamp((theta - outerCos) / max(innerCos - outerCos, 1e-4), 0.0, 1.0);
        if (cone <= 0.0) continue;
        // Final-Feat-8: 聚光灯阴影衰减（cast_shadow 且 shadow_index>=0 时生效）。
        float ssh = (u_spot_lights[i].cast_shadow != 0)
            ? SpotShadow(u_spot_lights[i].shadow_index, world_pos, N, L) : 0.0;
        vec3 radiance = u_spot_lights[i].color * u_spot_lights[i].intensity * atten * cone * (1.0 - ssh);
        vec3 H = normalize(V + L);
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 specular = (NDF * G * F) /
                        (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
        vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
        float NdotL = max(dot(N, L), 0.0);
        sum += (kD * surface_albedo / PI + specular) * radiance * NdotL;
    }
    return sum;
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

// CSM 单级联手动 PCF：投影到 light space → atlas 子区域 UV → 3x3 PCF 深度比较。
// 返回阴影量 [0,1]（1=全阴影）。与生产 shadow.glsl ShadowForCascade 同语义（手动比较版）。
float SampleCascadeShadow(int idx, vec3 worldPos, float bias) {
    vec4 lp = light_space_matrices[idx] * vec4(worldPos, 1.0);
    if (lp.w <= 1e-5) return 0.0;
    vec3 pc = lp.xyz / lp.w;
    pc = pc * 0.5 + 0.5;            // NDC[-1,1] → [0,1]（矩阵含 shadow_sample_correction，三后端一致）
    if (pc.z > 1.0) return 0.0;
    if (pc.x < 0.0 || pc.x > 1.0 || pc.y < 0.0 || pc.y > 1.0) return 0.0;
    vec2 uv = pc.xy * shadow_atlas_regions[idx].xy + shadow_atlas_regions[idx].zw;
    vec2 texel = 1.0 / vec2(textureSize(u_shadow_atlas, 0));
    float occ = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float d = textureLod(u_shadow_atlas, uv + vec2(x, y) * texel, 0.0).r;
            occ += (pc.z - bias) > d ? 1.0 : 0.0;
        }
    }
    return occ / 9.0;
}

// 方向光 CSM 阴影：view-space 深度选级联 → PCF → 乘 shadow_strength。
// receive_shadow 关 / 无 shadow map 时退化为 0（不影响着色）。
float DirectionalShadow(vec3 worldPos, vec3 N, vec3 L) {
    if (light_params.z < 0.5) return 0.0;                  // receive_shadow off
    float viewDepth = abs((view * vec4(worldPos, 1.0)).z);
    int idx = CSM_CASCADES - 1;
    for (int i = 0; i < CSM_CASCADES - 1; ++i) {
        if (viewDepth < cascade_splits[i]) { idx = i; break; }
    }
    float bias = max(0.0025 * (1.0 - dot(N, L)), 0.0004);
    float shadow = SampleCascadeShadow(idx, worldPos, bias);
    return clamp(shadow * light_params.y, 0.0, 1.0);       // light_params.y = shadow_strength
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

    // 地形 splatmap：按权重图归一化混合 4 个 layer；否则采样普通 albedo。
    vec4 baseTex;
    if (u_splat_enabled > 0.5) {
        vec4 w = texture(u_splat_weight_map, finalUV);
        float ws = w.r + w.g + w.b + w.a;
        if (ws > 0.001) w /= ws;
        vec3 c0 = texture(u_splat_layer0, finalUV * u_splat_tiling.x).rgb;
        vec3 c1 = texture(u_splat_layer1, finalUV * u_splat_tiling.y).rgb;
        vec3 c2 = texture(u_splat_layer2, finalUV * u_splat_tiling.z).rgb;
        vec3 c3 = texture(u_splat_layer3, finalUV * u_splat_tiling.w).rgb;
        baseTex = vec4(c0 * w.r + c1 * w.g + c2 * w.b + c3 * w.a, 1.0);
    } else {
        baseTex = texture(u_texture, finalUV);
    }
    vec4 texColor = baseTex * vColor;
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

    // CSM 方向光阴影（receive_shadow 关 / 无 shadow map 时为 0，不影响着色）。
    float shadow = DirectionalShadow(vWorldPos, N, L);

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
        vec3 diffuse_color = base_color * light_color * (hl * light_intensity * (1.0 - shadow) + ambient * 0.5);
        float spec_b = pow(max(dot(R, V), 0.0), 100.0);
        vec3 spec_tex = has_mr ? texture(u_metallic_roughness_map, finalUV).rgb : vec3(0.0);
        color = diffuse_color + spec_tex * spec_b;
        out_alpha = 1.0;
    } else if (shading_mode == 3) {
        // Half-Lambert STATIC (KF default)。
        vec3 R = reflect(-L, N);
        float hl = dot(N, L) * 0.5 + 0.5;
        vec3 diffuse = albedo.xyz * hl * light_color * light_intensity * (1.0 - shadow);
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
        diffuse = mix(shadowColor, diffuse, 1.0 - shadow);   // 阴影内压向阴影色
        float spec = step(toon_params.y, max(dot(N, H), 0.0)) * toon_params.z;
        vec3 specular = light_color * spec * (1.0 - shadow);
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
        diffuse *= (1.0 - shadow * 0.6);
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
        color = mix(shadowColor, color, 1.0 - shadow * 0.5);   // 阴影压向阴影色
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

        // 积雪覆盖：朝上表面（N.y 大）按阈值/锐利度混入雪面反照率与粗糙度。
        if (u_snow_coverage > 0.001) {
            float snow_dot = max(N.y, 0.0);
            float snow_mask = pow(smoothstep(u_snow_normal_threshold, 1.0, snow_dot),
                                  u_snow_edge_sharpness);
            float snow_factor = snow_mask * u_snow_coverage;
            vec3 snow_alb = pow(u_snow_params.xyz, vec3(2.2));
            surface_albedo = mix(surface_albedo, snow_alb, snow_factor);
            roughness = mix(roughness, u_snow_params.w, snow_factor);
            metallic = mix(metallic, 0.0, snow_factor);
        }

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

        // 方向光阴影：仅作用于方向光项（含 SSS / clearcoat），不影响点光。
        Lo *= (1.0 - shadow);

        // clustered 点光（≤64）：附加 Cook-Torrance 贡献。
        Lo += PointLightsLo(N, V, vWorldPos, surface_albedo, roughness, metallic, F0);

        // 聚光灯（≤64，Final-Feat-4）：锥角衰减后附加 Cook-Torrance 贡献（空=不影响）。
        Lo += SpotLightsLo(N, V, vWorldPos, surface_albedo, roughness, metallic, F0);

        // 间接漫反射：Lightmap > DDGI > SH > 平坦环境光（优先级递减）。
        vec3 indirect = vec3(ambient);
        if (probe_params.x > 0.5) indirect = EvaluateSH(N);
        if (u_ddgi_origin.w > 0.5) indirect = SampleDDGI(vWorldPos, N) * u_ddgi_spacing.w;
        // Lightmap: flags.w > 1.5 表示有光照贴图，使用 UV * st_offset 采样烘焙辐照度替换 ambient
        if (flags.w > 1.5) {
            vec2 lm_uv = vTexCoord * u_splat_tiling.zw + u_snow_params.xy;
            vec3 lm_irradiance = texture(u_lightmap, lm_uv).rgb;
            indirect = lm_irradiance;
        }
        vec3 ambient_term = indirect * surface_albedo * ao;
        color = ambient_term + Lo + surface_emissive;
    }

    // Reinhard tonemap + gamma（与 forward_pbr.frag 一致，保证软渲下三后端 LDR 输出一致）。
    color = color / (color + vec3(1.0));
    color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));

    // B2c-4: 透明 WBOIT（加权混合 OIT）。clearcoat.z 复用为 wboit_mode：
    //   0 = 普通（直写 color/alpha）；1 = accumulation 通道（预乘加权，配加性混合 ONE/ONE）；
    //   2 = revealage 通道（写 alpha，配 ZERO/ONE_MINUS_SRC_ALPHA 乘性混合）。
    // 权重函数与 includes/output.glsl 的 OutputFragment 一致，保证与既有透明路径同语义。
    float wboit_mode = clearcoat.z;
    if (wboit_mode > 0.5) {
        float z = gl_FragCoord.z;
        float weight = out_alpha * max(1e-2, 3e3 * pow(1.0 - z, 3.0));
        if (wboit_mode < 1.5) {
            FragColor = vec4(color * out_alpha * weight, out_alpha * weight);
        } else {
            FragColor = vec4(0.0, 0.0, 0.0, out_alpha);
        }
        return;
    }
    FragColor = vec4(color, out_alpha);
}
