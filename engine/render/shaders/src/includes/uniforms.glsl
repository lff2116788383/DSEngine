// Set 0: PerFrame

layout(std140, set = 0, binding = 0) uniform PerFrame {

    mat4 vp;

    mat4 view;

    vec4 camera_pos;

    vec4 foliage_wind;

    vec4 foliage_push;

};



// Set 1: PerScene

layout(std140, set = 1, binding = 0) uniform PerScene {

    vec4 light_dir_and_enabled;

    vec4 light_color_and_ambient;

    vec4 light_params;

    vec4 cascade_splits;

    mat4 light_space_matrices[3];

    vec4 shadow_atlas_regions[3]; // xy = UV scale, zw = UV offset per cascade

};



#ifdef GPU_DRIVEN

// GPU-Driven 路径：材质数据来自 MaterialSSBO，逐10实例读取

struct DSEGPUMat {

    vec4 albedo;

    vec4 roughness_ao;

    vec4 emissive;

    vec4 flags;

    vec4 extra_params;

    vec4 extra_params2;

    vec4 toon_shadow_color;

    vec4 toon_params;

};

layout(std430, set = 2, binding = 9) readonly buffer MaterialSSBO {

    DSEGPUMat gpu_materials[];

};

#define _DSE_MAT gpu_materials[v_material_id]

#else

// Set 2: PerMaterial

layout(std140, set = 2, binding = 0) uniform PerMaterial {

    vec4 albedo;

    vec4 roughness_ao;

    vec4 emissive;

    vec4 flags;

    vec4 extra_params;

    vec4 extra_params2;

    vec4 toon_shadow_color;

    vec4 toon_params;

};

#define _DSE_MAT

#endif



// 采样器 (Set 2)

layout(set = 2, binding = 1) uniform sampler2D u_texture;

layout(set = 2, binding = 2) uniform sampler2D u_normal_map;

layout(set = 2, binding = 3) uniform sampler2D u_metallic_roughness_map;

layout(set = 2, binding = 4) uniform sampler2D u_emissive_map;

layout(set = 2, binding = 5) uniform sampler2D u_occlusion_map;



#define CSM_CASCADES 3

layout(set = 2, binding = 6) uniform sampler2DShadow u_shadow_atlas;

layout(set = 2, binding = 7) uniform sampler2D u_spot_shadow_maps[4];



layout(std140, set = 2, binding = 19) uniform SpotLightData {

    mat4 u_spot_light_space_matrices[4];

};



layout(set = 3, binding = 0) uniform samplerCube u_point_shadow_maps[4];



struct PointLight {

    vec3 color;

    float intensity;

    vec3 position;

    float radius;

    int cast_shadow;

    int shadow_index;

    vec2 _pad;

};

#define MAX_POINT_LIGHTS 256

layout(std430, set = 1, binding = 1) readonly buffer PointLightSSBO {

    int u_point_light_count;

    int _pl_pad0;

    int _pl_pad1;

    int _pl_pad2;

    PointLight u_point_lights[];

};



struct SpotLight {

    vec3 color;

    float intensity;

    vec3 position;

    float radius;

    vec3 direction;

    float inner_cone;

    float outer_cone;

    int cast_shadow;

    int shadow_index;

    float _pad;  // NOTE: must be float (not vec2) to keep stride=64B matching C++ VulkanSpotLightsUBO::Entry

};

#define MAX_SPOT_LIGHTS 256

layout(std430, set = 1, binding = 2) readonly buffer SpotLightSSBO {

    int u_spot_light_count;

    int _sl_pad0;

    int _sl_pad1;

    int _sl_pad2;

    SpotLight u_spot_lights[];

};



// Clustered Forward+: cluster 网格参数 + 每 cluster 光源列表

struct ClusterInfoEntry {

    uint offset;

    uint point_count;

    uint spot_count;

    uint _pad;

};

layout(std430, set = 1, binding = 3) readonly buffer ClusterInfoSSBO {

    uint cluster_tiles_x;

    uint cluster_tiles_y;

    uint cluster_z_slices;

    float cluster_near;

    float cluster_far;

    uint _ci_pad0, _ci_pad1, _ci_pad2;

    ClusterInfoEntry cluster_infos[];

};

layout(std430, set = 1, binding = 4) readonly buffer LightIndexSSBO {

    uint light_indices[];

};



// Set 1: LightProbeData (SH L2 间接漫反射)

layout(std140, set = 1, binding = 5) uniform LightProbeData {

    vec4 sh_coefficients[9];

    vec4 probe_params;

    // probe_params: x=sh_enabled, y=ibl_enabled, z=unused, w=unused

};

#define u_sh_enabled  (probe_params.x > 0.5)

#define u_ibl_enabled (probe_params.y > 0.5)



// Terrain Splatmap (standalone uniforms, binding 11-15)

layout(set = 2, binding = 11) uniform sampler2D u_splat_weight_map;

layout(set = 2, binding = 12) uniform sampler2D u_splat_layer0;

layout(set = 2, binding = 13) uniform sampler2D u_splat_layer1;

layout(set = 2, binding = 14) uniform sampler2D u_splat_layer2;

layout(set = 2, binding = 15) uniform sampler2D u_splat_layer3;

layout(std140, set = 2, binding = 16) uniform TerrainParams {

    float u_splat_enabled;  // >0.5 = splatmap mode

    float u_snow_coverage;  // [0,1] 积雪覆盖率

    float u_snow_normal_threshold; // N.y 阈值

    float u_snow_edge_sharpness;   // 边缘锐利度 (pow 指数)

    vec4  u_splat_tiling;   // per-layer UV tiling factor

    vec4  u_snow_params;    // xyz=snow_albedo, w=snow_roughness

};



// IBL: Reflection Probe (Set 2, binding 17/18)

// 注意：binding 8/9 被 pbr.vert 用于 BoneMatrices / MorphWeights UBO，binding 16 被

// TerrainParams UBO 占用。同 set/binding 不能跨 stage 复用为不同 descriptor type

// （VUID-VkGraphicsPipelineCreateInfo-layout-07990），故 IBL sampler 移到 17/18。

layout(set = 2, binding = 17) uniform samplerCube u_reflection_cubemap;

layout(set = 2, binding = 18) uniform sampler2D   u_brdf_lut;



const float PI = 3.14159265359;



// UBO 字段便捷访问别名

#define u_lighting_enabled    (light_dir_and_enabled.w != 0.0)

#define u_light_direction     light_dir_and_enabled.xyz

#define u_light_color         light_color_and_ambient.xyz

#define u_light_intensity     light_params.x

#define u_ambient_intensity   light_color_and_ambient.w

#define u_shadow_strength     light_params.y

#define u_receive_shadow      (light_params.z != 0.0)

#define u_cascade_splits      cascade_splits.xyz

#define u_wboit_mode          cascade_splits.w



#ifdef GPU_DRIVEN

#define u_material_albedo           _DSE_MAT.albedo.xyz

#define u_material_metallic         _DSE_MAT.albedo.w

#define u_material_roughness        _DSE_MAT.roughness_ao.x

#define u_material_ao               _DSE_MAT.roughness_ao.y

#define u_material_normal_strength  _DSE_MAT.roughness_ao.z

#define u_material_alpha_cutoff     _DSE_MAT.roughness_ao.w

#define u_material_emissive         _DSE_MAT.emissive.xyz

#define u_material_alpha_test       (_DSE_MAT.emissive.w != 0.0)

#define u_has_normal_map            (_DSE_MAT.flags.x != 0.0)

#define u_has_metallic_roughness_map (_DSE_MAT.flags.y != 0.0)

#define u_has_emissive_map          (_DSE_MAT.flags.z != 0.0)

#define u_has_occlusion_map         (_DSE_MAT.flags.w != 0.0)

#define u_sss_strength              _DSE_MAT.extra_params.x

#define u_clear_coat                _DSE_MAT.extra_params.y

#define u_clear_coat_roughness      _DSE_MAT.extra_params.z

#define u_anisotropy                _DSE_MAT.extra_params.w

#define u_pom_height_scale          _DSE_MAT.extra_params2.x

#define u_sss_tint                  _DSE_MAT.extra_params2.yzw

#define u_toon_shadow_color         _DSE_MAT.toon_shadow_color.xyz

#define u_toon_shadow_threshold     _DSE_MAT.toon_shadow_color.w

#define u_toon_shadow_softness      _DSE_MAT.toon_params.x

#define u_toon_specular_size        _DSE_MAT.toon_params.y

#define u_toon_specular_strength    _DSE_MAT.toon_params.z

#define u_toon_rim_strength         _DSE_MAT.toon_params.w

#define _toon_shadow_color_vec4     _DSE_MAT.toon_shadow_color

#define _toon_params_vec4           _DSE_MAT.toon_params

#else

#define u_material_albedo           albedo.xyz

#define u_material_metallic         albedo.w

#define u_material_roughness        roughness_ao.x

#define u_material_ao               roughness_ao.y

#define u_material_normal_strength  roughness_ao.z

#define u_material_alpha_cutoff     roughness_ao.w

#define u_material_emissive         emissive.xyz

#define u_material_alpha_test       (emissive.w != 0.0)

#define u_has_normal_map            (flags.x != 0.0)

#define u_has_metallic_roughness_map (flags.y != 0.0)

#define u_has_emissive_map          (flags.z != 0.0)

#define u_has_occlusion_map         (flags.w != 0.0)

#define u_sss_strength              extra_params.x

#define u_clear_coat                extra_params.y

#define u_clear_coat_roughness      extra_params.z

#define u_anisotropy                extra_params.w

#define u_pom_height_scale          extra_params2.x

#define u_sss_tint                  extra_params2.yzw

#define u_toon_shadow_color         toon_shadow_color.xyz

#define u_toon_shadow_threshold     toon_shadow_color.w

#define u_toon_shadow_softness      toon_params.x

#define u_toon_specular_size        toon_params.y

#define u_toon_specular_strength    toon_params.z

#define u_toon_rim_strength         toon_params.w

#define _toon_shadow_color_vec4     toon_shadow_color

#define _toon_params_vec4           toon_params

#endif

#define u_camera_pos                camera_pos.xyz
