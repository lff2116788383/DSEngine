#ifndef DSE_COMPONENTS_3D_RENDER_H
#define DSE_COMPONENTS_3D_RENDER_H

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "engine/ecs/transform.h"
#include "engine/ecs/vegetation_mask.h"
#include "engine/render/rhi/rhi_handle.h"

#ifndef CSM_CASCADES
#define CSM_CASCADES 3
#endif

namespace dse {

struct MeshRendererComponent {
    enum class MaterialDataSource {
        ComponentFallback = 0,
        MaterialInstance = 1
    };

    std::string mesh_path;
    unsigned int material_instance_id = 0;
    std::string shader_variant = "MESH_UNLIT";
    glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec3 emissive = glm::vec3(0.0f, 0.0f, 0.0f);
    float metallic = 0.0f;
    float roughness = 0.5f;
    float ao = 1.0f;
    float normal_strength = 1.0f;
    float material_alpha_cutoff = 0.5f;
    bool material_alpha_test = false;
    bool material_double_sided = false;
    unsigned int albedo_texture_handle = 0;
    unsigned int normal_texture_handle = 0;
    unsigned int metallic_roughness_texture_handle = 0;
    unsigned int emissive_texture_handle = 0;
    unsigned int occlusion_texture_handle = 0;
    float sss_strength = 0.0f;
    glm::vec3 sss_tint = glm::vec3(0.0f);
    float clear_coat = 0.0f;
    float clear_coat_roughness = 0.1f;
    float anisotropy = 0.0f;
    float pom_height_scale = 0.0f;
    glm::vec3 toon_shadow_color = glm::vec3(0.15f, 0.1f, 0.18f);
    float toon_shadow_threshold = 0.35f;
    float toon_shadow_softness = 0.05f;
    float toon_specular_size = 0.6f;
    float toon_specular_strength = 0.8f;
    float toon_rim_strength = 0.3f;
    // Watercolor 参数（shading_mode=5, 与 toon 互斥，复用同一 UBO slot）
    float watercolor_paper_strength = 0.3f;
    float watercolor_edge_darkening = 0.4f;
    float watercolor_color_bleed = 0.2f;
    float watercolor_pigment_density = 1.0f;
    bool receive_shadow = true;
    bool depth_test_enabled = true;
    bool depth_write_enabled = true;
    bool visible = true;
    bool is_static = false;  ///< 标记为静态网格，首帧触发 StaticBatchBuilder 合批
    int sorting_layer = 0;
    int order_in_layer = 0;
    MaterialDataSource material_data_source = MaterialDataSource::ComponentFallback;
    unsigned int mesh_handle_override = 0;  ///< 非零时表示 LODSystem 正在管理此实体的 mesh_path
    std::vector<float> temp_vertices;
    std::vector<uint32_t> temp_indices;
    std::vector<float> temp_uvs;
    std::vector<float> temp_normals;
    std::vector<float> temp_tangents;
    int dmesh_vertex_stride = 20;  ///< v1=20 (no color), v2=24 (with RGBA color at [20-23])

    /// Hi-Z: 本地空间 AABB（mesh 加载后一次性计算）
    glm::vec3 local_bounds_min = glm::vec3(0.0f);
    glm::vec3 local_bounds_max = glm::vec3(0.0f);
    bool local_bounds_valid = false;
};

struct LODLevelConfig {
    std::string mesh_path;
    float screen_size_threshold = 0.0f;  ///< 当 screen_size > threshold 时选此级别
    unsigned int mesh_handle = 0;        ///< LODSystem 赋值的已加载 mesh 句柄
    bool loaded = false;
};

struct LODGroupComponent {
    bool enabled = true;
    std::vector<LODLevelConfig> levels;  ///< 按 threshold 降序排列（最高精度在前）
    int current_lod = -1;
    float global_scale = 1.0f;
    float hysteresis = 0.05f;            ///< 切换死区：升级需超 threshold*(1+h)，降级需低于 threshold*(1-h)
    float min_screen_size = 0.0f;        ///< 低于此 screen_size 时隐藏实体（0 = 不裁剪）
    bool lod_culled = false;             ///< LODSystem 管理的可见性，恢复时重置
    std::string original_mesh_path;      ///< 首次 LOD 切换前的原始 mesh_path，disable 时用于恢复
};

struct BoundingBoxComponent {
    glm::vec3 min_extents = glm::vec3(0.0f);
    glm::vec3 max_extents = glm::vec3(0.0f);
    
    // Calculate center
    glm::vec3 center() const {
        return (min_extents + max_extents) * 0.5f;
    }
    
    // Calculate half extents
    glm::vec3 extents() const {
        return (max_extents - min_extents) * 0.5f;
    }
};

struct Camera3DComponent {
    bool enabled = true;
    int priority = 0;
    float fov = 60.0f;
    float aspect_ratio = 1.333f;
    float near_clip = 0.1f;
    float far_clip = 1000.0f;
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 projection = glm::mat4(1.0f);
};

struct PostProcessComponent {
    bool enabled = true;
    
    // Bloom
    bool bloom_enabled = true;
    float bloom_threshold = 1.0f; 
    float bloom_intensity = 0.5f;
    float bloom_knee = 0.1f;           ///< 阈值柔和过渡宽度
    float bloom_mip_weight = 0.005f;   ///< mip upsample 混合权重
    
    // Color Grading
    bool color_grading_enabled = true;
    float exposure = 1.0f;
    float gamma = 2.2f;
    
    // SSAO
    bool ssao_enabled = false;
    float ssao_radius = 0.5f;
    float ssao_bias = 0.025f;
    int ssao_sample_count = 32;
    float ssao_power = 2.0f;
    float ssao_intensity = 1.0f;

    // Auto Exposure
    bool auto_exposure_enabled = false;
    float exposure_min = 0.1f;
    float exposure_max = 10.0f;
    float adaptation_speed_up = 2.0f;
    float adaptation_speed_down = 1.0f;
    float exposure_compensation = 0.0f;  // EV offset

    // Color Grading LUT
    unsigned int color_lut_handle = 0;   // 3D LUT texture handle (0 = disabled)
    float color_lut_intensity = 1.0f;    // blend factor (0 = no LUT, 1 = full LUT)

    // Vignette
    bool vignette_enabled = false;
    float vignette_intensity = 0.35f;
    float vignette_radius = 0.75f;
    float vignette_softness = 0.35f;

    // Film Grain
    bool film_grain_enabled = false;
    float film_grain_intensity = 0.08f;
    float film_grain_time_scale = 1.0f;

    // FXAA
    bool fxaa_enabled = true;

    // TAA (Temporal Anti-Aliasing)
    bool taa_enabled = false;
    float taa_blend_factor = 0.1f;

    // Contact Shadow
    bool contact_shadow_enabled = false;
    float contact_shadow_strength = 0.5f;
    int contact_shadow_steps = 16;
    float contact_shadow_step_size = 0.5f;

    // DOF (Depth of Field)
    bool dof_enabled = false;
    float dof_focus_distance = 100.0f;   // 对焦距离
    float dof_focus_range = 50.0f;       // 对焦范围（过渡宽度）
    float dof_bokeh_radius = 4.0f;       // 散景半径（像素）

    // Motion Blur
    bool motion_blur_enabled = false;
    float motion_blur_intensity = 1.0f;  // 模糊强度
    int motion_blur_samples = 8;         // 采样数

    // SSR (Screen Space Reflections)
    bool ssr_enabled = false;
    float ssr_max_distance = 100.0f;     // 最大光线步进距离
    float ssr_thickness = 0.5f;          // 表面厚度阈值
    float ssr_step_size = 1.0f;          // 步进大小
    int ssr_max_steps = 64;              // 最大步数
    float ssr_fade_distance = 0.2f;      // 屏幕边缘淡出距离 (UV 空间)
    float ssr_max_roughness = 0.5f;      // 粗糙度截止值

    // Outline / Edge Detection
    bool outline_enabled = false;
    glm::vec3 outline_color = glm::vec3(0.0f, 0.0f, 0.0f);  // 描边颜色
    float outline_thickness = 1.0f;       // 描边粗细（像素）
    float outline_depth_threshold = 0.1f; // 深度边缘阈值
    float outline_normal_threshold = 0.4f;// 法线边缘阈值

    // Light Shaft / God Ray（screen-space radial blur）
    bool light_shaft_enabled = false;
    glm::vec3 light_shaft_color = glm::vec3(1.0f, 0.95f, 0.8f); // 暖阳色
    float light_shaft_density = 0.84f;     // 步进密度
    float light_shaft_weight = 0.04f;      // 每步权重
    float light_shaft_decay = 0.97f;       // 逐步衰减
    float light_shaft_exposure = 0.4f;     // 最终曝光乘数
    float light_shaft_intensity = 1.0f;    // 整体混合强度
    int light_shaft_samples = 64;          // 采样数

    // Volumetric Fog（高度指数雾 + Mie 散射近似 raymarching）
    bool fog_enabled = false;
    glm::vec3 fog_color = glm::vec3(0.70f, 0.75f, 0.85f); // 雾颜色（默认天空蓝灰）
    float fog_density = 0.02f;        // 基础散射密度
    float fog_height_falloff = 0.3f;  // 高度衰减系数（越大雾层越薄）
    float fog_height_offset = 0.0f;   // 雾基线高度（世界坐标 Y）
    float fog_start = 0.0f;           // 开始积累雾的最小距离
    float fog_end = 1000.0f;          // 雾效最大步进距离
    int fog_steps = 16;               // Raymarching 步数（8~32 可用）
    float fog_sun_scatter = 0.6f;     // 日光 Mie 散射强度
};

// Screen-Space Decal（基于深度重建投影贴花）
struct DecalComponent {
    bool enabled = true;
    unsigned int albedo_texture = 0;     // 贴花颜色纹理
    glm::vec4 color = glm::vec4(1.0f);  // 颜色乘算 + alpha 不透明度
    float angle_fade = 0.5f;             // 法线角度衰减阈值（0=无衰减,1=严格正面）
};

struct DirectionalLight3DComponent {
    bool enabled = true;
    glm::vec3 direction = glm::vec3(-0.4f, -1.0f, -0.3f);
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
    float intensity = 1.0f;
    float ambient_intensity = 0.2f;
    float shadow_strength = 0.35f;
    bool cast_shadow = true;
    
    // CSM (Cascaded Shadow Maps) Settings
    float cascade_splits[CSM_CASCADES] = { 20.0f, 60.0f, 200.0f };
    float cascade_split_lambda = 0.75f;  ///< PSSM 混合因子 (0=均匀, 1=对数)
};

struct PointLightComponent {
    bool enabled = true;
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
    float intensity = 1.0f;
    float radius = 10.0f; // Attenuation radius
    float falloff = 1.0f;
    bool cast_shadow = false;
    unsigned int shadow_map_handle = 0; // Omnidirectional shadow map
};

struct SpotLightComponent {
    bool enabled = true;
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
    glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
    float intensity = 1.0f;
    float radius = 20.0f;
    float falloff = 1.0f;
    float inner_cone_angle = 12.5f; // degrees
    float outer_cone_angle = 17.5f; // degrees
    bool cast_shadow = false;
    unsigned int shadow_map_handle = 0;
};

struct SkyLightComponent {
    bool enabled = true;
    glm::vec3 up_color = glm::vec3(0.2f, 0.2f, 0.2f);
    glm::vec3 down_color = glm::vec3(0.0f, 0.0f, 0.5f);
    float intensity = 1.0f;
};

struct SkyboxComponent {
    bool enabled = true;
    unsigned int cubemap_handle = 0; // The RHI handle for the loaded cubemap texture
    std::string cubemap_path; // Path to load the cubemap from
};

struct FreeCameraControllerComponent {
    bool enabled = true;
    float move_speed = 5.0f;
    float mouse_sensitivity = 0.1f;
    float pitch = 0.0f;
    float yaw = -90.0f;
};

/// Sub-Scene: 将另一个 .dscene 文件作为嵌套场景实例化到当前层级中。
/// 编辑模式下作为占位节点显示；运行时由脚本/运行时系统负责加载实际场景内容。
struct SubSceneComponent {
    bool enabled = true;
    std::string scene_path;  ///< 相对于 data root 的 .dscene 路径
};

struct TerrainComponent {
    bool enabled = true;
    std::string heightmap_path;
    std::string texture_path;
    unsigned int texture_handle = 0;
    int heightmap_width = 0;
    int heightmap_height = 0;
    int heightmap_channels = 0;
    
    // Terrain parameters
    float width = 100.0f;
    float depth = 100.0f;
    float max_height = 20.0f;
    int resolution_x = 64; // Should be a power of 2 (e.g., 64, 128, 256)
    int resolution_z = 64;
    
    // Advanced LOD parameters (Based on VSEngine2.1 VSQuadTerrainGeometry)
    bool use_dynamic_lod = true;
    int max_lod_levels = 4; // 0 is highest detail, max_lod_levels-1 is lowest
    float lod_distance_factor = 50.0f; // Distance multiplier for LOD switching
    int current_lod = 0; // Updated dynamically by the TerrainSystem
    
    // Frustum Culling visibility
    bool visible = true;
    
    // Splat map: 4-layer texture weights per vertex (R=layer0, G=layer1, B=layer2, A=layer3)
    std::vector<float> splat_data;           // size = resolution_x * resolution_z * 4
    std::string splat_texture_paths[4];      // texture path per layer
    unsigned int splat_texture_handles[4] = {0, 0, 0, 0};
    glm::vec4 splat_tiling = glm::vec4(10.0f); // per-layer UV tiling factor
    bool splat_dirty = true; // 置位时 TerrainSystem 会把 splat_data 上传为权重图纹理

    // Internal state
    bool is_dirty = true;
    unsigned int splat_weight_texture = 0; // splat_data 上传得到的 RGBA8 权重图 GPU 句柄（0=未上传）
    std::vector<float> height_data;
    dse::render::VertexArrayHandle vao;
    dse::render::BufferHandle vbo;
    dse::render::BufferHandle shaded_vbo; // GpuMeshVertex 局部空间模板 VB（MeshRenderer 前向 pass，B2b-6）
    dse::render::BufferHandle ebo; // The base EBO (max LOD)
    std::vector<dse::render::BufferHandle> lod_ebos; // EBOs for different LOD levels
    std::vector<unsigned int> lod_index_counts;
    unsigned int index_count = 0;
};

inline float SampleTerrainHeight(const TerrainComponent& terrain,
                                  const TransformComponent& transform,
                                  float world_x, float world_z) {
    if (terrain.height_data.empty() || terrain.resolution_x < 2 || terrain.resolution_z < 2)
        return 0.0f;
    glm::mat4 inv_model = glm::inverse(transform.local_to_world);
    glm::vec4 local = inv_model * glm::vec4(world_x, 0.0f, world_z, 1.0f);
    const int rx = terrain.resolution_x;
    const int rz = terrain.resolution_z;
    const float half_w = terrain.width * 0.5f;
    const float half_d = terrain.depth * 0.5f;
    float gx = (local.x + half_w) / terrain.width * static_cast<float>(rx - 1);
    float gz = (local.z + half_d) / terrain.depth * static_cast<float>(rz - 1);
    if (gx < 0.0f || gz < 0.0f || gx >= static_cast<float>(rx - 1) || gz >= static_cast<float>(rz - 1))
        return 0.0f;
    int ix = static_cast<int>(gx);
    int iz = static_cast<int>(gz);
    float fx = gx - static_cast<float>(ix);
    float fz = gz - static_cast<float>(iz);
    int ix1 = std::min(ix + 1, rx - 1);
    int iz1 = std::min(iz + 1, rz - 1);
    float h00 = terrain.height_data[static_cast<size_t>(iz * rx + ix)];
    float h10 = terrain.height_data[static_cast<size_t>(iz * rx + ix1)];
    float h01 = terrain.height_data[static_cast<size_t>(iz1 * rx + ix)];
    float h11 = terrain.height_data[static_cast<size_t>(iz1 * rx + ix1)];
    float h0 = h00 + fx * (h10 - h00);
    float h1 = h01 + fx * (h11 - h01);
    return h0 + fz * (h1 - h0);
}

/// Water / Ocean surface (screen-space Gerstner wave + refraction/reflection)
struct WaterComponent {
    bool enabled = true;
    float water_level = 0.0f;                  ///< Y 高度 (世界坐标)
    glm::vec3 deep_color = glm::vec3(0.0f, 0.05f, 0.15f);   ///< 深水颜色
    glm::vec3 shallow_color = glm::vec3(0.0f, 0.4f, 0.55f);  ///< 浅水颜色
    float max_depth = 30.0f;                   ///< 深浅过渡的最大深度
    float transparency = 0.6f;                 ///< 水面整体透明度 (0=全透, 1=全不透)
    float wave_amplitude = 0.15f;              ///< Gerstner 波幅
    float wave_frequency = 1.5f;               ///< Gerstner 频率
    float wave_speed = 1.0f;                   ///< 波浪速度
    glm::vec2 wave_direction = glm::vec2(1.0f, 0.3f); ///< 主波浪方向 (自动归一化)
    float refraction_strength = 0.03f;         ///< 折射偏移强度
    float reflection_strength = 0.5f;          ///< 菲涅尔反射强度
    float specular_power = 128.0f;             ///< 高光指数
    // 视觉增强
    float caustic_intensity = 0.3f;            ///< 焦散光纹强度
    float caustic_scale = 8.0f;                ///< 焦散纹理缩放
    float foam_intensity = 0.5f;               ///< 岸边泡沫强度
    float foam_depth_threshold = 2.0f;         ///< 泡沫出现的深度阈值
    float underwater_fog_density = 0.15f;      ///< 水下雾密度
    glm::vec3 underwater_fog_color = glm::vec3(0.0f, 0.1f, 0.2f); ///< 水下雾颜色
};

/// Light Probe: captures indirect diffuse lighting at a point for GI approximation
struct LightProbeComponent {
    bool enabled = true;
    float influence_radius = 10.0f;       ///< Radius of influence (for blending)
    glm::vec3 sh_coefficients[9] = {};    ///< SH L2 coefficients (9 vec3 for RGB)
    bool needs_rebake = true;             ///< Flag for system to rebake
    bool show_debug = true;               ///< Show debug sphere in editor
};

/// Reflection Probe: captures environment reflections as a cubemap
struct ReflectionProbeComponent {
    bool enabled = true;
    float influence_radius = 15.0f;       ///< Blend distance
    float box_size_x = 10.0f;            ///< Box projection extents
    float box_size_y = 10.0f;
    float box_size_z = 10.0f;
    bool use_box_projection = false;
    int resolution = 128;                 ///< Cubemap face resolution
    unsigned int cubemap_handle = 0;      ///< GPU cubemap texture
    bool needs_rebake = true;
    bool show_debug = true;               ///< Show debug wireframe in editor
};

/// 大型植被系统：草地实例化渲染组件
struct GrassComponent {
    bool enabled = true;

    // 分布参数
    float density = 1.0f;              ///< 每平方单位草叶数
    float spawn_radius = 50.0f;        ///< 围绕摄像机的生成半径
    unsigned int seed = 42;            ///< 随机种子
    float chunk_size = 8.0f;           ///< 空间 chunk 边长（米）

    // 草叶外观
    float blade_width = 0.1f;
    float blade_height = 1.0f;
    float blade_height_variation = 0.3f;  ///< ±高度随机比例
    glm::vec3 base_color = glm::vec3(0.15f, 0.45f, 0.1f);
    glm::vec3 tip_color  = glm::vec3(0.3f, 0.65f, 0.15f);  ///< 叶尖颜色（GrassSystem 按高度 base_color→tip_color 顶点色渐变）
    unsigned int albedo_texture = 0;

    // 风场
    glm::vec2 wind_direction = glm::vec2(1.0f, 0.0f);
    float wind_speed = 1.0f;
    float wind_strength = 0.3f;
    float wind_turbulence = 0.2f;

    // LOD
    float lod_near = 30.0f;           ///< < near: 全精度草叶
    float lod_far  = 80.0f;           ///< > far: 完全剔除
    float fade_range = 5.0f;          ///< LOD 过渡距离：在边界区间内按 fade 缩放草叶高度平滑淡入/淡出

    // 阴影
    bool  cast_shadow = false;
    float shadow_distance = 20.0f;    ///< 仅近距离投射阴影

    // 植被密度遮罩（编辑器植被刷绘制；空=全图均匀满密度，向后兼容）
    VegetationDensityMask density_mask;

    // 运行时（GrassSystem 管理，用户不应手动写入）
    int cached_instance_count_ = 0;
};

/// TressFX 风格毛发组件
struct HairComponent {
    bool enabled = true;

    /// 毛发资产路径（.dhair 或程序化生成标记 "procedural:N:V:L:R"）
    std::string hair_asset_path;

    // 物理参数
    float damping          = 0.04f;
    float stiffness_local  = 0.8f;
    float stiffness_global = 0.4f;
    float gravity          = 9.81f;
    glm::vec3 wind         = glm::vec3(0.0f);
    float wind_turbulence  = 0.2f;

    // 渲染参数
    glm::vec4 root_color = glm::vec4(0.1f, 0.05f, 0.02f, 1.0f);
    glm::vec4 tip_color  = glm::vec4(0.4f, 0.25f, 0.15f, 1.0f);
    float fiber_radius   = 0.04f;
    float opacity        = 0.9f;
    float specular_power_primary   = 80.0f;
    float specular_power_secondary = 20.0f;
    float specular_strength_primary   = 0.6f;
    float specular_strength_secondary = 0.3f;
    glm::vec3 specular_color = glm::vec3(1.0f, 0.9f, 0.8f);

    // LOD
    float lod0_distance = 20.0f;
    float lod1_distance = 40.0f;
    float lod2_distance = 80.0f;
    float cull_distance = 120.0f;

    // follower strands
    int   num_follow_per_guide = 4;
    float follow_root_offset   = 1.5f;

    bool  cast_shadow   = true;
    bool  receive_shadow = true;

    // 运行时（HairSystem 管理，用户不应手动写入）
    int hair_instance_index_ = -1;
};

/// DDGI GI Probe Volume 组件
/// 定义一个 3D 探针网格，用于实时间接漫反射光照
struct GIProbeVolumeComponent {
    bool enabled = true;

    // 网格范围（世界坐标）
    glm::vec3 origin = glm::vec3(-50.0f);     ///< 网格最小角
    glm::vec3 extent = glm::vec3(100.0f);     ///< 网格范围

    // 探针分辨率（各轴数量）
    int resolution_x = 8;
    int resolution_y = 4;
    int resolution_z = 8;

    // 质量参数
    int irradiance_texels = 8;        ///< 每探针辐照度 octahedral 分辨率
    int visibility_texels = 8;        ///< 每探针可见性 octahedral 分辨率
    int rays_per_probe = 256;         ///< 每次更新采样 VPL 数量
    float hysteresis = 0.97f;         ///< Temporal 混合因子 (0=全新, 1=全旧)

    // 间接光强度
    float gi_intensity = 1.0f;        ///< 全局 GI 强度乘数
    float normal_bias = 0.2f;         ///< 法线方向偏移（减少自遮挡泄漏）
    float view_bias = 0.1f;           ///< 视线方向偏移

    // 调试
    bool show_debug_probes = false;   ///< 编辑器中显示探针球体

    // 运行时（DDGISystem 管理，用户不应手动写入）
    bool needs_reinit_ = true;
};

// ============================================================
// Morph Target (Blend Shape) Component
// ============================================================

struct MorphTargetDelta {
    glm::vec3 delta_position;
    float _pad0;
    glm::vec3 delta_normal;
    float _pad1;
};

struct MorphTargetData {
    std::string name;
    std::vector<MorphTargetDelta> deltas;  ///< per-vertex deltas, count == vertex_count
};

struct MorphTargetComponent {
    bool enabled = true;
    std::vector<MorphTargetData> targets;      ///< all morph targets for this mesh
    std::vector<float> weights;                ///< current weight for each target [0,1]
    int vertex_count = 0;                      ///< base mesh vertex count

    // GPU runtime state (managed by MorphTargetSystem)
    render::BufferHandle gpu_base_buffer;      ///< SSBO: base vertex data
    render::BufferHandle gpu_delta_buffer;     ///< SSBO: all target deltas (interleaved)
    render::BufferHandle gpu_weight_buffer;    ///< SSBO: weight array
    render::BufferHandle gpu_output_buffer;    ///< SSBO: deformed vertex output
    bool gpu_dirty = true;                     ///< needs re-upload

    float GetWeight(const std::string& name) const {
        for (size_t i = 0; i < targets.size(); ++i) {
            if (targets[i].name == name) return weights[i];
        }
        return 0.0f;
    }

    void SetWeight(const std::string& name, float w) {
        for (size_t i = 0; i < targets.size(); ++i) {
            if (targets[i].name == name) {
                weights[i] = w;
                gpu_dirty = true;
                return;
            }
        }
    }

    void SetWeightByIndex(int index, float w) {
        if (index >= 0 && index < static_cast<int>(weights.size())) {
            weights[index] = w;
            gpu_dirty = true;
        }
    }
};

} // namespace dse

#endif // DSE_COMPONENTS_3D_RENDER_H
