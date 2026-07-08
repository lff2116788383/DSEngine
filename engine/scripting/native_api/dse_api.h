/**
 * @file dse_api.h
 * @brief DSEngine Native C ABI — Lua 与 C# 共享的底层引擎接口
 *
 * 纯 C 函数导出，消除 Lua / C# 两套绑定的重复逻辑。
 * C# 侧通过 Mono InternalCall 或 P/Invoke 调用。
 * Lua 侧 lua_binding_ecs_*.cpp 逐步迁移为调用本层函数。
 */

#ifndef DSE_API_H
#define DSE_API_H

#include <stdint.h>

#ifdef _WIN32
#  define DSE_CAPI __declspec(dllexport)
#else
#  define DSE_CAPI __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// API Version — 用于运行时 ABI 兼容性检测
// ============================================================

/// 返回 C ABI 版本号。每次 dse_api.h / dse_api.gen.h 的公共接口
/// 发生破坏性变更时递增。脚本宿主（Lua/C#）在加载 DLL 后应首先
/// 调用此函数，与编译时 DSE_API_VERSION 比较，不一致则拒绝运行。
///   格式：MMNNPP（Major * 10000 + Minor * 100 + Patch）
DSE_CAPI uint32_t dse_api_version(void);

#define DSE_API_VERSION 10000u   /* v1.0.0 — MMNNPP format */

// ============================================================
// Context Setup — 引擎初始化时调用一次
// ============================================================

DSE_CAPI void dse_native_api_init(
    void* world,           // World*
    void* asset_manager,   // AssetManager*
    void* audio_system,    // AudioSystem* (可为 nullptr)
    void  (*quit_fn)(void),
    void  (*set_title_fn)(const char*),
    float (*get_fps_fn)(void),
    void  (*set_fps_fn)(float),
    int   (*get_draw_calls_fn)(void)   // 可为 nullptr
);

// 供生成代码（dse_api.gen.cpp）访问内部 World 指针
DSE_CAPI void* dse_get_world_ptr(void);

// ============================================================
// Entity
// ============================================================

DSE_CAPI uint32_t dse_entity_create(void);
DSE_CAPI void     dse_entity_destroy(uint32_t e);
DSE_CAPI int      dse_entity_valid(uint32_t e);

// ============================================================
// TransformComponent
// ============================================================

DSE_CAPI void dse_transform_add(uint32_t e,
    float x, float y, float z,
    float sx, float sy, float sz);

DSE_CAPI void dse_transform_get_position(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void dse_transform_set_position(uint32_t e, float x, float y, float z);

DSE_CAPI void dse_transform_get_rotation(uint32_t e, float* x, float* y, float* z); // Euler degrees
DSE_CAPI void dse_transform_set_rotation(uint32_t e, float x, float y, float z);    // Euler degrees

DSE_CAPI void dse_transform_get_scale(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void dse_transform_set_scale(uint32_t e, float x, float y, float z);

// ============================================================
// Camera3DComponent
// ============================================================

DSE_CAPI void  dse_camera3d_add(uint32_t e, float fov, float near_clip, float far_clip);
DSE_CAPI float dse_camera3d_get_fov(uint32_t e);
DSE_CAPI void  dse_camera3d_set_fov(uint32_t e, float v);
DSE_CAPI float dse_camera3d_get_near_clip(uint32_t e);
DSE_CAPI void  dse_camera3d_set_near_clip(uint32_t e, float v);
DSE_CAPI float dse_camera3d_get_far_clip(uint32_t e);
DSE_CAPI void  dse_camera3d_set_far_clip(uint32_t e, float v);
DSE_CAPI int   dse_camera3d_get_enabled(uint32_t e);
DSE_CAPI void  dse_camera3d_set_enabled(uint32_t e, int v);
DSE_CAPI int   dse_camera3d_get_priority(uint32_t e);
DSE_CAPI void  dse_camera3d_set_priority(uint32_t e, int v);

// ============================================================
// MeshRendererComponent
// ============================================================

DSE_CAPI void  dse_mesh_renderer_add(uint32_t e, const char* mesh_path);
// 手写 setter（capi_setter:manual）：设 mesh_path 并清空过程网格缓存；getter 由 codegen 生成
DSE_CAPI void  dse_mesh_renderer_set_mesh_path(uint32_t e, const char* mesh_path);
DSE_CAPI int   dse_mesh_renderer_get_mesh_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI void  dse_mesh_renderer_set_shader_variant(uint32_t e, const char* v);
DSE_CAPI int   dse_mesh_renderer_get_shader_variant(uint32_t e, char* buf, int buf_size);
DSE_CAPI void  dse_mesh_renderer_get_color(uint32_t e, float* r, float* g, float* b, float* a);
DSE_CAPI void  dse_mesh_renderer_set_color(uint32_t e, float r, float g, float b, float a);
DSE_CAPI int   dse_mesh_renderer_get_visible(uint32_t e);
DSE_CAPI void  dse_mesh_renderer_set_visible(uint32_t e, int v);
DSE_CAPI float dse_mesh_renderer_get_metallic(uint32_t e);
DSE_CAPI void  dse_mesh_renderer_set_metallic(uint32_t e, float v);
DSE_CAPI float dse_mesh_renderer_get_roughness(uint32_t e);
DSE_CAPI void  dse_mesh_renderer_set_roughness(uint32_t e, float v);
DSE_CAPI void  dse_mesh_renderer_get_emissive(uint32_t e, float* r, float* g, float* b);
DSE_CAPI void  dse_mesh_renderer_set_emissive(uint32_t e, float r, float g, float b);
DSE_CAPI int   dse_mesh_renderer_get_receive_shadow(uint32_t e);
DSE_CAPI void  dse_mesh_renderer_set_receive_shadow(uint32_t e, int v);

// ============================================================
// DirectionalLight3DComponent
// ============================================================

DSE_CAPI void  dse_dir_light_add(uint32_t e);
DSE_CAPI void  dse_dir_light_get_direction(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void  dse_dir_light_set_direction(uint32_t e, float x, float y, float z);
DSE_CAPI void  dse_dir_light_get_color(uint32_t e, float* r, float* g, float* b);
DSE_CAPI void  dse_dir_light_set_color(uint32_t e, float r, float g, float b);
DSE_CAPI float dse_dir_light_get_intensity(uint32_t e);
DSE_CAPI void  dse_dir_light_set_intensity(uint32_t e, float v);
DSE_CAPI float dse_dir_light_get_ambient_intensity(uint32_t e);
DSE_CAPI void  dse_dir_light_set_ambient_intensity(uint32_t e, float v);
DSE_CAPI int   dse_dir_light_get_cast_shadow(uint32_t e);
DSE_CAPI void  dse_dir_light_set_cast_shadow(uint32_t e, int v);
DSE_CAPI float dse_dir_light_get_shadow_strength(uint32_t e);
DSE_CAPI void  dse_dir_light_set_shadow_strength(uint32_t e, float v);
DSE_CAPI int   dse_dir_light_get_enabled(uint32_t e);
DSE_CAPI void  dse_dir_light_set_enabled(uint32_t e, int v);
// S1.8 Tier C：复合阴影参数 setter，封装 cascade 级联约束（split[i] ≥ split[i-1]+0.1）+ clamp；
//             手写实现见 dse_api.cpp（非 codegen）。传入值由调用方合并好现值。
DSE_CAPI void  dse_dir_light_set_shadow_params(uint32_t e, int cast_shadow, float shadow_strength,
                                               float c0, float c1, float c2, float lambda);

// ============================================================
// PointLightComponent
// ============================================================

DSE_CAPI void  dse_point_light_add(uint32_t e);
DSE_CAPI void  dse_point_light_get_color(uint32_t e, float* r, float* g, float* b);
DSE_CAPI void  dse_point_light_set_color(uint32_t e, float r, float g, float b);
DSE_CAPI float dse_point_light_get_intensity(uint32_t e);
DSE_CAPI void  dse_point_light_set_intensity(uint32_t e, float v);
DSE_CAPI float dse_point_light_get_radius(uint32_t e);
DSE_CAPI void  dse_point_light_set_radius(uint32_t e, float v);
DSE_CAPI int   dse_point_light_get_enabled(uint32_t e);
DSE_CAPI void  dse_point_light_set_enabled(uint32_t e, int v);
DSE_CAPI int   dse_point_light_get_cast_shadow(uint32_t e);
DSE_CAPI void  dse_point_light_set_cast_shadow(uint32_t e, int v);

// ============================================================
// SpotLightComponent
// ============================================================

DSE_CAPI void  dse_spot_light_add(uint32_t e);
DSE_CAPI void  dse_spot_light_get_color(uint32_t e, float* r, float* g, float* b);
DSE_CAPI void  dse_spot_light_set_color(uint32_t e, float r, float g, float b);
DSE_CAPI float dse_spot_light_get_intensity(uint32_t e);
DSE_CAPI void  dse_spot_light_set_intensity(uint32_t e, float v);
DSE_CAPI float dse_spot_light_get_radius(uint32_t e);
DSE_CAPI void  dse_spot_light_set_radius(uint32_t e, float v);
DSE_CAPI float dse_spot_light_get_inner_cone_angle(uint32_t e);
DSE_CAPI void  dse_spot_light_set_inner_cone_angle(uint32_t e, float v);
DSE_CAPI float dse_spot_light_get_outer_cone_angle(uint32_t e);
DSE_CAPI void  dse_spot_light_set_outer_cone_angle(uint32_t e, float v);
DSE_CAPI void  dse_spot_light_get_direction(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void  dse_spot_light_set_direction(uint32_t e, float x, float y, float z);
DSE_CAPI int   dse_spot_light_get_enabled(uint32_t e);
DSE_CAPI void  dse_spot_light_set_enabled(uint32_t e, int v);
DSE_CAPI int   dse_spot_light_get_cast_shadow(uint32_t e);
DSE_CAPI void  dse_spot_light_set_cast_shadow(uint32_t e, int v);

// ============================================================
// SkyLightComponent
// ============================================================

DSE_CAPI void  dse_sky_light_add(uint32_t e);
DSE_CAPI void  dse_sky_light_get_up_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void  dse_sky_light_set_up_color(uint32_t e, float x, float y, float z);
DSE_CAPI void  dse_sky_light_get_down_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void  dse_sky_light_set_down_color(uint32_t e, float x, float y, float z);
DSE_CAPI float dse_sky_light_get_intensity(uint32_t e);
DSE_CAPI void  dse_sky_light_set_intensity(uint32_t e, float v);
DSE_CAPI int   dse_sky_light_get_enabled(uint32_t e);
DSE_CAPI void  dse_sky_light_set_enabled(uint32_t e, int v);

// ============================================================
// TreeComponent
// ============================================================

DSE_CAPI int   dse_tree_get_enabled(uint32_t e);
DSE_CAPI void  dse_tree_set_enabled(uint32_t e, int v);
DSE_CAPI float dse_tree_get_density(uint32_t e);
DSE_CAPI void  dse_tree_set_density(uint32_t e, float v);
DSE_CAPI float dse_tree_get_spawn_radius(uint32_t e);
DSE_CAPI void  dse_tree_set_spawn_radius(uint32_t e, float v);
DSE_CAPI float dse_tree_get_chunk_size(uint32_t e);
DSE_CAPI void  dse_tree_set_chunk_size(uint32_t e, float v);
DSE_CAPI float dse_tree_get_min_scale(uint32_t e);
DSE_CAPI void  dse_tree_set_min_scale(uint32_t e, float v);
DSE_CAPI float dse_tree_get_max_scale(uint32_t e);
DSE_CAPI void  dse_tree_set_max_scale(uint32_t e, float v);
DSE_CAPI float dse_tree_get_lod1_distance(uint32_t e);
DSE_CAPI void  dse_tree_set_lod1_distance(uint32_t e, float v);
DSE_CAPI float dse_tree_get_cull_distance(uint32_t e);
DSE_CAPI void  dse_tree_set_cull_distance(uint32_t e, float v);
DSE_CAPI float dse_tree_get_wind_strength(uint32_t e);
DSE_CAPI void  dse_tree_set_wind_strength(uint32_t e, float v);
DSE_CAPI float dse_tree_get_wind_speed(uint32_t e);
DSE_CAPI void  dse_tree_set_wind_speed(uint32_t e, float v);
DSE_CAPI int   dse_tree_get_cast_shadow(uint32_t e);
DSE_CAPI void  dse_tree_set_cast_shadow(uint32_t e, int v);
DSE_CAPI float dse_tree_get_shadow_distance(uint32_t e);
DSE_CAPI void  dse_tree_set_shadow_distance(uint32_t e, float v);
DSE_CAPI int   dse_tree_get_seed(uint32_t e);
DSE_CAPI void  dse_tree_set_seed(uint32_t e, int v);
DSE_CAPI float dse_tree_get_height_variation(uint32_t e);
DSE_CAPI void  dse_tree_set_height_variation(uint32_t e, float v);
DSE_CAPI int   dse_tree_get_random_rotation(uint32_t e);
DSE_CAPI void  dse_tree_set_random_rotation(uint32_t e, int v);
DSE_CAPI float dse_tree_get_billboard_distance(uint32_t e);
DSE_CAPI void  dse_tree_set_billboard_distance(uint32_t e, float v);

// ============================================================
// TerrainTileManagerComponent
// ============================================================

DSE_CAPI int   dse_terrain_tile_get_enabled(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_enabled(uint32_t e, int v);
DSE_CAPI float dse_terrain_tile_get_tile_world_size(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_tile_world_size(uint32_t e, float v);
DSE_CAPI int   dse_terrain_tile_get_tile_resolution(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_tile_resolution(uint32_t e, int v);
DSE_CAPI float dse_terrain_tile_get_max_height(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_max_height(uint32_t e, float v);
DSE_CAPI float dse_terrain_tile_get_load_radius(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_load_radius(uint32_t e, float v);
DSE_CAPI float dse_terrain_tile_get_unload_radius(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_unload_radius(uint32_t e, float v);
DSE_CAPI int   dse_terrain_tile_get_use_procedural(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_use_procedural(uint32_t e, int v);
DSE_CAPI float dse_terrain_tile_get_procedural_base_height(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_procedural_base_height(uint32_t e, float v);
DSE_CAPI int   dse_terrain_tile_get_max_lod_levels(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_max_lod_levels(uint32_t e, int v);
DSE_CAPI float dse_terrain_tile_get_lod_distance_factor(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_lod_distance_factor(uint32_t e, float v);

// ============================================================
// DynamicObstacleComponent
// ============================================================

DSE_CAPI int   dse_dyn_obstacle_get_enabled(uint32_t e);
DSE_CAPI void  dse_dyn_obstacle_set_enabled(uint32_t e, int v);
DSE_CAPI int   dse_dyn_obstacle_get_shape(uint32_t e);   ///< 0=Box, 1=Cylinder
DSE_CAPI void  dse_dyn_obstacle_set_shape(uint32_t e, int v);
DSE_CAPI void  dse_dyn_obstacle_get_box_extents(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void  dse_dyn_obstacle_set_box_extents(uint32_t e, float x, float y, float z);
DSE_CAPI float dse_dyn_obstacle_get_cylinder_radius(uint32_t e);
DSE_CAPI void  dse_dyn_obstacle_set_cylinder_radius(uint32_t e, float v);
DSE_CAPI float dse_dyn_obstacle_get_cylinder_height(uint32_t e);
DSE_CAPI void  dse_dyn_obstacle_set_cylinder_height(uint32_t e, float v);

// ============================================================
// NavMeshAutoRebakeComponent
// ============================================================

DSE_CAPI int   dse_navmesh_rebake_get_enabled(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_enabled(uint32_t e, int v);
DSE_CAPI float dse_navmesh_rebake_get_tile_size(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_tile_size(uint32_t e, float v);
DSE_CAPI float dse_navmesh_rebake_get_rebake_cooldown(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_rebake_cooldown(uint32_t e, float v);
DSE_CAPI int   dse_navmesh_rebake_get_collect_terrain(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_collect_terrain(uint32_t e, int v);
DSE_CAPI int   dse_navmesh_rebake_get_collect_mesh_renderers(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_collect_mesh_renderers(uint32_t e, int v);
DSE_CAPI float dse_navmesh_rebake_get_agent_height(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_agent_height(uint32_t e, float v);
DSE_CAPI float dse_navmesh_rebake_get_agent_radius(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_agent_radius(uint32_t e, float v);
DSE_CAPI float dse_navmesh_rebake_get_agent_max_climb(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_agent_max_climb(uint32_t e, float v);
DSE_CAPI float dse_navmesh_rebake_get_agent_max_slope(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_agent_max_slope(uint32_t e, float v);
DSE_CAPI float dse_navmesh_rebake_get_cell_size(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_cell_size(uint32_t e, float v);
DSE_CAPI float dse_navmesh_rebake_get_cell_height(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_cell_height(uint32_t e, float v);

// ============================================================
// TreeComponent — string paths（实现见 dse_api.gen.cpp）
// ============================================================

DSE_CAPI void dse_tree_set_mesh_path(uint32_t e, const char* path);
DSE_CAPI int  dse_tree_get_mesh_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI void dse_tree_set_lod1_mesh_path(uint32_t e, const char* path);
DSE_CAPI int  dse_tree_get_lod1_mesh_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI void dse_tree_set_billboard_texture_path(uint32_t e, const char* path);
DSE_CAPI int  dse_tree_get_billboard_texture_path(uint32_t e, char* buf, int buf_size);

// ============================================================
// PostProcessComponent — 每字段访问器（实现见 dse_api_post_process.gen.cpp）
// ============================================================

DSE_CAPI int  dse_post_process_get_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_enabled(uint32_t e, int v);
DSE_CAPI int  dse_post_process_get_bloom_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_bloom_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_bloom_threshold(uint32_t e);
DSE_CAPI void  dse_post_process_set_bloom_threshold(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_bloom_intensity(uint32_t e);
DSE_CAPI void  dse_post_process_set_bloom_intensity(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_bloom_knee(uint32_t e);
DSE_CAPI void  dse_post_process_set_bloom_knee(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_bloom_mip_weight(uint32_t e);
DSE_CAPI void  dse_post_process_set_bloom_mip_weight(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_color_grading_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_color_grading_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_exposure(uint32_t e);
DSE_CAPI void  dse_post_process_set_exposure(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_gamma(uint32_t e);
DSE_CAPI void  dse_post_process_set_gamma(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_ssao_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_ssao_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_ssao_radius(uint32_t e);
DSE_CAPI void  dse_post_process_set_ssao_radius(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_ssao_bias(uint32_t e);
DSE_CAPI void  dse_post_process_set_ssao_bias(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_ssao_sample_count(uint32_t e);
DSE_CAPI void dse_post_process_set_ssao_sample_count(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_ssao_power(uint32_t e);
DSE_CAPI void  dse_post_process_set_ssao_power(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_ssao_intensity(uint32_t e);
DSE_CAPI void  dse_post_process_set_ssao_intensity(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_auto_exposure_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_auto_exposure_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_exposure_min(uint32_t e);
DSE_CAPI void  dse_post_process_set_exposure_min(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_exposure_max(uint32_t e);
DSE_CAPI void  dse_post_process_set_exposure_max(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_adaptation_speed_up(uint32_t e);
DSE_CAPI void  dse_post_process_set_adaptation_speed_up(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_adaptation_speed_down(uint32_t e);
DSE_CAPI void  dse_post_process_set_adaptation_speed_down(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_exposure_compensation(uint32_t e);
DSE_CAPI void  dse_post_process_set_exposure_compensation(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_color_lut_intensity(uint32_t e);
DSE_CAPI void  dse_post_process_set_color_lut_intensity(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_vignette_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_vignette_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_vignette_intensity(uint32_t e);
DSE_CAPI void  dse_post_process_set_vignette_intensity(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_vignette_radius(uint32_t e);
DSE_CAPI void  dse_post_process_set_vignette_radius(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_vignette_softness(uint32_t e);
DSE_CAPI void  dse_post_process_set_vignette_softness(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_film_grain_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_film_grain_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_film_grain_intensity(uint32_t e);
DSE_CAPI void  dse_post_process_set_film_grain_intensity(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_film_grain_time_scale(uint32_t e);
DSE_CAPI void  dse_post_process_set_film_grain_time_scale(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_fxaa_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_fxaa_enabled(uint32_t e, int v);
DSE_CAPI int  dse_post_process_get_taa_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_taa_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_taa_blend_factor(uint32_t e);
DSE_CAPI void  dse_post_process_set_taa_blend_factor(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_contact_shadow_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_contact_shadow_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_contact_shadow_strength(uint32_t e);
DSE_CAPI void  dse_post_process_set_contact_shadow_strength(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_contact_shadow_steps(uint32_t e);
DSE_CAPI void dse_post_process_set_contact_shadow_steps(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_contact_shadow_step_size(uint32_t e);
DSE_CAPI void  dse_post_process_set_contact_shadow_step_size(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_dof_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_dof_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_dof_focus_distance(uint32_t e);
DSE_CAPI void  dse_post_process_set_dof_focus_distance(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_dof_focus_range(uint32_t e);
DSE_CAPI void  dse_post_process_set_dof_focus_range(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_dof_bokeh_radius(uint32_t e);
DSE_CAPI void  dse_post_process_set_dof_bokeh_radius(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_motion_blur_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_motion_blur_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_motion_blur_intensity(uint32_t e);
DSE_CAPI void  dse_post_process_set_motion_blur_intensity(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_motion_blur_samples(uint32_t e);
DSE_CAPI void dse_post_process_set_motion_blur_samples(uint32_t e, int v);
DSE_CAPI int  dse_post_process_get_ssr_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_ssr_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_ssr_max_distance(uint32_t e);
DSE_CAPI void  dse_post_process_set_ssr_max_distance(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_ssr_thickness(uint32_t e);
DSE_CAPI void  dse_post_process_set_ssr_thickness(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_ssr_step_size(uint32_t e);
DSE_CAPI void  dse_post_process_set_ssr_step_size(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_ssr_max_steps(uint32_t e);
DSE_CAPI void dse_post_process_set_ssr_max_steps(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_ssr_fade_distance(uint32_t e);
DSE_CAPI void  dse_post_process_set_ssr_fade_distance(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_ssr_max_roughness(uint32_t e);
DSE_CAPI void  dse_post_process_set_ssr_max_roughness(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_outline_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_outline_enabled(uint32_t e, int v);
DSE_CAPI void dse_post_process_get_outline_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void dse_post_process_set_outline_color(uint32_t e, float x, float y, float z);
DSE_CAPI float dse_post_process_get_outline_thickness(uint32_t e);
DSE_CAPI void  dse_post_process_set_outline_thickness(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_outline_depth_threshold(uint32_t e);
DSE_CAPI void  dse_post_process_set_outline_depth_threshold(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_outline_normal_threshold(uint32_t e);
DSE_CAPI void  dse_post_process_set_outline_normal_threshold(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_light_shaft_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_light_shaft_enabled(uint32_t e, int v);
DSE_CAPI void dse_post_process_get_light_shaft_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void dse_post_process_set_light_shaft_color(uint32_t e, float x, float y, float z);
DSE_CAPI float dse_post_process_get_light_shaft_density(uint32_t e);
DSE_CAPI void  dse_post_process_set_light_shaft_density(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_light_shaft_weight(uint32_t e);
DSE_CAPI void  dse_post_process_set_light_shaft_weight(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_light_shaft_decay(uint32_t e);
DSE_CAPI void  dse_post_process_set_light_shaft_decay(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_light_shaft_exposure(uint32_t e);
DSE_CAPI void  dse_post_process_set_light_shaft_exposure(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_light_shaft_intensity(uint32_t e);
DSE_CAPI void  dse_post_process_set_light_shaft_intensity(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_light_shaft_samples(uint32_t e);
DSE_CAPI void dse_post_process_set_light_shaft_samples(uint32_t e, int v);
DSE_CAPI int  dse_post_process_get_fog_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_fog_enabled(uint32_t e, int v);
DSE_CAPI void dse_post_process_get_fog_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void dse_post_process_set_fog_color(uint32_t e, float x, float y, float z);
DSE_CAPI float dse_post_process_get_fog_density(uint32_t e);
DSE_CAPI void  dse_post_process_set_fog_density(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_fog_height_falloff(uint32_t e);
DSE_CAPI void  dse_post_process_set_fog_height_falloff(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_fog_height_offset(uint32_t e);
DSE_CAPI void  dse_post_process_set_fog_height_offset(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_fog_start(uint32_t e);
DSE_CAPI void  dse_post_process_set_fog_start(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_fog_end(uint32_t e);
DSE_CAPI void  dse_post_process_set_fog_end(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_fog_steps(uint32_t e);
DSE_CAPI void dse_post_process_set_fog_steps(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_fog_sun_scatter(uint32_t e);
DSE_CAPI void  dse_post_process_set_fog_sun_scatter(uint32_t e, float v);

// ============================================================
// Animator3DComponent — S1.9 每字段访问器（实现见 dse_api_animator3d.gen.cpp）
// danim_path/dskel_path 为纯字符串字段：动画系统按路径值比较自动重载，setter 纯赋值无副作用。
// 复合/FSM/blend tree 仍手写于 lua_binding_ecs_animation.cpp，不在此。
// ============================================================
DSE_CAPI int   dse_animator3d_get_enabled(uint32_t e);
DSE_CAPI void  dse_animator3d_set_enabled(uint32_t e, int v);
DSE_CAPI void  dse_animator3d_set_danim_path(uint32_t e, const char* v);
DSE_CAPI int   dse_animator3d_get_danim_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI void  dse_animator3d_set_dskel_path(uint32_t e, const char* v);
DSE_CAPI int   dse_animator3d_get_dskel_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI float dse_animator3d_get_speed(uint32_t e);
DSE_CAPI void  dse_animator3d_set_speed(uint32_t e, float v);
DSE_CAPI int   dse_animator3d_get_loop(uint32_t e);
DSE_CAPI void  dse_animator3d_set_loop(uint32_t e, int v);
DSE_CAPI int   dse_animator3d_get_use_anim_tree(uint32_t e);
DSE_CAPI void  dse_animator3d_set_use_anim_tree(uint32_t e, int v);
DSE_CAPI void  dse_animator3d_set_blend_parameter(uint32_t e, const char* v);
DSE_CAPI int   dse_animator3d_get_blend_parameter(uint32_t e, char* buf, int buf_size);
DSE_CAPI float dse_animator3d_get_blend_parameter_value(uint32_t e);
DSE_CAPI void  dse_animator3d_set_blend_parameter_value(uint32_t e, float v);

// ============================================================
// Physics3D 服务（L5，手写 dse_api_physics3d.cpp）
// 依赖 Physics3D 服务 + ECS 碰撞体回退，非纯组件字段，codegen 无法表达。
// raycast：direction 内部归一化；命中返回 1 并填充非空 out_*（point/normal 为 float[3]）。
// ============================================================
DSE_CAPI int dse_physics3d_raycast(float ox, float oy, float oz,
                                   float dx, float dy, float dz,
                                   float max_dist,
                                   uint32_t* out_entity,
                                   float* out_point,
                                   float* out_normal,
                                   float* out_distance);

// RigidBody3D 动力学（服务委托 + 组件缓存回退）。
// add_force/add_impulse/add_torque/set_angular_velocity：仅在物理服务存在时生效，否则 no-op。
// set_velocity/set_gravity：委托服务（若有）并同步组件缓存。
// get_velocity：服务优先，否则回退组件缓存；get_angular_velocity：服务优先，否则 0。
DSE_CAPI void dse_rigidbody3d_add_force(uint32_t e, float fx, float fy, float fz);
DSE_CAPI void dse_rigidbody3d_add_impulse(uint32_t e, float ix, float iy, float iz);
DSE_CAPI void dse_rigidbody3d_add_torque(uint32_t e, float tx, float ty, float tz);
DSE_CAPI void dse_rigidbody3d_set_velocity(uint32_t e, float vx, float vy, float vz);
DSE_CAPI void dse_rigidbody3d_get_velocity(uint32_t e, float* out_vel);          // float[3]
DSE_CAPI void dse_rigidbody3d_set_angular_velocity(uint32_t e, float ax, float ay, float az);
DSE_CAPI void dse_rigidbody3d_get_angular_velocity(uint32_t e, float* out_vel);  // float[3]
DSE_CAPI void dse_rigidbody3d_set_gravity(uint32_t e, int enabled);

// CharacterController3D。move：服务优先（含 ECS 地形贴地补正），否则 ECS 回退
// （碰撞体推开 + 地形贴地 + 着地检测）。返回 is_grounded(0/1)，填充非空 out_velocity[3] 与 out_flags。
// jump：仅在物理服务存在时生效，返回 success(0/1)。
DSE_CAPI int dse_character_controller3d_move(uint32_t e, float dx, float dy, float dz,
                                             float min_dist, float dt,
                                             float* out_velocity, uint32_t* out_flags);
DSE_CAPI int dse_character_controller3d_jump(uint32_t e, float jump_speed);
// is_grounded/get_position：服务优先，否则 ECS 回退（组件缓存 / Transform）。
DSE_CAPI int dse_character_controller3d_is_grounded(uint32_t e);
DSE_CAPI int dse_character_controller3d_get_position(uint32_t e, float* out_xyz); // float[3]，返回 found(0/1)

// 组件创建（L3，ECS emplace_or_replace）。type/direction 为对应枚举的整型值。
DSE_CAPI void dse_rigidbody3d_add(uint32_t e, int type, float mass);
DSE_CAPI void dse_box_collider3d_add(uint32_t e, float x, float y, float z);
DSE_CAPI void dse_sphere_collider3d_add(uint32_t e, float radius);
DSE_CAPI void dse_capsule_collider3d_add(uint32_t e, float radius, float height,
                                         int direction, int is_trigger);
DSE_CAPI void dse_mesh_collider3d_add(uint32_t e, int convex, int is_trigger);
DSE_CAPI void dse_character_controller3d_add(uint32_t e, float radius, float height,
                                             float slope_limit, float step_offset);
DSE_CAPI void dse_joint3d_add(uint32_t e, uint32_t connected_id, int type,
                              float ax, float ay, float az,
                              float bx, float by, float bz,
                              float break_force, float break_torque);
DSE_CAPI void dse_terrain_heightmap_add(uint32_t e, float origin_x, float origin_z,
                                        float block_size, int cols, int rows,
                                        float scale, int flip_z);

// TerrainHeightmap 数据写入 / 查询。
DSE_CAPI void dse_terrain_heightmap_set_data(uint32_t e, const float* heights, int count);
DSE_CAPI int dse_terrain_get_height(float world_x, float world_z, float* out_y); // 返回 found(0/1)

// Joint3D 附加参数 setter / 查询。
DSE_CAPI void dse_joint3d_set_hinge_limits(uint32_t e, float lower_deg, float upper_deg);
DSE_CAPI void dse_joint3d_set_spring(uint32_t e, float stiffness, float damping);
DSE_CAPI void dse_joint3d_set_distance(uint32_t e, float min_dist, float max_dist);
DSE_CAPI int dse_joint3d_is_broken(uint32_t e);

// 碰撞层 / trigger / 材质。set_layer 写 RigidBody 并委托物理服务（若有）。
// set_trigger/set_material 写入实体上存在的任意碰撞体类型（Box/Sphere/Capsule/Mesh）。
DSE_CAPI void dse_collision_set_layer(uint32_t e, int layer, int mask);
DSE_CAPI void dse_collider_set_trigger(uint32_t e, int is_trigger);
DSE_CAPI void dse_collider_set_material(uint32_t e, float friction, float bounciness);

// 重叠查询：写入命中实体 id 到 out（容量 cap），返回命中总数（可能 > cap）。
DSE_CAPI int dse_physics3d_overlap_sphere(float cx, float cy, float cz, float radius,
                                          uint32_t* out, int cap);
DSE_CAPI int dse_physics3d_overlap_box(float min_x, float min_y, float min_z,
                                       float max_x, float max_y, float max_z,
                                       uint32_t* out, int cap);

// ============================================================
// Render 服务（L5，手写 dse_api_render.cpp）
// ============================================================
// world_to_screen：把 3D 世界坐标投影到屏幕像素，填充非空 out_sx/out_sy，返回 is_visible(0/1)。
DSE_CAPI int dse_render_world_to_screen(float wx, float wy, float wz,
                                        float* out_sx, float* out_sy);

// screen_to_world_ray：由屏幕像素 (sx,sy) 用主相机反投影出世界空间拾取射线。
// 填充非空 out_origin[3]（射线起点=相机位置）与 out_dir[3]（已归一化方向）。
// 成功（存在启用的主相机）返回 1，否则返回 0 且不修改输出。
DSE_CAPI int dse_render_screen_to_world_ray(float sx, float sy,
                                            float* out_origin, float* out_dir);

// MeshRenderer 材质/贴图加载（依赖 AssetManager）。
// set_material_from_dmat：从 .dmat 载入 MaterialInstance 并拷入 MeshRenderer，成功返回 1。
// set_texture：按 slot 名载入贴图并绑定到对应 handle，成功返回 1 并填充非空 out_*；slot 非法/加载失败返回 0。
DSE_CAPI int dse_mesh_renderer_set_material_from_dmat(uint32_t e, const char* dmat_path,
                                                      uint32_t material_index);
DSE_CAPI int dse_mesh_renderer_set_texture(uint32_t e, const char* slot, const char* path,
                                           uint32_t* out_handle, int* out_width, int* out_height);

// MeshRenderer 过程网格创作 + 材质创作（手写 dse_api_render.cpp）。
// add_procedural：emplace_or_replace 组件并写入过程顶点（xyz 扁平）与索引（越界索引丢弃）。
// set_material_params：浮点 NaN=保持当前值；receive_shadow/double_sided：-1=保持当前值。
// set_material_scalar：按名写标量并把材质数据源标记为 ComponentFallback；未知名仅标记。
// set_advanced_material：写高级材质参数并标记 ComponentFallback。
// set_uvs/set_normals/set_tangents：写入过程网格属性缓冲；返回属性数量与顶点数是否匹配(0/1)，
//   填充非空 out_attr_count（属性个数）与 out_vertex_count。
// set_emissive_authoring：写 emissive 并标记 ComponentFallback。
DSE_CAPI void dse_mesh_renderer_add_procedural(uint32_t e, float r, float g, float b, float a,
                                               const float* vertices, int vertex_float_count,
                                               const int* indices, int index_count);
DSE_CAPI void dse_mesh_renderer_set_material_params(uint32_t e, float metallic, float roughness,
                                                    float ao, float er, float eg, float eb,
                                                    float normal_strength,
                                                    int receive_shadow, int double_sided,
                                                    float cr, float cg, float cb, float ca);
DSE_CAPI void dse_mesh_renderer_set_depth_state(uint32_t e, int depth_test, int depth_write);
DSE_CAPI void dse_mesh_renderer_set_material_scalar(uint32_t e, const char* name, float value);
DSE_CAPI void dse_mesh_renderer_set_advanced_material(uint32_t e, float clear_coat,
                                                      float clear_coat_roughness, float anisotropy,
                                                      float pom_height_scale, float sss_strength,
                                                      float sss_r, float sss_g, float sss_b);
DSE_CAPI int  dse_mesh_renderer_set_uvs(uint32_t e, const float* uvs, int count,
                                        int* out_attr_count, int* out_vertex_count);
DSE_CAPI int  dse_mesh_renderer_set_normals(uint32_t e, const float* normals, int count,
                                            int* out_attr_count, int* out_vertex_count);
DSE_CAPI int  dse_mesh_renderer_set_tangents(uint32_t e, const float* tangents, int count,
                                             int* out_attr_count, int* out_vertex_count);
DSE_CAPI void dse_mesh_renderer_set_emissive_authoring(uint32_t e, float r, float g, float b);

// ---- Morph 简单权重组件（MorphComponent，区别于 MorphTargetComponent 的 dse_morph_*）。 ----
DSE_CAPI void  dse_morph_simple_add(uint32_t e);
DSE_CAPI void  dse_morph_simple_add_target(uint32_t e, const char* name, float weight);
DSE_CAPI void  dse_morph_simple_set_weight(uint32_t e, const char* name, float w);
DSE_CAPI void  dse_morph_simple_set_weight_index(uint32_t e, int idx, float w);
DSE_CAPI float dse_morph_simple_get_weight(uint32_t e, const char* name);
DSE_CAPI float dse_morph_simple_get_weight_index(uint32_t e, int idx);
DSE_CAPI void  dse_morph_simple_set_enabled(uint32_t e, int enabled);

// 供 L5 手写实现（dse_api_render.cpp）访问内部 AssetManager 指针
DSE_CAPI void* dse_get_asset_manager_ptr(void);

// ============================================================
// Gameplay3D（破碎 / 布料 / 流体，手写 dse_api_gameplay3d.cpp）
// ============================================================
// 纯 ECS 组件控制，操作全局 World。可选「保持当前值」字段以 NaN 哨兵表示；
// 固定默认值由调用方（Lua 薄包装）解析后传入。

// Fracture：source 0=Prefractured 1=RuntimeVoronoi。
DSE_CAPI void dse_fracture_add(uint32_t e, int source, uint32_t fragment_count,
                               float break_force, float health);
DSE_CAPI void dse_fracture_set_params(uint32_t e, float explosion_force, float fragment_lifetime,
                                      float fade_duration, float mass_scale);  // NaN=保持
DSE_CAPI void dse_fracture_apply_damage(uint32_t e, float damage, float ix, float iy, float iz);
DSE_CAPI void dse_fracture_trigger(uint32_t e, float ix, float iy, float iz);
DSE_CAPI int  dse_fracture_is_fractured(uint32_t e);

// Cloth。
DSE_CAPI void dse_cloth_add(uint32_t e, uint32_t solver_iterations, float stiffness,
                            float damping, float bend_stiffness);
DSE_CAPI void dse_cloth_set_wind(uint32_t e, float wx, float wy, float wz, float turbulence);  // turbulence NaN=保持
DSE_CAPI void dse_cloth_set_gravity(uint32_t e, float gx, float gy, float gz);
DSE_CAPI void dse_cloth_pin_vertices(uint32_t e, const uint32_t* vertices, int count);
DSE_CAPI void dse_cloth_add_sphere_collider(uint32_t e, uint32_t collider_entity, float radius);

// Fluid：shape 0=Point 1=Sphere 2=Box。
DSE_CAPI void dse_fluid_add_emitter(uint32_t e, int shape, float emission_rate,
                                    float particle_lifetime, float emit_speed);
DSE_CAPI void dse_fluid_set_physics(uint32_t e, float viscosity, float surface_tension,
                                    float rest_density, float gas_stiffness);  // NaN=保持
DSE_CAPI void dse_fluid_set_rendering(uint32_t e, float r, float g, float b, float a,
                                      float refraction, float fresnel, float specular);  // refraction/fresnel/specular NaN=保持
DSE_CAPI void dse_fluid_set_emit_direction(uint32_t e, float dx, float dy, float dz, float spread);  // spread NaN=保持
DSE_CAPI void dse_fluid_set_floor(uint32_t e, float floor_y, float restitution);  // NaN=保持
DSE_CAPI uint32_t dse_fluid_get_particle_count(uint32_t e);

// Ragdoll（仅设标志，激活由 RagdollSystem 处理）。auto_setup/active 用 int(0/1)。
DSE_CAPI void dse_ragdoll_add(uint32_t e, float total_mass, int auto_setup,
                              float joint_stiffness, float joint_damping);
DSE_CAPI void dse_ragdoll_activate(uint32_t e);
DSE_CAPI void dse_ragdoll_deactivate(uint32_t e);
DSE_CAPI int  dse_ragdoll_is_active(uint32_t e);
// ragdoll 逐字段 get/set 由 dse_api_ragdoll.gen.cpp 提供（codegen）
DSE_CAPI void dse_ragdoll_set_collision_layer(uint32_t e, int v);  // codegen 逐字段
DSE_CAPI void dse_ragdoll_set_collision_layer_mask(uint32_t e, uint32_t layer, uint32_t mask);

// SoftBody。gravity_scale NaN=保持。
DSE_CAPI void dse_softbody_add(uint32_t e, float stiffness, int iterations,
                               float damping, float volume_stiffness);
DSE_CAPI void dse_softbody_set_gravity(uint32_t e, int use_gravity, float gravity_scale);
DSE_CAPI void dse_softbody_pin_vertex(uint32_t e, int vertex_index);
DSE_CAPI uint32_t dse_softbody_get_particle_count(uint32_t e);

// Vehicle（raycast 车辆）。set_input 内部 clamp 到合法范围。
DSE_CAPI void dse_vehicle_add(uint32_t e, float max_engine_force, float max_brake_force,
                              float max_steer_angle);
DSE_CAPI void dse_vehicle_add_wheel(uint32_t e, float px, float py, float pz, float radius,
                                    int is_drive, int is_steer, float susp_stiffness,
                                    float susp_damping);
DSE_CAPI void dse_vehicle_set_input(uint32_t e, float throttle, float brake, float steering);
DSE_CAPI float dse_vehicle_get_speed(uint32_t e);
DSE_CAPI uint32_t dse_vehicle_get_wheel_count(uint32_t e);

// Rope。get_positions：填充 out_xyz（最多 max_points 点×3 float），返回点总数；
// out_xyz=null 时仅返回总数供预分配。gravity_scale NaN=保持。
DSE_CAPI void dse_rope_add(uint32_t e, int segment_count, float segment_length,
                           float damping, int iterations);
DSE_CAPI void dse_rope_set_anchors(uint32_t e, uint32_t anchor_a, uint32_t anchor_b,
                                   float oax, float oay, float oaz,
                                   float obx, float oby, float obz);
DSE_CAPI int  dse_rope_get_positions(uint32_t e, float* out_xyz, int max_points);
DSE_CAPI void dse_rope_set_gravity(uint32_t e, int use_gravity, float gravity_scale);

// Buoyancy。
DSE_CAPI void dse_buoyancy_add(uint32_t e, float water_level, float buoyancy_force,
                               float water_drag, float angular_drag, float submerge_depth);
DSE_CAPI void dse_buoyancy_add_sample_point(uint32_t e, float ox, float oy, float oz,
                                            float force_scale);
DSE_CAPI void dse_buoyancy_set_water_level(uint32_t e, float water_level);  // codegen 逐字段
DSE_CAPI float dse_buoyancy_get_submerge_ratio(uint32_t e);
DSE_CAPI void dse_buoyancy_set_use_fluid(uint32_t e, int use_fluid);

// ---- Batch 3 环境子系统（无物理依赖）。浮点 NaN=保持当前值。 ----
// Weather。type: 0=None,1=Rain,2=Snow；set 中 type<0=保持。spawn 中 max_particles<0=保持。
DSE_CAPI void dse_weather_add(uint32_t e, int type, float intensity);
DSE_CAPI void dse_weather_set(uint32_t e, int type, float intensity,
                              float wind_x, float wind_z);
DSE_CAPI void dse_weather_set_spawn(uint32_t e, float radius, float height,
                                    int max_particles);

// SnowCover。snow_cover_get 填充 out_*（可为 null），返回 1=存在/0=缺失。
// set_texture：path=null 仅改 tiling。
DSE_CAPI void dse_snow_cover_add(uint32_t e);
DSE_CAPI void dse_snow_cover_set(uint32_t e, float target_coverage,
                                 float accumulation_rate, float melt_rate);
DSE_CAPI void dse_snow_set_appearance(uint32_t e, float albedo_r, float albedo_g,
                                      float albedo_b, float roughness, float metallic,
                                      float threshold, float sharpness);
DSE_CAPI int  dse_snow_cover_get(uint32_t e, float* out_coverage,
                                 float* out_target, int* out_enabled);
DSE_CAPI void dse_snow_cover_set_enabled(uint32_t e, int enabled);
DSE_CAPI void dse_snow_set_texture(uint32_t e, const char* path, float tiling);
DSE_CAPI void dse_snow_set_displacement(uint32_t e, float displacement_height,
                                        float deformation_strength);
DSE_CAPI void dse_snow_cover_remove(uint32_t e);

// Atmosphere（物理天空参数）。
DSE_CAPI void dse_atmosphere_add(uint32_t e);
DSE_CAPI void dse_atmosphere_set_params(uint32_t e, float planet_radius,
                                        float atmosphere_height, float sun_disk_angle);
DSE_CAPI void dse_atmosphere_set_rayleigh(uint32_t e, float coeff_r, float coeff_g,
                                          float coeff_b, float scale_height);
DSE_CAPI void dse_atmosphere_set_mie(uint32_t e, float coeff, float scale_height, float g);
DSE_CAPI void dse_atmosphere_set_sun_intensity(uint32_t e, float x, float y, float z);  // codegen 逐字段 vec3

// DayNightCycle。set_location 中 day_of_year<=0=保持。get_sun_direction 填充 out_xyz(3)。
DSE_CAPI void dse_day_night_add(uint32_t e, float time_of_day, int auto_advance,
                                float time_speed);
DSE_CAPI void dse_day_night_set_time(uint32_t e, float time_of_day);
DSE_CAPI float dse_day_night_get_time(uint32_t e);
DSE_CAPI void dse_day_night_set_speed(uint32_t e, float speed);
DSE_CAPI void dse_day_night_set_auto_advance(uint32_t e, int enabled);  // codegen 逐字段
DSE_CAPI void dse_day_night_set_location(uint32_t e, float latitude, float longitude,
                                         int day_of_year);
DSE_CAPI float dse_day_night_get_sun_elevation(uint32_t e);
DSE_CAPI void dse_day_night_get_sun_direction(uint32_t e, float* out_xyz);

// VolumetricCloud。
DSE_CAPI void dse_volumetric_cloud_add(uint32_t e);
DSE_CAPI void dse_cloud_set_layer(uint32_t e, float bottom, float top,
                                  float coverage, float density);
DSE_CAPI void dse_cloud_set_wind(uint32_t e, float dir_x, float dir_y, float speed);

// ============================================================
// 动画子系统（L4/L5，纯 ECS）。浮点 NaN=保持当前值。
// ============================================================

// ---- 2D 帧动画（AnimatorComponent）。add_state: loop 为 0/1；frame_handles 可为 null。
// pop_event 将事件名写入 out（null 结尾，按 cap 截断），返回 1=弹出/0=无。 ----
DSE_CAPI void dse_anim2d_add(uint32_t e);
DSE_CAPI void dse_anim2d_add_state(uint32_t e, const char* name, float fps, int loop,
                                   const uint32_t* frame_handles, int handle_count);
DSE_CAPI void dse_anim2d_add_event(uint32_t e, const char* state_name,
                                   float normalized_time, const char* event_name);
DSE_CAPI void dse_anim2d_play(uint32_t e, const char* state_name);
DSE_CAPI void dse_anim2d_play_segment(uint32_t e, int start_frame, int end_frame, int loop);
DSE_CAPI int  dse_anim2d_pop_event(uint32_t e, char* out, int cap);

// ---- 3D 骨骼动画 / 状态机（Animator3DComponent）。
// set_state: state_name=null 不改状态，speed=NaN 保持，loop<0 保持。
// get_state: 填充 out_*（均可为 null），返回 1=存在/0=缺失。
// add_transition: 条件以并行扁平数组传入（names/modes/thresholds/ints，长度 cond_count）。 ----
DSE_CAPI void dse_anim3d_add(uint32_t e, const char* danim_path, const char* dskel_path);
DSE_CAPI void dse_anim3d_set_state(uint32_t e, const char* state_name, float speed, int loop);
DSE_CAPI int  dse_anim3d_get_state(uint32_t e, char* out_state, int state_cap,
                                   float* out_norm, float* out_time, float* out_speed,
                                   int* out_loop, int* out_transitioning,
                                   int* out_bone_count, int* out_has_skel);
DSE_CAPI void dse_anim3d_init_fsm(uint32_t e);
DSE_CAPI void dse_anim3d_add_fsm_state(uint32_t e, const char* state_name,
                                       const char* danim_path, int loop, float speed);
DSE_CAPI void dse_anim3d_add_transition(uint32_t e, const char* from_state,
                                        const char* to_state, float transition_duration,
                                        int has_exit_time, float exit_time,
                                        int cond_count,
                                        const char* const* cond_names,
                                        const int* cond_modes,
                                        const float* cond_thresholds,
                                        const int* cond_ints);
// Lua 绑定薄包装：init_fsm 带可选 default_state；无条件 transition
DSE_CAPI void dse_compat_anim3d_init_fsm(uint32_t e, const char* default_state);
DSE_CAPI void dse_compat_anim3d_add_transition(uint32_t e, const char* from_state,
                                               const char* to_state, float transition_duration,
                                               int has_exit_time, float exit_time);
// ---- S1.9 3D compat 薄包装（定义见 dse_api_gameplay3d.cpp） ----
DSE_CAPI void dse_compat_weather_add(uint32_t e, const char* type, float intensity);
DSE_CAPI void dse_compat_set_directional_light_3d(uint32_t e, int enabled,
        float dx, float dy, float dz, float r, float g, float b,
        float intensity, float ambient, float shadow_strength);
DSE_CAPI void dse_compat_set_point_light_3d(uint32_t e, float r, float g, float b,
        float intensity, float radius);
DSE_CAPI void dse_compat_set_spot_light_3d(uint32_t e, float dx, float dy, float dz,
        float r, float g, float b, float intensity, float radius, float inner, float outer);
DSE_CAPI void dse_compat_world_to_screen(float wx, float wy, float wz,
        float* out_sx, float* out_sy, int* out_visible);
DSE_CAPI void dse_anim3d_set_param_float(uint32_t e, const char* param_name, float value);
DSE_CAPI void dse_anim3d_set_param_trigger(uint32_t e, const char* param_name);
DSE_CAPI void dse_anim3d_set_lock_root_motion(uint32_t e, int lock);
DSE_CAPI void dse_anim3d_add_event(uint32_t e, const char* event_name, float trigger_time);
DSE_CAPI int  dse_anim3d_pop_event(uint32_t e, char* out, int cap);
DSE_CAPI void dse_anim3d_set_extract_root_motion(uint32_t e, int enabled);
DSE_CAPI int  dse_anim3d_get_root_motion_delta(uint32_t e, float* out_xyz);

// ---- 动画层 / 混合树（AnimLayerComponent）。add 返回层索引（-1=无组件）。
// blend_tree_1d: paths/thresholds/speeds 并行数组，长度 count。 ----
DSE_CAPI void dse_animlayer_add_component(uint32_t e);
DSE_CAPI int  dse_animlayer_add(uint32_t e, const char* name, float weight, int blend_mode);
DSE_CAPI void dse_animlayer_set_clip(uint32_t e, int idx, const char* danim_path,
                                     float speed, int loop);
DSE_CAPI void dse_animlayer_set_weight(uint32_t e, int idx, float w);
DSE_CAPI void dse_animlayer_set_bone_mask(uint32_t e, int idx,
                                          const char* const* bones, int count);
DSE_CAPI void dse_animlayer_set_blend_tree_1d(uint32_t e, int idx,
                                              const char* const* paths,
                                              const float* thresholds,
                                              const float* speeds, int count);
DSE_CAPI void dse_animlayer_set_blend_param(uint32_t e, int idx, float val);
DSE_CAPI void dse_animlayer_set_enabled(uint32_t e, int enabled);

// ---- IK（IKChain3DComponent）。add_chain 返回链索引（-1=无组件）。
// set_target_entity: target=UINT32_MAX 清除目标实体。 ----
DSE_CAPI void dse_ik_add_component(uint32_t e);
DSE_CAPI int  dse_ik_add_chain(uint32_t e, const char* name, int type,
                               const char* root_bone, const char* tip_bone, float weight);
DSE_CAPI void dse_ik_set_target(uint32_t e, int idx, float x, float y, float z);
DSE_CAPI void dse_ik_set_target_entity(uint32_t e, int idx, uint32_t target);
DSE_CAPI void dse_ik_set_weight(uint32_t e, int idx, float w);
DSE_CAPI void dse_ik_set_pole_vector(uint32_t e, int idx, float x, float y, float z);
DSE_CAPI void dse_ik_set_iterations(uint32_t e, int idx, int iters);
DSE_CAPI void dse_ik_set_enabled(uint32_t e, int enabled);

// ---- FootIK（FootIK3DComponent）。脚部贴地，依赖物理 Raycast 检测地面。
// add_foot 返回脚索引（-1=无组件）；浮点参数为 NaN 时保持默认值。 ----
DSE_CAPI void dse_foot_ik_add_component(uint32_t e);
DSE_CAPI int  dse_foot_ik_add_foot(uint32_t e, const char* name, const char* foot_bone,
                                   const char* hip_bone, float foot_height,
                                   float max_ground_distance, float blend_speed, float weight);
DSE_CAPI void dse_foot_ik_set_foot_weight(uint32_t e, int idx, float w);
DSE_CAPI void dse_foot_ik_set_foot_height(uint32_t e, int idx, float h);
DSE_CAPI void dse_foot_ik_set_pelvis(uint32_t e, float pelvis_weight, float max_pelvis_offset);
DSE_CAPI void dse_foot_ik_set_enabled(uint32_t e, int enabled);

// ---- 骨骼挂点（BoneAttachmentComponent）。set_offset: 缩放 sx/sy/sz 为 NaN 时取 1。
// get_world_pos: 由目标实体动画姿态计算，out_xyz(3) 始终写入，返回 1=成功/0=失败。 ----
DSE_CAPI void dse_bone_attach_add(uint32_t e, uint32_t target, const char* bone_name);
DSE_CAPI void dse_bone_attach_set_offset(uint32_t e, float px, float py, float pz,
                                         float qx, float qy, float qz, float qw,
                                         float sx, float sy, float sz);
DSE_CAPI void dse_bone_attach_set_bone(uint32_t e, const char* bone_name);
DSE_CAPI void dse_bone_attach_set_target(uint32_t e, uint32_t target);
DSE_CAPI int  dse_bone_attach_get_world_pos(uint32_t target, const char* bone_name,
                                            float* out_xyz);
DSE_CAPI void dse_bone_attach_remove(uint32_t e);

// ---- Morph Target / Blend Shape（MorphTargetComponent）。
// add_target: deltas 扁平布局，每顶点 6 float（dpx,dpy,dpz,dnx,dny,dnz）。 ----
DSE_CAPI void  dse_morph_add_component(uint32_t e);
DSE_CAPI void  dse_morph_add_target(uint32_t e, const char* name,
                                    const float* deltas, int float_count);
DSE_CAPI void  dse_morph_set_weight(uint32_t e, const char* name, float w);
DSE_CAPI void  dse_morph_set_weight_index(uint32_t e, int idx, float w);
DSE_CAPI float dse_morph_get_weight(uint32_t e, const char* name);
DSE_CAPI int   dse_morph_get_target_count(uint32_t e);

// ============================================================
// Input
// ============================================================

DSE_CAPI int   dse_input_get_key(int key_code);
DSE_CAPI int   dse_input_get_key_down(int key_code);
DSE_CAPI int   dse_input_get_key_up(int key_code);
DSE_CAPI int   dse_input_get_mouse_button(int button);
DSE_CAPI int   dse_input_get_mouse_button_down(int button);
DSE_CAPI int   dse_input_get_mouse_button_up(int button);
DSE_CAPI float dse_input_get_mouse_x(void);
DSE_CAPI float dse_input_get_mouse_y(void);
DSE_CAPI float dse_input_get_mouse_scroll(void);
DSE_CAPI float dse_input_get_gamepad_axis(int gamepad_id, int axis);

// ============================================================
// Assets
// ============================================================

DSE_CAPI uint32_t dse_assets_load_texture(const char* path);
DSE_CAPI void     dse_assets_set_data_root(const char* path);

// ============================================================
// App / System
// ============================================================

DSE_CAPI void  dse_app_quit(void);
DSE_CAPI void  dse_app_set_window_title(const char* title);
DSE_CAPI float dse_app_get_time(void);
DSE_CAPI float dse_app_get_delta_time(void);
DSE_CAPI void  dse_app_set_target_fps(float fps);
DSE_CAPI float dse_app_get_target_fps(void);

// ============================================================
// Metrics
// ============================================================

DSE_CAPI int dse_metrics_get_draw_calls(void);

// ============================================================
// Audio（全局 AudioSystem + ECS AudioSource，手写 dse_api_services.cpp）
// ============================================================
// 全局函数依赖 dse_native_api_init 注入的 AudioSystem*，为 null 时安全返回 0/无操作。
// bool 参数/返回值使用 int(0/1)。

DSE_CAPI int  dse_audio_play_bgm(const char* path, float volume, int loop);
DSE_CAPI void dse_audio_pause_bgm(void);
DSE_CAPI void dse_audio_resume_bgm(void);
DSE_CAPI void dse_audio_stop_bgm(void);
DSE_CAPI int  dse_audio_crossfade_bgm(const char* path, float fade_sec, float volume, int loop);
DSE_CAPI void dse_audio_play_sfx(const char* path, float volume, int loop);
DSE_CAPI void dse_audio_stop_all_sfx(void);
DSE_CAPI void dse_audio_fade_out_all_sfx(float duration_sec);
DSE_CAPI int  dse_audio_preload(const char* path);
DSE_CAPI void dse_audio_set_master_volume(float volume);
DSE_CAPI void dse_audio_set_bgm_volume(float volume);
DSE_CAPI void dse_audio_set_sfx_volume(float volume);

// ECS AudioSource / AudioListener（纯组件操作）。
DSE_CAPI void dse_audio_source_add(uint32_t e, const char* path, int play_on_awake,
                                   int loop, float volume);
DSE_CAPI void dse_audio_source_set_playing(uint32_t e, int playing);
DSE_CAPI void dse_audio_source_restart(uint32_t e);
DSE_CAPI void dse_audio_source_set_loop(uint32_t e, int loop);
DSE_CAPI void dse_audio_source_set_volume(uint32_t e, float volume);
DSE_CAPI void dse_audio_source_set_pitch(uint32_t e, float pitch);
DSE_CAPI void dse_audio_source_set_3d_mode(uint32_t e, int enabled);
DSE_CAPI void dse_audio_source_set_3d_distance(uint32_t e, float min_distance,
                                               float max_distance, float rolloff);
DSE_CAPI void dse_audio_source_set_bus(uint32_t e, const char* bus_name);
DSE_CAPI int  dse_audio_source_is_playing(uint32_t e);
DSE_CAPI void dse_audio_listener_add(uint32_t e, int enabled);

// Audio — SFX 随机化 / 混音总线 / 快照 / 源状态
DSE_CAPI void dse_audio_play_sfx_random(const char* path, float volume,
                                        float pitch_min, float pitch_max);
DSE_CAPI int  dse_audio_bus_set_volume(const char* name, float volume);
DSE_CAPI int  dse_audio_bus_set_muted(const char* name, int muted);
DSE_CAPI int  dse_audio_bus_create(const char* name, const char* parent, float volume);
DSE_CAPI int  dse_audio_bus_remove(const char* name);
DSE_CAPI int  dse_audio_bus_add_effect(const char* bus_name, int type, float cutoff_hz, float q,
                                       float delay_time_ms, float feedback, float wet_mix,
                                       float room_size, float damping);
DSE_CAPI int  dse_audio_bus_remove_effect(const char* bus_name, int index);
// 名称以 '\n' 分隔写入 out（null 结尾，按 cap 截断），返回写入长度。
DSE_CAPI int  dse_audio_bus_get_names(char* out, int cap);
DSE_CAPI int  dse_audio_snapshot_save(const char* name);
DSE_CAPI int  dse_audio_snapshot_load(const char* name);
DSE_CAPI int  dse_audio_snapshot_list(char* out, int cap);
// 返回 1=组件存在；out_flags[3]=has_clip/is_playing/spatial；
// out_params[5]=min_distance/max_distance/rolloff/volume/pitch；
// out_runtime_handle/out_clip_size 可为 NULL；clip 路径写入 out_path。
DSE_CAPI int  dse_audio_source_get_state(uint32_t e, int* out_flags, float* out_params,
                                         long long* out_runtime_handle, long long* out_clip_size,
                                         char* out_path, int path_cap);

// ============================================================
// Navigation（NavMeshSystem via ServiceLocator + NavMeshAgentComponent）
// ============================================================
// DSE_ENABLE_NAVMESH 关闭时所有函数为安全空实现（返回 0）。
// find_path：路径点写入 out_xyz（最多 max_points 点×3 float），返回实际点数；
// out_xyz=null 时仅返回点数。

DSE_CAPI int  dse_nav_is_ready(void);
DSE_CAPI int  dse_nav_load(const char* path);
DSE_CAPI int  dse_nav_save(const char* path);
DSE_CAPI int  dse_nav_find_nearest(float x, float y, float z, float* out_xyz);
DSE_CAPI int  dse_nav_raycast(float sx, float sy, float sz,
                              float ex, float ey, float ez, float* out_hit_xyz);
DSE_CAPI int  dse_nav_find_path(float sx, float sy, float sz,
                                float ex, float ey, float ez,
                                float* out_xyz, int max_points);
// bake：verts=nverts×3 float，tris=ntris×3 int；config 浮点 NaN=使用默认值。
DSE_CAPI int  dse_nav_bake(const float* verts, int nverts, const int* tris, int ntris,
                           float cell_size, float cell_height,
                           float agent_height, float agent_radius,
                           float agent_max_climb, float agent_max_slope);

// NavMeshAgent（ECS）。set_agent: 浮点 NaN=保持当前/默认值。
DSE_CAPI void dse_nav_agent_set(uint32_t e, float speed, float acceleration,
                                float stopping_dist, float radius, float height);
DSE_CAPI void dse_nav_agent_set_destination(uint32_t e, float x, float y, float z);
DSE_CAPI void dse_nav_agent_get_destination(uint32_t e, float* out_xyz);
DSE_CAPI int  dse_nav_agent_has_path(uint32_t e);
DSE_CAPI int  dse_nav_agent_arrived(uint32_t e);
// get：out_params={speed,acceleration,stopping_dist,radius,height,dest_x,dest_y,dest_z}（float[8]）；
// out_flags={has_path,path_pending,arrived,current_waypoint}（int[4]）。返回 1=存在组件。
DSE_CAPI int  dse_nav_agent_get(uint32_t e, float* out_params, int* out_flags);

// ============================================================
// Localization（LocalizationManager via ServiceLocator）
// ============================================================
// 字符串输出走 out 缓冲（null 结尾，按 cap 截断），返回写入长度（不含 null）。

DSE_CAPI int  dse_l10n_load(const char* path, const char* locale);
DSE_CAPI int  dse_l10n_load_string(const char* json, const char* locale);
DSE_CAPI void dse_l10n_set_locale(const char* locale);
DSE_CAPI int  dse_l10n_get_locale(char* out, int cap);
DSE_CAPI int  dse_l10n_get(const char* key, char* out, int cap);
DSE_CAPI int  dse_l10n_has_key(const char* key);
DSE_CAPI int  dse_l10n_get_locales(char* out, int cap);  // null-separated, returns count
// Lua 绑定薄包装
DSE_CAPI int         dse_compat_l10n_load(const char* locale, const char* json);
DSE_CAPI const char* dse_compat_l10n_get_locale(void);
DSE_CAPI const char* dse_compat_l10n_get(const char* key, const char* fallback);

// ============================================================
// Scene / Prefab 序列化
// ============================================================
// load/save：路径按字面使用（不做 data root 拼接）。返回 1=成功/0=失败。
// instantiate_prefab：use_pos!=0 时用 (x,y,z) 覆盖预制体内置 Transform；
// 返回新实体 id，失败返回 UINT32_MAX 语义的无效 id（dse_entity_valid 判 0）。

DSE_CAPI int      dse_scene_load(const char* path);
DSE_CAPI int      dse_scene_save(const char* path);
DSE_CAPI int      dse_scene_save_prefab(uint32_t e, const char* path);
DSE_CAPI uint32_t dse_scene_instantiate_prefab(const char* path, float x, float y, float z,
                                               int use_pos);

// ============================================================
// SubScene / SceneManager（异步加载 / 卸载 / 查询 / 场景过渡）
// ============================================================
// 路径相对 data root（内部拼接）。transition mode：0=instant 1=additive 2=fade。
// transition state：0=idle 1=fading_out 2=loading 3=fading_in。

DSE_CAPI int   dse_scene_load_sub(const char* path, int* out_entity_count);  // 同步加载
DSE_CAPI int   dse_scene_load_sub_async(const char* path);
DSE_CAPI void  dse_scene_unload_sub(const char* path);
DSE_CAPI void  dse_scene_unload_all_subs(void);
DSE_CAPI int   dse_scene_is_sub_loaded(const char* path);
// 已加载子场景完整路径以 '\n' 分隔写入 out，返回条数。
DSE_CAPI int   dse_scene_get_loaded_subs(char* out, int cap);
DSE_CAPI int   dse_scene_get_sub_count(void);
DSE_CAPI int   dse_scene_get_pending_count(void);
DSE_CAPI void  dse_scene_transition_to(const char* path, int mode, float fade_duration);
DSE_CAPI int   dse_scene_get_transition_state(void);
DSE_CAPI float dse_scene_get_fade_progress(void);
DSE_CAPI int   dse_scene_get_active(char* out, int cap);

// ============================================================
// UUIDComponent（跨场景稳定引用）
// ============================================================
// uuid 均为 16 位十六进制字符串。get：无组件或 uuid==0 返回 0。
// set：uuid_str=null 时自动生成，最终字符串写入 out。resolve：失败返回 0xFFFFFFFF。

DSE_CAPI int      dse_uuid_get(uint32_t e, char* out, int cap);
DSE_CAPI int      dse_uuid_set(uint32_t e, const char* uuid_str, char* out, int cap);
DSE_CAPI uint32_t dse_uuid_resolve(const char* uuid_str);

// ============================================================
// ECS Core — 通用组件查询 / 层级 / 脚本 / AABB / 时间缩放
// ============================================================
// find_*：实体 id 写入 out（最多 cap 个），返回总数；组件名未知返回 -1。
// has_component：1/0；组件名未知返回 -1。
// queryable components：组件名以 '\n' 分隔写入 out，返回条数。

DSE_CAPI int  dse_ecs_find_entities_by_mesh_path(const char* mesh_path,
                                                 uint32_t* out, int cap);
DSE_CAPI int  dse_ecs_find_entities_with(const char* component, uint32_t* out, int cap);
DSE_CAPI int  dse_ecs_count_entities_with(const char* component);
DSE_CAPI int  dse_ecs_has_component(uint32_t e, const char* component);
DSE_CAPI int  dse_ecs_get_queryable_components(char* out, int cap);

// AABB：out_min_max={min_x,min_y,min_z,max_x,max_y,max_z}（float[6]）。返回 1=有 BoundingBox。
DSE_CAPI int  dse_ecs_get_world_aabb(uint32_t e, float* out_min_max);
DSE_CAPI int  dse_ecs_get_local_aabb(uint32_t e, float* out_min_max);

// TimeScaleComponent：get 无组件时返回 1.0。
DSE_CAPI void  dse_ecs_set_time_scale(uint32_t e, float scale);
DSE_CAPI float dse_ecs_get_time_scale(uint32_t e);

// Transform（position+scale 初始化，emplace_or_replace）
DSE_CAPI void dse_ecs_add_transform(uint32_t e, float x, float y, float z,
                                    float sx, float sy, float sz);

// ParentComponent。get_parent：无父级返回 0xFFFFFFFF。
DSE_CAPI void     dse_ecs_add_parent(uint32_t e, uint32_t parent);
DSE_CAPI void     dse_ecs_set_parent(uint32_t e, uint32_t parent);
DSE_CAPI uint32_t dse_ecs_get_parent(uint32_t e);
DSE_CAPI void     dse_ecs_clear_parent(uint32_t e);

// ScriptComponent。get_script_path/enabled：无组件返回 -1。
DSE_CAPI void dse_ecs_add_script(uint32_t e, const char* path);
DSE_CAPI void dse_ecs_set_script_path(uint32_t e, const char* path);
DSE_CAPI int  dse_ecs_get_script_path(uint32_t e, char* out, int cap);
DSE_CAPI void dse_ecs_set_script_enabled(uint32_t e, int enabled);
DSE_CAPI int  dse_ecs_get_script_enabled(uint32_t e);

// ============================================================
// UI（核心控件，纯 ECS 组件操作）
// ============================================================
// bool 参数/返回值使用 int(0/1)。

DSE_CAPI void  dse_ui_add_renderer(uint32_t e, uint32_t texture_handle,
                                   float r, float g, float b, float a,
                                   int order, float w, float h);
DSE_CAPI void  dse_ui_add_panel(uint32_t e, int blocks_input);
DSE_CAPI void  dse_ui_add_button(uint32_t e, float r, float g, float b, float a);
DSE_CAPI void  dse_ui_add_ttf_label(uint32_t e, const char* text, const char* font_id,
                                    float font_size, float r, float g, float b, float a);
DSE_CAPI void  dse_ui_set_label_text(uint32_t e, const char* text);
DSE_CAPI void  dse_ui_set_label_font(uint32_t e, const char* font_id, float font_size);
DSE_CAPI void  dse_ui_set_position(uint32_t e, float x, float y);
DSE_CAPI void  dse_ui_set_size(uint32_t e, float w, float h);
DSE_CAPI void  dse_ui_set_anchor(uint32_t e, float ax, float ay);
DSE_CAPI void  dse_ui_set_color(uint32_t e, float r, float g, float b, float a);
DSE_CAPI void  dse_ui_set_visible(uint32_t e, int visible);
DSE_CAPI int   dse_ui_is_hovered(uint32_t e);
DSE_CAPI int   dse_ui_is_pressed(uint32_t e);

DSE_CAPI void  dse_ui_add_joystick(uint32_t e, float max_radius, int follow_pointer,
                                   int reset_on_release);
DSE_CAPI float dse_ui_get_joystick_x(uint32_t e);
DSE_CAPI float dse_ui_get_joystick_y(uint32_t e);

DSE_CAPI void  dse_ui_add_slider(uint32_t e, float min_value, float max_value,
                                 float value, int whole_numbers);
DSE_CAPI void  dse_ui_set_slider_value(uint32_t e, float value);
DSE_CAPI float dse_ui_get_slider_value(uint32_t e);

DSE_CAPI void  dse_ui_add_toggle(uint32_t e, int is_on, int group);
DSE_CAPI void  dse_ui_set_toggle(uint32_t e, int is_on);
DSE_CAPI int   dse_ui_get_toggle(uint32_t e);

DSE_CAPI void  dse_ui_add_progress_bar(uint32_t e, float value, float max_value);
DSE_CAPI void  dse_ui_set_progress(uint32_t e, float value);
DSE_CAPI float dse_ui_get_progress(uint32_t e);

DSE_CAPI void  dse_ui_add_text_input(uint32_t e, const char* placeholder,
                                     int max_length, int is_password);
DSE_CAPI void  dse_ui_set_text_input_text(uint32_t e, const char* text);
DSE_CAPI int   dse_ui_get_text_input_text(uint32_t e, char* out, int cap);
// Lua 绑定薄包装：返回字符串
DSE_CAPI const char* dse_compat_ui_get_text_input_text(uint32_t e);
DSE_CAPI void  dse_ui_set_text_input_focus(uint32_t e, int focused);

// UI 布局文件加载（UISerializer）。返回创建的实体数（写入 out_entities，最多 cap 个）。
DSE_CAPI int   dse_ui_load_from_file(const char* path, uint32_t* out_entities, int cap);
DSE_CAPI int   dse_ui_load_from_json(const char* json, uint32_t* out_entities, int cap);

// 供手写实现访问内部 AudioSystem 指针
DSE_CAPI void* dse_get_audio_system_ptr(void);

// ============================================================
// Extended Context Setup — 扩展上下文（metrics 回调 + floating origin）
// ============================================================

DSE_CAPI void dse_native_api_init_ext(
    int   (*get_max_batch_sprites_fn)(void),
    int   (*get_sprite_count_fn)(void),
    int   (*get_gpu_driven_active_fn)(void),
    int   (*get_gpu_indirect_draw_count_fn)(void),
    int   (*get_gpu_total_instances_fn)(void),
    void* floating_origin);

DSE_CAPI void* dse_get_floating_origin_ptr(void);

// ============================================================
// Input 扩展（Screen / Gamepad buttons / Touch / Mouse extras）
// ============================================================

DSE_CAPI float dse_input_get_screen_width(void);
DSE_CAPI float dse_input_get_screen_height(void);
DSE_CAPI int   dse_input_is_gamepad_connected(int gamepad_id);
DSE_CAPI void  dse_input_set_gamepad_dead_zone(float zone);
DSE_CAPI float dse_input_get_gamepad_dead_zone(void);
DSE_CAPI float dse_input_get_mouse_scroll_dx(void);
DSE_CAPI float dse_input_get_mouse_scroll_dy(void);
DSE_CAPI int   dse_input_get_mouse_middle(void);
DSE_CAPI int   dse_input_get_mouse_middle_down(void);
DSE_CAPI int   dse_input_get_mouse_left_double_click(void);
DSE_CAPI int   dse_input_get_mouse_left_long_press(float duration);
DSE_CAPI float dse_input_get_mouse_swipe_dx(void);
DSE_CAPI float dse_input_get_mouse_swipe_dy(void);
DSE_CAPI int   dse_input_get_device_shake(void);
DSE_CAPI int   dse_input_get_touch_count(void);
DSE_CAPI int   dse_input_get_touch(int index, float* out_x, float* out_y, int* out_phase);

// ============================================================
// App / Time 扩展
// ============================================================

DSE_CAPI float dse_app_get_time_since_startup(void);
DSE_CAPI void  dse_app_set_time_scale(float scale);
DSE_CAPI float dse_app_get_time_scale(void);
DSE_CAPI float dse_app_get_fps(void);
DSE_CAPI float dse_app_get_frame_time_ms(void);

// ============================================================
// Metrics 扩展
// ============================================================

DSE_CAPI int   dse_metrics_get_max_batch_sprites(void);
DSE_CAPI int   dse_metrics_get_sprite_count(void);
DSE_CAPI int   dse_metrics_get_gpu_driven_active(void);
DSE_CAPI int   dse_metrics_get_gpu_indirect_draw_count(void);
DSE_CAPI int   dse_metrics_get_gpu_total_instances(void);
DSE_CAPI float dse_metrics_get_fps(void);
DSE_CAPI float dse_metrics_get_frame_time_ms(void);

// ============================================================
// Floating Origin
// ============================================================

DSE_CAPI void  dse_origin_get_accumulated(float* out_x, float* out_y, float* out_z);
DSE_CAPI void  dse_origin_to_absolute(float lx, float ly, float lz, float* out_x, float* out_y, float* out_z);
DSE_CAPI void  dse_origin_to_local(float ax, float ay, float az, float* out_x, float* out_y, float* out_z);
DSE_CAPI void  dse_origin_set_rebase_threshold(float threshold);
DSE_CAPI float dse_origin_get_rebase_threshold(void);

// ============================================================
// Physics3D 扩展（Collision/Trigger events, SphereCast, BoxCast, Sleep/Wake, Kinematic, Mass, Damping）
// ============================================================
// collision_events: 每个事件 11 floats (type, entity_a, entity_b, px,py,pz, nx,ny,nz, impulse, pad)
//                   写入 out_buf（cap 个 float），返回实际事件数
DSE_CAPI int dse_physics3d_get_collision_count(void);
DSE_CAPI int dse_physics3d_get_collision_events(float* out_buf, int cap);
DSE_CAPI int dse_physics3d_get_trigger_count(void);
DSE_CAPI int dse_physics3d_get_trigger_events(uint32_t* out_entities, int* out_types, int cap);
DSE_CAPI int dse_physics3d_spherecast(float ox, float oy, float oz,
                                      float dx, float dy, float dz,
                                      float radius, float max_dist,
                                      uint32_t* out_entity, float* out_point,
                                      float* out_normal, float* out_distance);
DSE_CAPI int dse_physics3d_boxcast(float ox, float oy, float oz,
                                   float dx, float dy, float dz,
                                   float hx, float hy, float hz,
                                   float max_dist,
                                   uint32_t* out_entity, float* out_point,
                                   float* out_normal, float* out_distance);
DSE_CAPI void dse_rigidbody3d_set_kinematic(uint32_t e, int kinematic);
DSE_CAPI float dse_rigidbody3d_get_mass(uint32_t e);   // codegen 逐字段
DSE_CAPI void  dse_rigidbody3d_set_mass(uint32_t e, float mass);  // codegen 逐字段
DSE_CAPI void  dse_rigidbody3d_add_force_at_position(uint32_t e, float fx, float fy, float fz,
                                                     float px, float py, float pz);
DSE_CAPI void  dse_rigidbody3d_set_linear_damping(uint32_t e, float damping);
DSE_CAPI float dse_rigidbody3d_get_linear_damping(uint32_t e);
DSE_CAPI void  dse_rigidbody3d_set_angular_damping(uint32_t e, float damping);
DSE_CAPI float dse_rigidbody3d_get_angular_damping(uint32_t e);

// ============================================================
// Physics2D（Box2D 集成）
// ============================================================

DSE_CAPI void dse_physics2d_add_rigidbody(uint32_t e, int type, float gravity_scale, int fixed_rotation);
DSE_CAPI void dse_physics2d_set_rigidbody_velocity(uint32_t e, float vx, float vy);
DSE_CAPI void dse_physics2d_add_box_collider(uint32_t e, float w, float h,
                                              float density, float friction, float restitution);
DSE_CAPI void dse_physics2d_set_box_collider_trigger(uint32_t e, int is_trigger);
DSE_CAPI void dse_physics2d_add_circle_collider(uint32_t e, float radius,
                                                 float density, float friction, float restitution);
DSE_CAPI void dse_physics2d_set_circle_collider_trigger(uint32_t e, int is_trigger);
DSE_CAPI void dse_physics2d_add_polygon_collider(uint32_t e, const float* verts, int count,
                                                  float density, float friction, float restitution);
DSE_CAPI void dse_physics2d_set_polygon_collider_trigger(uint32_t e, int is_trigger);
DSE_CAPI void dse_physics2d_add_joint(uint32_t e, int type, uint32_t entity_a, uint32_t entity_b,
                                       float ax, float ay, float bx, float by, int collide_connected);
DSE_CAPI void dse_physics2d_set_joint_revolute(uint32_t e, int enable_limit, float lower_deg, float upper_deg,
                                                int enable_motor, float motor_speed, float max_torque);
DSE_CAPI void dse_physics2d_set_joint_distance(uint32_t e, float min_len, float max_len,
                                                float stiffness, float damping);
DSE_CAPI void dse_physics2d_set_joint_prismatic(uint32_t e, float axis_x, float axis_y,
                                                 int enable_limit, float lower, float upper,
                                                 int enable_motor, float motor_speed, float max_force);
DSE_CAPI void dse_physics2d_destroy_joint(uint32_t e);
DSE_CAPI int  dse_physics2d_raycast(float sx, float sy, float ex, float ey,
                                    uint32_t* out_entity, float* out_point, float* out_normal);
DSE_CAPI int  dse_physics2d_poll_collision_event(uint32_t e, uint32_t* out_other,
                                                  int* out_is_trigger, int* out_is_enter);
DSE_CAPI void dse_physics2d_add_tilemap(uint32_t e, int width, int height,
                                        float tile_size, uint32_t tex_handle);
DSE_CAPI void dse_physics2d_set_tile(uint32_t e, int x, int y, int tile_id);

// ============================================================
// 2D Systems（Parallax / Light2D / SpriteSheet / Atlas / Camera2D / Trail / LineRenderer / AudioSpatial2D）
// ============================================================

DSE_CAPI void  dse_parallax_add(uint32_t e);
DSE_CAPI int   dse_parallax_add_layer(uint32_t e, float scroll_x, float scroll_y);
DSE_CAPI void  dse_parallax_set_layer_scroll(uint32_t e, int layer, float sx, float sy);
DSE_CAPI void  dse_parallax_set_layer_auto_scroll(uint32_t e, int layer, float sx, float sy);
DSE_CAPI void  dse_parallax_set_layer_opacity(uint32_t e, int layer, float opacity);
DSE_CAPI int   dse_parallax_get_layer_count(uint32_t e);

DSE_CAPI void  dse_light2d_add(uint32_t e, int type);
DSE_CAPI void  dse_light2d_set_color(uint32_t e, float r, float g, float b);
DSE_CAPI void  dse_light2d_set_intensity(uint32_t e, float intensity);
DSE_CAPI void  dse_light2d_set_range(uint32_t e, float range);
DSE_CAPI void  dse_light2d_set_shadow(uint32_t e, int mode);
DSE_CAPI void  dse_light2d_set_ambient(uint32_t e, float r, float g, float b, float intensity);
DSE_CAPI void  dse_normal_map_2d_add(uint32_t e, float strength);

DSE_CAPI int   dse_sprite_sheet_load(const char* path);
DSE_CAPI int   dse_sprite_sheet_frame_count(int sheet);
DSE_CAPI void  dse_sprite_sheet_get_frame_uv(int sheet, int frame, float* out_uv);

DSE_CAPI int   dse_atlas_load(const char* path);
DSE_CAPI int   dse_atlas_entry_count(int atlas);
DSE_CAPI void  dse_atlas_get_entry_uv(int atlas, const char* name, float* out_uv);

DSE_CAPI void  dse_camera_controller_2d_add(uint32_t e);
DSE_CAPI void  dse_camera_2d_shake(uint32_t e, float trauma);
DSE_CAPI void  dse_camera_2d_set_zoom(uint32_t e, float zoom);
DSE_CAPI void  dse_camera_2d_set_bounds(uint32_t e, float min_x, float min_y, float max_x, float max_y);
DSE_CAPI void  dse_camera_2d_set_look_ahead(uint32_t e, float lax, float lay);

DSE_CAPI void  dse_trail_renderer_add(uint32_t e, float lifetime, float start_width, float end_width);
DSE_CAPI void  dse_trail_set_emitting(uint32_t e, int emitting);
DSE_CAPI void  dse_trail_set_colors(uint32_t e, float r1, float g1, float b1, float a1,
                                    float r2, float g2, float b2, float a2);
DSE_CAPI void  dse_trail_clear(uint32_t e);

DSE_CAPI void  dse_line_renderer_add(uint32_t e, float width);
DSE_CAPI void  dse_line_renderer_set_points(uint32_t e, const float* points, int count);
DSE_CAPI void  dse_line_renderer_set_width(uint32_t e, float width);
DSE_CAPI void  dse_line_renderer_set_color(uint32_t e, float r, float g, float b, float a);
DSE_CAPI void  dse_line_renderer_set_closed(uint32_t e, int closed);

DSE_CAPI void  dse_audio_spatial_2d_add(uint32_t e, float min_dist, float max_dist);
DSE_CAPI void  dse_audio_spatial_2d_set_range(uint32_t e, float min_dist, float max_dist);
DSE_CAPI void  dse_audio_spatial_2d_set_attenuation(uint32_t e, int model, float rolloff);
DSE_CAPI void  dse_audio_listener_2d_add(uint32_t e, float global_volume);

// ============================================================
// Particles 3D
// ============================================================

// 浮点参数 NaN=保持当前值；mode<0=保持当前值；texture_path=NULL 保持当前值。

DSE_CAPI void  dse_particle_system_3d_add(uint32_t e, int max_particles, float emission_rate);
DSE_CAPI void  dse_particle_system_3d_set_params(uint32_t e,
                                                 float life_min, float life_max,
                                                 float size_min, float size_max,
                                                 float speed_min, float speed_max,
                                                 float r, float g, float b, float a,
                                                 float gx, float gy, float gz,
                                                 const char* texture_path);
// out_life/out_size/out_speed: float[2]，out_gravity: float[3]，out_color: float[4]。
// 组件存在返回 1，否则 0。out_tex 写入纹理路径（cap 截断，null 结尾）。
DSE_CAPI int   dse_particle_system_3d_get_state(uint32_t e, int* out_active, int* out_max_particles,
                                                float* out_emission_rate,
                                                float* out_life, float* out_size, float* out_speed,
                                                float* out_gravity, float* out_color,
                                                char* out_tex, int tex_cap,
                                                int* out_enabled, int* out_initialized,
                                                uint32_t* out_texture_handle);
DSE_CAPI void  dse_particle_emitter_add(uint32_t e, uint32_t texture_handle, int max_particles, float emit_rate);
DSE_CAPI void  dse_particle_set_density(uint32_t e, float emit_rate_scale);
DSE_CAPI void  dse_particle_burst(uint32_t e, int count);
DSE_CAPI void  dse_particle_set_random(uint32_t e,
                                       float vmin_x, float vmin_y, float vmin_z,
                                       float vmax_x, float vmax_y, float vmax_z,
                                       float life_min, float life_max,
                                       float size_min, float size_max);
DSE_CAPI void  dse_particle_set_size_curve(uint32_t e, int enabled, float start_value, float end_value);
DSE_CAPI void  dse_particle_set_alpha_curve(uint32_t e, int enabled, float start_value, float end_value);
DSE_CAPI void  dse_particle_set_speed_curve(uint32_t e, int enabled, float start_value, float end_value);
DSE_CAPI void  dse_particle_set_gravity(uint32_t e, float gx, float gy, float gz);
DSE_CAPI void  dse_particle_set_collision(uint32_t e, int enabled, int mode, float bounce,
                                          float friction, float life_loss, float ground_y);
DSE_CAPI void  dse_particle_set_color_curve(uint32_t e, int enabled,
                                            float end_r, float end_g, float end_b, float end_a);
DSE_CAPI void  dse_particle_set_rotation(uint32_t e, float rotation_min, float rotation_max,
                                         float angular_velocity_min, float angular_velocity_max);
DSE_CAPI void  dse_gameplay_tuning_add(uint32_t e);
DSE_CAPI void  dse_gameplay_tuning_set(uint32_t e, float leaf_min_distance,
                                       float leaf_move_left, float leaf_move_right,
                                       float jump_speed_scale, float jump_speed_max,
                                       float camera_follow_damping);

// ============================================================
// Rendering Light（运行时动态操作）
// ============================================================

DSE_CAPI void  dse_rendering_add_skybox(uint32_t e, const char* cubemap_path);
DSE_CAPI void  dse_rendering_add_gi_probe(uint32_t e);
DSE_CAPI void  dse_rendering_set_gi_probe(uint32_t e, float gi_intensity, float ox, float oy, float oz,
                                          float ex, float ey, float ez,
                                          int res_x, int res_y, int res_z);
DSE_CAPI void  dse_rendering_set_gi_probe_enabled(uint32_t e, int enabled);
DSE_CAPI int   dse_rendering_get_gi_probe(uint32_t e, float* out_gi_intensity,
                                          float* out_origin, float* out_extent, int* out_resolution);
DSE_CAPI void  dse_rendering_add_light_probe(uint32_t e);
DSE_CAPI void  dse_rendering_set_light_probe(uint32_t e, float influence_radius);
DSE_CAPI void  dse_rendering_set_light_probe_enabled(uint32_t e, int enabled);
DSE_CAPI void  dse_rendering_add_reflection_probe(uint32_t e);
DSE_CAPI void  dse_rendering_set_reflection_probe(uint32_t e, float influence_radius, int resolution);
DSE_CAPI void  dse_rendering_set_reflection_probe_enabled(uint32_t e, int enabled);

// 灯光/探针补充（浮点 NaN=保持当前值；int -1=保持当前值）。has/get 系列组件缺失时返回 0。
DSE_CAPI int   dse_dir_light_has(uint32_t e);
DSE_CAPI int   dse_point_light_has(uint32_t e);
DSE_CAPI int   dse_spot_light_has(uint32_t e);
DSE_CAPI int   dse_sky_light_has(uint32_t e);
DSE_CAPI int   dse_dir_light_get_shadow_params(uint32_t e, int* out_cast_shadow, float* out_strength,
                                               float* out_c0, float* out_c1, float* out_c2,
                                               float* out_lambda);
DSE_CAPI void  dse_rendering_set_gi_probe_bias(uint32_t e, float normal_bias, float hysteresis);
DSE_CAPI int   dse_rendering_get_gi_probe_ex(uint32_t e, int* out_enabled, float* out_normal_bias);
DSE_CAPI void  dse_rendering_set_light_probe_ex(uint32_t e, float influence_radius, int needs_rebake);
DSE_CAPI void  dse_rendering_set_reflection_probe_ex(uint32_t e, float influence_radius,
                                                     float box_x, float box_y, float box_z,
                                                     int resolution);

// ============================================================
// Rendering Camera 扩展
// ============================================================

DSE_CAPI void  dse_camera_add(uint32_t e, float ortho_size, int priority);
DSE_CAPI void  dse_camera_set_priority(uint32_t e, int priority);
DSE_CAPI void  dse_camera_set_enabled(uint32_t e, int enabled);
DSE_CAPI void  dse_camera_set_follow(uint32_t e, uint32_t target, float damping,
                                     float dead_zone_x, float dead_zone_y,
                                     float offset_x, float offset_y);
DSE_CAPI void  dse_free_camera_add(uint32_t e, float move_speed, float mouse_sensitivity);
DSE_CAPI void  dse_sprite_add(uint32_t e, float r, float g, float b, float a,
                              int order_in_layer, uint32_t texture_handle);
DSE_CAPI void  dse_sprite_set_uv_scroll(uint32_t e, float sx, float sy);
DSE_CAPI void  dse_sprite_set_uv_offset(uint32_t e, float ox, float oy);

// ============================================================
// Rendering Mesh 扩展
// ============================================================

DSE_CAPI void  dse_mesh_set_material(uint32_t e, const char* material_path);
DSE_CAPI void  dse_mesh_set_depth_state(uint32_t e, int depth_test, int depth_write);
DSE_CAPI void  dse_mesh_set_material_scalar(uint32_t e, const char* param_name, float value);
DSE_CAPI void  dse_mesh_set_texture_handle(uint32_t e, const char* slot, uint32_t texture_handle);
DSE_CAPI void  dse_mesh_set_emissive(uint32_t e, float r, float g, float b);

// ============================================================
// Rendering FX（Steering / LOD / Hair / Pick）
// ============================================================

DSE_CAPI void  dse_steering_add(uint32_t e, float max_velocity, float max_force, float mass);
// behavior: 0=seek 1=flee 2=arrive；返回 1=成功。
DSE_CAPI int   dse_steering_set_target(uint32_t e, int behavior, float x, float y, float z);
// out_flags[4]=enabled/seek/flee/arrive；out_velocity[3]；out_params[4]=max_velocity/max_force/mass/arrive_deceleration_radius；
// out_targets[9]=seek/flee/arrive 各 3 分量。返回 1=组件存在。
DSE_CAPI int   dse_steering_get_state(uint32_t e, int* out_flags, float* out_velocity,
                                      float* out_params, float* out_targets);

DSE_CAPI void  dse_lod_add_level(uint32_t e, const char* mesh_path, float screen_size_threshold);
DSE_CAPI void  dse_lod_set_scale(uint32_t e, float scale);
DSE_CAPI void  dse_lod_set_min_screen_size(uint32_t e, float min_size);
DSE_CAPI void  dse_lod_set_enabled(uint32_t e, int enabled);

// num_follow_per_guide<0=保持默认。浮点 NaN=保持当前值。
// dse_hair_set_enabled 见 dse_api.gen.h（codegen 逐字段 setter）。
DSE_CAPI void  dse_hair_add(uint32_t e, const char* asset_path, int num_follow_per_guide);
DSE_CAPI void  dse_hair_set_physics(uint32_t e, float damping, float stiffness_local,
                                    float stiffness_global, float gravity);
DSE_CAPI void  dse_hair_set_render(uint32_t e,
                                   float root_r, float root_g, float root_b, float root_a,
                                   float tip_r, float tip_g, float tip_b, float tip_a,
                                   float fiber_radius, float opacity);
DSE_CAPI void  dse_hair_set_wind_full(uint32_t e, float wx, float wy, float wz, float turbulence);
DSE_CAPI void  dse_hair_set_lod(uint32_t e, float lod0_distance, float lod1_distance,
                                float lod2_distance, float cull_distance);

// ============================================================
// Rendering Post 扩展（Decal / PostProcess state query）
// ============================================================

DSE_CAPI void  dse_decal_add(uint32_t e, uint32_t albedo_texture);
DSE_CAPI void  dse_decal_set(uint32_t e, float r, float g, float b, float a, float angle_fade);
DSE_CAPI int   dse_post_process_get_state(uint32_t e, int* out_enabled, int* out_bloom, int* out_ssao,
                                           int* out_ssr, int* out_fxaa, int* out_dof);

// ============================================================
// Animation 扩展
// ============================================================

DSE_CAPI void  dse_anim3d_set_blend_tree_1d(uint32_t e, const char* const* clips, const float* thresholds,
                                             const float* speeds, int count);
DSE_CAPI void  dse_anim3d_set_blend_param(uint32_t e, float value);
DSE_CAPI float dse_anim3d_get_blend_param(uint32_t e);
DSE_CAPI void  dse_anim3d_set_layer_weight(uint32_t e, int layer, float weight);
DSE_CAPI float dse_anim3d_get_layer_weight(uint32_t e, int layer);
DSE_CAPI void  dse_anim3d_set_layer_mask(uint32_t e, int layer, const char* const* bones, int count);

// ============================================================
// Gameplay3D 扩展
// ============================================================

DSE_CAPI int   dse_character_check_ground(uint32_t e, float* out_normal);

// ============================================================
// Open World（WorldPartition / HLOD / VirtualTexture / Clipmap / SDF / AI LoD / GpuParticle / WorldStatePersistence / Procedural）
// ============================================================

DSE_CAPI int   dse_wp_get_loaded_count(void);
DSE_CAPI int   dse_wp_force_load(int cx, int cz);
DSE_CAPI int   dse_wp_force_unload(int cx, int cz);
DSE_CAPI void  dse_wp_world_to_cell(float x, float y, float z, float cell_size, int* out_cx, int* out_cz);
DSE_CAPI void  dse_wp_cell_to_world(int cx, int cz, float cell_size, float* out_x, float* out_y, float* out_z);

DSE_CAPI int   dse_hlod_get_cluster_count(void);
DSE_CAPI int   dse_hlod_get_active_proxy_count(void);

DSE_CAPI float dse_vt_get_cache_hit_rate(void);
DSE_CAPI int   dse_vt_get_page_table_size(void);
DSE_CAPI int   dse_vt_get_physical_atlas_size(void);
DSE_CAPI int   dse_vt_get_occupied_pages(void);

DSE_CAPI int   dse_clipmap_get_level_count(void);
DSE_CAPI int   dse_clipmap_sample_height(float x, float z, float* out_y);
DSE_CAPI void  dse_clipmap_get_config(float* out_cell_size, int* out_levels);

DSE_CAPI float dse_sdf_query_distance(float x, float y, float z);
DSE_CAPI int   dse_sdf_get_cascade_count(void);
DSE_CAPI int   dse_sdf_rebuild(void);

DSE_CAPI void  dse_ai_lod_register(uint32_t e, float importance);
DSE_CAPI void  dse_ai_lod_unregister(uint32_t e);
DSE_CAPI int   dse_ai_lod_should_tick(uint32_t e);
DSE_CAPI int   dse_ai_lod_get_level(uint32_t e);
DSE_CAPI void  dse_ai_lod_set_force_active(uint32_t e, int force);
DSE_CAPI int   dse_ai_lod_get_registered_count(void);
DSE_CAPI void  dse_ai_lod_get_config(float* out_near_dist, float* out_far_dist, int* out_max_level);

DSE_CAPI void  dse_gpu_particle_set_enabled(uint32_t e, int enabled);
DSE_CAPI void  dse_gpu_particle_set_emission_rate(uint32_t e, float rate);
DSE_CAPI void  dse_gpu_particle_set_gravity(uint32_t e, float gx, float gy, float gz);
DSE_CAPI void  dse_gpu_particle_set_wind(uint32_t e, float wx, float wy, float wz);
DSE_CAPI void  dse_gpu_particle_set_color(uint32_t e, float r1, float g1, float b1, float a1,
                                         float r2, float g2, float b2, float a2);

DSE_CAPI int   dse_wsp_save_all(void);
DSE_CAPI int   dse_wsp_save_cell(int cx, int cz);
DSE_CAPI int   dse_wsp_load_cell(int cx, int cz);
DSE_CAPI int   dse_wsp_reset_cell(int cx, int cz);
DSE_CAPI int   dse_wsp_get_dirty_count(void);
DSE_CAPI int   dse_wsp_get_total_modifications(void);
DSE_CAPI void  dse_wsp_record_destruction(int cx, int cz, uint64_t entity_id);

DSE_CAPI float dse_procedural_perlin2d(float x, float y, uint32_t seed);
DSE_CAPI float dse_procedural_simplex2d(float x, float y, uint32_t seed);
DSE_CAPI float dse_procedural_worley2d(float x, float y, uint32_t seed);
DSE_CAPI float dse_procedural_fbm2d(float x, float y, int octaves, float frequency,
                                    float lacunarity, float persistence, uint32_t seed);
DSE_CAPI void  dse_procedural_random_seed(uint64_t seed);
DSE_CAPI float dse_procedural_random_float(float min_val, float max_val);

// ============================================================
// Streaming
// ============================================================

DSE_CAPI uint32_t dse_streaming_create_zone(const char* name, float x, float y, float z,
                                       float load_r, float unload_r);
DSE_CAPI void  dse_streaming_destroy_zone(uint32_t zone);
DSE_CAPI void  dse_streaming_add_asset(uint32_t zone, const char* path, const char* type_str);
DSE_CAPI void  dse_streaming_add_assets(uint32_t zone, const char* const* paths, int count,
                                        const char* type_str);
DSE_CAPI void  dse_streaming_set_zone_center(uint32_t zone, float x, float y, float z);
DSE_CAPI void  dse_streaming_force_load(uint32_t zone);
DSE_CAPI void  dse_streaming_force_unload(uint32_t zone);
DSE_CAPI int   dse_streaming_get_zone_state(uint32_t zone);
DSE_CAPI float dse_streaming_get_zone_progress(uint32_t zone);
DSE_CAPI void  dse_streaming_set_budget(int max_loads_per_frame, int max_concurrent);
DSE_CAPI int   dse_streaming_get_active_loads(void);
DSE_CAPI int   dse_streaming_get_zone_count(void);

// ============================================================
// HTTP — 异步完成队列模型
// ============================================================

// 发送请求，返回 request_id（0 = 失败）。headers_json 为 JSON 字符串 {"K":"V",...} 或 NULL。
DSE_CAPI uint32_t dse_http_send(const char* method, const char* url, const char* body,
                                const char* headers_json, int timeout_sec, int verify_peer,
                                const char* ca_file);
// 轮询已完成的请求 ID，返回数量。每帧调用。
DSE_CAPI int   dse_http_poll(uint32_t* out_ids, int max_ids);
// 获取指定请求的响应数据。成功返回 1。
DSE_CAPI int   dse_http_get_response(uint32_t request_id, int* out_status,
                                     char* out_body, int body_cap,
                                     char* out_error, int error_cap);
// 内部 pump（引擎 Tick 自动调用）
DSE_CAPI void  dse_http_update(void);
DSE_CAPI int   dse_http_available(void);

// ============================================================
// Video — 句柄式视频播放器
// ============================================================

DSE_CAPI uint32_t dse_video_create_player(void);
DSE_CAPI void  dse_video_destroy_player(uint32_t player);
// play: backend: 0=Auto, 1=FFmpeg, 2=PlMpeg
DSE_CAPI void  dse_video_play(uint32_t player, const char* path, int loop, float playback_rate,
                             int decode_audio, int prefetch_frames, int backend);
DSE_CAPI void  dse_video_pause(uint32_t player);
DSE_CAPI void  dse_video_resume(uint32_t player);
DSE_CAPI void  dse_video_stop(uint32_t player);
DSE_CAPI void  dse_video_seek(uint32_t player, float time);
DSE_CAPI void  dse_video_set_loop(uint32_t player, int loop);
DSE_CAPI void  dse_video_set_playback_rate(uint32_t player, float rate);
DSE_CAPI uint32_t dse_video_update(uint32_t player, float delta_time);  // 返回 texture_id
DSE_CAPI int   dse_video_get_state(uint32_t player);   // 0=Idle,1=Playing,2=Paused,3=Stopped,4=Ended,5=Error
DSE_CAPI float dse_video_get_time(uint32_t player);
DSE_CAPI float dse_video_get_duration(uint32_t player);
DSE_CAPI void  dse_video_get_info(uint32_t player, int* out_w, int* out_h, float* out_fps,
                                 float* out_duration, int* out_total_frames, int* out_has_audio,
                                 int* out_sample_rate, int* out_channels,
                                 char* out_codec, int codec_cap);
DSE_CAPI uint32_t dse_video_get_texture(uint32_t player);

// ============================================================
// DSSL（DSEngine Shader Language）
// ============================================================

DSE_CAPI uint32_t dse_dssl_load_material(const char* path);
DSE_CAPI uint32_t dse_dssl_create_instance(const char* path);
DSE_CAPI void  dse_dssl_set_float(uint32_t instance, const char* name, float value);
DSE_CAPI void  dse_dssl_set_color(uint32_t instance, const char* name, float r, float g, float b, float a);
DSE_CAPI void  dse_dssl_set_vec3(uint32_t instance, const char* name, float x, float y, float z);
DSE_CAPI void  dse_dssl_set_texture(uint32_t instance, const char* name, const char* path);
DSE_CAPI void  dse_dssl_set_texture_handle(uint32_t instance, const char* name, uint32_t handle);
DSE_CAPI void  dse_dssl_apply_material(uint32_t e, uint32_t instance);
DSE_CAPI float dse_dssl_get_float(uint32_t instance, const char* name);
DSE_CAPI void  dse_dssl_get_color(uint32_t instance, const char* name, float* out_rgba);

// ============================================================
// Meshlet — Cluster 渲染系统
// ============================================================

// build: positions = flat xyz triplets (count = pos_count * 3), indices = uint32 array
DSE_CAPI uint32_t dse_meshlet_build(const float* positions, int pos_count,
                                    const uint32_t* indices, int idx_count,
                                    uint32_t max_vertices, uint32_t max_triangles);
DSE_CAPI int   dse_meshlet_serialize(uint32_t handle, const char* path);
DSE_CAPI uint32_t dse_meshlet_deserialize(const char* path);
DSE_CAPI void  dse_meshlet_destroy(uint32_t handle);
DSE_CAPI void  dse_meshlet_get_info(uint32_t handle, int* out_meshlets, int* out_vertices,
                                    int* out_indices, int* out_meshlet_vertices);
DSE_CAPI uint32_t dse_meshlet_cull_create(void);
DSE_CAPI void  dse_meshlet_cull_destroy(uint32_t cull_handle);
DSE_CAPI uint32_t dse_meshlet_cull_register(uint32_t cull_handle, uint32_t meshlet_handle);
DSE_CAPI void  dse_meshlet_cull_unregister(uint32_t cull_handle, uint32_t reg_handle);
DSE_CAPI void  dse_meshlet_cull_begin_frame(uint32_t cull_handle);
DSE_CAPI void  dse_meshlet_cull_add_instance(uint32_t cull_handle, uint32_t reg_handle,
                                            const float* matrix16);
// prepare: vp_matrix = 16 floats, returns instance count
DSE_CAPI uint32_t dse_meshlet_cull_prepare(uint32_t cull_handle, const float* vp_matrix16,
                                           float cam_x, float cam_y, float cam_z);
// execute_cpu: vp_matrix = 16 floats, flags: bit0=frustum, bit1=occlusion, bit2=cone
DSE_CAPI uint32_t dse_meshlet_cull_execute_cpu(uint32_t cull_handle, const float* vp_matrix16,
                                                float cam_x, float cam_y, float cam_z,
                                                uint32_t flags);
DSE_CAPI void  dse_meshlet_cull_stats(uint32_t cull_handle, int* out_total, int* out_visible,
                                      int* out_meshes, int* out_instances);

// ============================================================
// World Systems（Spline / Ocean / Editor / VSM / EQS / Distribution）
// ============================================================

// Spline
DSE_CAPI int   dse_spline_init(void);
DSE_CAPI void  dse_spline_shutdown(void);
DSE_CAPI uint32_t dse_spline_create(const char* name);
DSE_CAPI void  dse_spline_destroy(uint32_t spline);
DSE_CAPI void  dse_spline_add_point(uint32_t spline, float x, float y, float z, float width);
DSE_CAPI void  dse_spline_set_point(uint32_t spline, int index, float x, float y, float z, float width);
DSE_CAPI void  dse_spline_remove_point(uint32_t spline, int index);
DSE_CAPI int   dse_spline_get_point_count(uint32_t spline);
DSE_CAPI float dse_spline_get_length(uint32_t spline);
DSE_CAPI void  dse_spline_evaluate(uint32_t spline, float t, float* out_xyz);
DSE_CAPI void  dse_spline_evaluate_distance(uint32_t spline, float dist, float* out_xyz);
DSE_CAPI float dse_spline_find_nearest(uint32_t spline, float x, float y, float z);
DSE_CAPI int   dse_spline_gen_road(uint32_t spline, float segment_length, int width_segments);
DSE_CAPI int   dse_spline_gen_river(uint32_t spline, float segment_length, float depth);

// Ocean
DSE_CAPI int   dse_ocean_init(int fft_resolution, float tile_size, float wind_speed, float choppiness);
DSE_CAPI void  dse_ocean_shutdown(void);
DSE_CAPI void  dse_ocean_update(float time, float cam_x, float cam_y, float cam_z);
DSE_CAPI float dse_ocean_get_height(float x, float z);
DSE_CAPI void  dse_ocean_get_normal(float x, float z, float* out_xyz);
DSE_CAPI float dse_ocean_get_foam(float x, float z);
DSE_CAPI void  dse_ocean_set_wind(float speed, float dx, float dz);
DSE_CAPI void  dse_ocean_set_choppiness(float choppiness);
DSE_CAPI void  dse_ocean_get_stats(int* out_total_tiles, int* out_visible_tiles,
                                   int* out_fft_res, float* out_max_height);
DSE_CAPI int   dse_ocean_get_lod_count(void);

// Editor helpers
DSE_CAPI int   dse_editor_init(void);
DSE_CAPI void  dse_editor_shutdown(void);
DSE_CAPI int   dse_editor_terrain_brush(int op, float x, float y, float z, float radius,
                                        float strength, float falloff);
DSE_CAPI void  dse_editor_brush_preview(float x, float y, float z, float radius,
                                        float* out_min_x, float* out_min_y,
                                        float* out_max_x, float* out_max_y);
DSE_CAPI int   dse_editor_place_foliage(float x, float y, float z, float radius,
                                        float density, const char* mesh_path);
DSE_CAPI int   dse_editor_erase_foliage(float x, float y, float z, float radius);
DSE_CAPI int   dse_editor_get_foliage_count(void);
DSE_CAPI int   dse_editor_begin_road(float width);
DSE_CAPI void  dse_editor_add_road_point(uint32_t session, float x, float y, float z);
DSE_CAPI void  dse_editor_end_road(uint32_t session);
DSE_CAPI void  dse_editor_update_partition_vis(float cam_x, float cam_y, float cam_z, float cell_size);
DSE_CAPI int   dse_editor_get_cell_count(void);
DSE_CAPI int   dse_editor_undo(void);
DSE_CAPI int   dse_editor_redo(void);

// VSM (Virtual Shadow Maps)
DSE_CAPI int   dse_vsm_init(uint32_t virtual_resolution, uint32_t page_size,
                            uint32_t pool_pages, uint32_t clipmap_levels);
DSE_CAPI void  dse_vsm_shutdown(void);
DSE_CAPI uint32_t dse_vsm_register_light(uint32_t light_id, int is_directional,
                                         float dx, float dy, float dz);
DSE_CAPI void  dse_vsm_unregister_light(uint32_t light_id);
DSE_CAPI void  dse_vsm_begin_frame(uint32_t frame, float cam_x, float cam_y, float cam_z);
DSE_CAPI void  dse_vsm_end_frame(void);
DSE_CAPI void  dse_vsm_invalidate(uint32_t light_id,
                                  float min_x, float min_y, float min_z,
                                  float max_x, float max_y, float max_z);
DSE_CAPI void  dse_vsm_mark_page_rendered(uint32_t vx, uint32_t vy, uint32_t mip, uint32_t light_id);
DSE_CAPI int   dse_vsm_get_pages_to_render(void);
DSE_CAPI int   dse_vsm_lookup_page(uint32_t vx, uint32_t vy, uint32_t mip, uint32_t light_id,
                                   uint32_t* out_px, uint32_t* out_py);
DSE_CAPI void  dse_vsm_get_stats(int* out_total, int* out_mapped, int* out_dirty,
                                 int* out_rendered, int* out_cache_hit, int* out_pool_usage);
DSE_CAPI int   dse_vsm_get_clipmap_levels(void);

// EQS (Environment Query System)
DSE_CAPI int   dse_eqs_init(void);
DSE_CAPI void  dse_eqs_shutdown(void);
DSE_CAPI uint32_t dse_eqs_create_template(const char* name);
DSE_CAPI void  dse_eqs_destroy_template(uint32_t tmpl);
DSE_CAPI void  dse_eqs_set_generator(uint32_t tmpl, int type, float radius, float spacing, int max_points);
DSE_CAPI void  dse_eqs_add_scorer(uint32_t tmpl, int type, float weight, int invert, float max_value);
DSE_CAPI void  dse_eqs_clear_scorers(uint32_t tmpl);
DSE_CAPI void  dse_eqs_set_combine_mode(uint32_t tmpl, int mode);
DSE_CAPI void  dse_eqs_set_max_results(uint32_t tmpl, uint32_t max_results);
// execute: out = {best_x,best_y,best_z,best_score,total_generated,valid_count,query_time_ms}
DSE_CAPI void  dse_eqs_execute(uint32_t tmpl, float x, float y, float z, float* out_result);
DSE_CAPI int   dse_eqs_get_template_count(void);
DSE_CAPI void  dse_eqs_execute_at(uint32_t tmpl, float px, float py, float pz,
                                  float cx, float cy, float cz, float* out_result);

// Distribution
DSE_CAPI int   dse_dist_init(float cell_size, int max_downloads, const char* cdn_url);
DSE_CAPI void  dse_dist_shutdown(void);
DSE_CAPI int   dse_dist_load_manifest(const char* path);
DSE_CAPI int   dse_dist_save_manifest(const char* path);
DSE_CAPI int   dse_dist_package_cell(int cx, int cz, int lod, const char* const* assets, int count);
DSE_CAPI void  dse_dist_request_download(const char* package);
DSE_CAPI void  dse_dist_cancel_download(const char* package);
DSE_CAPI void  dse_dist_update_priorities(float x, float y, float z);
DSE_CAPI void  dse_dist_tick(float dt);
DSE_CAPI int   dse_dist_is_installed(const char* package);
DSE_CAPI void  dse_dist_get_stats(int* out_total, int* out_installed, int* out_downloading,
                                  int* out_pending, double* out_downloaded_bytes, double* out_speed_bps);
DSE_CAPI int   dse_dist_get_missing(float x, float y, float z, float radius,
                                    char* out_buf, int buf_cap);
DSE_CAPI int   dse_dist_verify(const char* package);
DSE_CAPI uint64_t dse_dist_get_disk_usage(void);

// ============================================================
// Font — 字体服务
// ============================================================

DSE_CAPI int   dse_font_load(const char* font_id, const char* ttf_path);
DSE_CAPI int   dse_font_load_cjk(const char* font_id, const char* ttf_path);
DSE_CAPI void  dse_font_unload(const char* font_id);
DSE_CAPI int   dse_font_set_default(const char* font_id);
DSE_CAPI float dse_font_measure(const char* text, const char* font_id, float font_size);
DSE_CAPI float dse_font_line_height(const char* font_id, float font_size);
DSE_CAPI uint32_t dse_font_get_texture(const char* font_id);

// ============================================================
// Spine — 骨骼动画渲染组件
// ============================================================

DSE_CAPI void  dse_spine_add_renderer(uint32_t e, const char* skel_path, const char* atlas_path);
DSE_CAPI void  dse_spine_set_animation(uint32_t e, const char* anim_name, int loop);

// ============================================================
// PostProcess — 组件创建 / LUT 加载
// ============================================================

DSE_CAPI void  dse_post_process_add(uint32_t e);
DSE_CAPI void  dse_post_process_set_color_lut(uint32_t e, const char* path, float intensity);

// ============================================================
// Decal — 组件创建 / 扩展字段设置
// ============================================================

DSE_CAPI void  dse_decal_add_simple(uint32_t e);
DSE_CAPI void  dse_decal_set_full(uint32_t e, int enabled, int has_texture, uint32_t texture,
                                   float r, float g, float b, float a, float angle_fade);

// ============================================================
// Terrain / Water / Grass / Foliage / Environment — 组件创建与复合操作
// ============================================================

DSE_CAPI void  dse_terrain_add(uint32_t e, const char* heightmap_path,
                                float width, float depth, float max_height);
DSE_CAPI void  dse_terrain_set_params(uint32_t e, int res_x, int res_z,
                                       int max_lod, float lod_factor, int use_dynamic_lod);
DSE_CAPI void  dse_terrain_set_height(uint32_t e, int x, int z, float height);
DSE_CAPI int   dse_terrain_load_heightmap(uint32_t e, const char* path,
                                           int* out_w, int* out_h, int* out_ch,
                                           int* out_rx, int* out_rz);
DSE_CAPI int   dse_terrain_set_texture(uint32_t e, const char* path,
                                        uint32_t* out_handle, int* out_w, int* out_h);
DSE_CAPI void  dse_terrain_get_lod(uint32_t e, int* out_lod, int* out_rx, int* out_rz,
                                    int* out_max_lod, float* out_lod_factor);
DSE_CAPI float dse_terrain_sample_height(uint32_t e, float wx, float wz);
DSE_CAPI int   dse_terrain_set_splat_texture(uint32_t e, int layer, const char* path);

DSE_CAPI void  dse_water_add(uint32_t e);
DSE_CAPI void  dse_water_set(uint32_t e, int enabled, float water_level,
                              float dr, float dg, float db,
                              float sr, float sg, float sb,
                              float max_depth, float transparency,
                              float wave_amp, float wave_freq, float wave_speed,
                              float wdir_x, float wdir_y,
                              float refraction, float reflection, float spec_power,
                              float caustic_int, float caustic_scale,
                              float foam_int, float foam_threshold,
                              float ufog_density, float ufog_r, float ufog_g, float ufog_b);
DSE_CAPI int   dse_water_get(uint32_t e, int* out_enabled, float* out_water_level,
                              float* out_deep_rgb, float* out_shallow_rgb,
                              float* out_max_depth, float* out_transparency,
                              float* out_wave, float* out_wdir,
                              float* out_refraction, float* out_reflection, float* out_spec_power);

DSE_CAPI void  dse_grass_add(uint32_t e, float density, float spawn_radius,
                              float blade_height, float blade_width);
DSE_CAPI void  dse_grass_set_params(uint32_t e, float density, float spawn_radius,
                                     float blade_height, float blade_width,
                                     float blade_height_var, float chunk_size, int seed);
DSE_CAPI void  dse_grass_set_color(uint32_t e, float br, float bg, float bb,
                                    float tr, float tg, float tb);
DSE_CAPI void  dse_grass_set_wind(uint32_t e, float dx, float dy,
                                   float speed, float strength, float turbulence);
DSE_CAPI void  dse_grass_set_lod(uint32_t e, float near_dist, float far_dist,
                                  int cast_shadow, float shadow_dist);
DSE_CAPI void  dse_grass_set_enabled(uint32_t e, int enabled);
DSE_CAPI int   dse_grass_get_stats(uint32_t e);

DSE_CAPI void  dse_foliage_add(uint32_t e);
DSE_CAPI void  dse_foliage_set_wind_strength(uint32_t e, float v);   // codegen 逐字段
DSE_CAPI float dse_foliage_get_wind_strength(uint32_t e);   // codegen 逐字段
DSE_CAPI void  dse_foliage_set_stiffness(uint32_t e, float v);   // codegen 逐字段
DSE_CAPI float dse_foliage_get_stiffness(uint32_t e);   // codegen 逐字段
DSE_CAPI void  dse_foliage_set_enabled(uint32_t e, int v);   // codegen 逐字段
DSE_CAPI int   dse_foliage_get_enabled(uint32_t e);   // codegen 逐字段

DSE_CAPI void  dse_tree_add(uint32_t e, const char* mesh_path);
DSE_CAPI void  dse_terrain_tile_manager_add(uint32_t e);
DSE_CAPI void  dse_dynamic_obstacle_add(uint32_t e, int shape);
DSE_CAPI void  dse_navmesh_rebake_add(uint32_t e);

// ============================================================
// UI — 扩展控件（纯 ECS 组件操作）
// ============================================================

DSE_CAPI void  dse_ui_add_label(uint32_t e, const char* text, uint32_t font_tex_handle,
                                 float r, float g, float b, float a,
                                 float glyph_w, float glyph_h, float spacing,
                                 int atlas_cols, int atlas_rows, int ascii_start,
                                 float offset_x, float offset_y);
DSE_CAPI void  dse_ui_set_label_number(uint32_t e, long long number);
DSE_CAPI void  dse_ui_set_label_layout(uint32_t e, float max_width, int align,
                                        int overflow, int max_lines, float line_spacing);
DSE_CAPI void  dse_ui_add_mask(uint32_t e, float w, float h, float ox, float oy, int block_outside);
DSE_CAPI void  dse_ui_add_rich_text(uint32_t e, const char* text,
                                     float r, float g, float b, float a, int shadow, int outline);
DSE_CAPI void  dse_ui_set_rich_text(uint32_t e, const char* text);
DSE_CAPI void  dse_ui_set_button_scale(uint32_t e, float hover, float pressed, float lerp_speed);
DSE_CAPI void  dse_ui_set_uv(uint32_t e, float u, float v, float w, float h);
DSE_CAPI void  dse_ui_set_nine_slice(uint32_t e, int enabled, float l, float b, float r, float t,
                                      float sw, float sh);
DSE_CAPI void  dse_ui_add_anchor(uint32_t e, int anchor_type, float ox, float oy);
DSE_CAPI void  dse_ui_set_anchor_type(uint32_t e, int anchor_type);
DSE_CAPI void  dse_ui_set_anchor_offset(uint32_t e, float ox, float oy);
DSE_CAPI void  dse_ui_add_grid_layout(uint32_t e, int columns, float cw, float ch,
                                       float sx, float sy);
DSE_CAPI void  dse_ui_set_grid_layout(uint32_t e, int columns, int rows,
                                       float cw, float ch, float sx, float sy, int alignment);
DSE_CAPI void  dse_ui_add_canvas_scaler(uint32_t e, float ref_w, float ref_h, int match);
DSE_CAPI void  dse_ui_set_canvas_scaler(uint32_t e, float ref_w, float ref_h,
                                         float scale_factor, int match_wh, float match, int pixel_snap);
DSE_CAPI void  dse_ui_add_box_layout(uint32_t e, int vertical, float spacing,
                                      float pad_x, float pad_y, int align_main, int align_cross, int reverse);
DSE_CAPI void  dse_ui_set_box_layout(uint32_t e, int vertical, float spacing,
                                      float pad_x, float pad_y, int align_main, int align_cross, int reverse);
DSE_CAPI void  dse_ui_add_content_size_fitter(uint32_t e, int fit_w, int fit_h,
                                                float min_w, float min_h, float max_w, float max_h);
DSE_CAPI void  dse_ui_add_animation(uint32_t e, float duration, int easing,
                                     int loop, int ping_pong, float delay);
DSE_CAPI void  dse_ui_animate_position(uint32_t e, float tx, float ty);
DSE_CAPI void  dse_ui_animate_scale(uint32_t e, float sx, float sy);
DSE_CAPI void  dse_ui_animate_alpha(uint32_t e, float alpha);
DSE_CAPI void  dse_ui_animate_color(uint32_t e, float r, float g, float b, float a);
DSE_CAPI void  dse_ui_stop_animation(uint32_t e);
DSE_CAPI void  dse_ui_set_text_input_placeholder(uint32_t e, const char* placeholder);
DSE_CAPI void  dse_ui_add_scroll_view(uint32_t e, float cw, float ch, int horizontal, int vertical);
DSE_CAPI void  dse_ui_set_scroll_offset(uint32_t e, float x, float y);
DSE_CAPI void  dse_ui_get_scroll_offset(uint32_t e, float* out_x, float* out_y);
DSE_CAPI void  dse_ui_set_scroll_content_size(uint32_t e, float w, float h);
DSE_CAPI void  dse_ui_set_slider_colors(uint32_t e, float tr, float tg, float tb, float ta,
                                         float fr, float fg, float fb, float fa,
                                         float hr, float hg, float hb, float ha);
DSE_CAPI void  dse_ui_set_slider_handle_size(uint32_t e, float size);
DSE_CAPI void  dse_ui_set_slider_vertical(uint32_t e, int vertical);
DSE_CAPI void  dse_ui_set_slider_range(uint32_t e, float min_val, float max_val);
DSE_CAPI void  dse_ui_add_dropdown(uint32_t e, float item_height, int max_visible);
DSE_CAPI void  dse_ui_dropdown_add_option(uint32_t e, const char* text, const char* value);
DSE_CAPI void  dse_ui_dropdown_clear_options(uint32_t e);
DSE_CAPI void  dse_ui_set_dropdown_index(uint32_t e, int index);
DSE_CAPI int   dse_ui_get_dropdown_index(uint32_t e);
DSE_CAPI int   dse_ui_get_dropdown_value(uint32_t e, char* out, int cap);
DSE_CAPI void  dse_ui_set_dropdown_open(uint32_t e, int open);
DSE_CAPI void  dse_ui_add_filled_image(uint32_t e, float fill_amount, int method, int origin, int clockwise);
DSE_CAPI void  dse_ui_set_fill_amount(uint32_t e, float amount);
DSE_CAPI float dse_ui_get_fill_amount(uint32_t e);
DSE_CAPI void  dse_ui_set_fill_method(uint32_t e, int method, int origin, int clockwise);
DSE_CAPI void  dse_ui_add_focus_navigable(uint32_t e, int tab_index);
DSE_CAPI void  dse_ui_set_focus_nav(uint32_t e, uint32_t up, uint32_t down, uint32_t left, uint32_t right);
DSE_CAPI int   dse_ui_is_focused(uint32_t e);
DSE_CAPI void  dse_ui_set_focus_tint(uint32_t e, float r, float g, float b, float a);
DSE_CAPI void  dse_ui_add_event_propagation(uint32_t e, int bubbles_click, int bubbles_hover);
DSE_CAPI void  dse_ui_stop_propagation(uint32_t e);
DSE_CAPI void  dse_ui_add_visual_effect(uint32_t e);
DSE_CAPI void  dse_ui_set_corner_radius(uint32_t e, float radius);
DSE_CAPI void  dse_ui_set_gradient(uint32_t e, float sr, float sg, float sb, float sa,
                                    float er, float eg, float eb, float ea, int direction);
DSE_CAPI void  dse_ui_set_blur(uint32_t e, float radius, float intensity);
DSE_CAPI void  dse_ui_add_virtual_scroll(uint32_t e, int total_items, float item_height);
DSE_CAPI void  dse_ui_set_virtual_scroll_count(uint32_t e, int count);
DSE_CAPI void  dse_ui_get_virtual_scroll_range(uint32_t e, int* out_start, int* out_end);
DSE_CAPI void  dse_ui_destroy_virtual_scroll(uint32_t e);

// ============================================================
// Open World P2-P5（Mesh Streaming / Physics LOD / Terrain Deform / Audio LOD）
// ============================================================
// 系统实例由本层管理（单例）。浮点参数 NaN=使用默认值，整型参数 <0=使用默认值。

// Mesh Streaming
DSE_CAPI void dse_mesh_streaming_init(float hysteresis, int load_budget_per_frame);
DSE_CAPI uint32_t dse_mesh_streaming_register_mesh(const char* name,
                                                   float x, float y, float z, float radius);
DSE_CAPI void dse_mesh_streaming_add_lod(uint32_t mesh_id, uint32_t level, const char* path,
                                         float distance, uint32_t triangle_count);
DSE_CAPI void dse_mesh_streaming_tick(float cam_x, float cam_y, float cam_z, float dt);
DSE_CAPI int  dse_mesh_streaming_get_current_lod(uint32_t mesh_id);
DSE_CAPI int  dse_mesh_streaming_get_mesh_count(void);
DSE_CAPI void dse_mesh_streaming_shutdown(void);

// Physics LOD
DSE_CAPI void dse_physics_lod_init(float full_distance, float reduced_distance,
                                   float simplified_distance);
DSE_CAPI void dse_physics_lod_register_body(uint32_t entity_id,
                                            float x, float y, float z, float radius);
// evaluate：激活 body 的 entity_id 写入 out_ids（最多 cap 个），返回激活总数。
// out_ids=null 时仅返回总数。
DSE_CAPI int  dse_physics_lod_evaluate(float cam_x, float cam_y, float cam_z,
                                       uint32_t frame, uint32_t* out_ids, int cap);
// stats：out={full,reduced,simplified,sleeping}（int[4]）。返回 1=系统已初始化。
DSE_CAPI int  dse_physics_lod_get_stats(int* out_stats);
DSE_CAPI void dse_physics_lod_wake(uint32_t entity_id);
DSE_CAPI void dse_physics_lod_sleep(uint32_t entity_id);
DSE_CAPI void dse_physics_lod_shutdown(void);

// Terrain Deformation
DSE_CAPI void dse_terrain_deform_init(float max_depth, float max_height);
DSE_CAPI int  dse_terrain_deform_apply(int type, float x, float y, float z,
                                       float radius, float strength);
DSE_CAPI int  dse_terrain_deform_undo(void);
DSE_CAPI int  dse_terrain_deform_redo(void);
DSE_CAPI float dse_terrain_deform_sample_height(float x, float z);
DSE_CAPI void dse_terrain_deform_shutdown(void);

// Audio LOD
DSE_CAPI void dse_audio_lod_init(float full_distance, int max_active_sources);
DSE_CAPI uint32_t dse_audio_lod_register_source(const char* path,
                                                float x, float y, float z,
                                                float max_distance, float priority);
DSE_CAPI void dse_audio_lod_tick(float lx, float ly, float lz, float dt);
// stats：out={full,reduced,virtual,culled}（int[4]）。返回 1=系统已初始化。
DSE_CAPI int  dse_audio_lod_get_stats(int* out_stats);
DSE_CAPI int  dse_audio_lod_is_audible(uint32_t source_id);
DSE_CAPI void dse_audio_lod_shutdown(void);

// 释放所有 P2-P5 大世界系统实例。
DSE_CAPI void dse_open_world_p2p5_shutdown(void);

// ============================================================
// Cutscene — 过场/导演系统（实例句柄由本层管理）
// ============================================================

// 相机轨道应用回调。
typedef void (*dse_cutscene_camera_fn)(float px, float py, float pz,
                                       float lx, float ly, float lz,
                                       float fov, void* user_data);
// 事件轨道触发回调。
typedef void (*dse_cutscene_event_fn)(const char* event_name, const char* payload,
                                      void* user_data);
// 序列播放完成回调。
typedef void (*dse_cutscene_finish_fn)(const char* seq_name, void* user_data);

DSE_CAPI int   dse_cutscene_create(void);
DSE_CAPI void  dse_cutscene_destroy(int player_id);
DSE_CAPI void  dse_cutscene_shutdown(void);

DSE_CAPI void  dse_cutscene_add_sequence(int player_id, const char* name, float duration);
DSE_CAPI void  dse_cutscene_remove_sequence(int player_id, const char* name);

DSE_CAPI void  dse_cutscene_add_camera_keyframe(int player_id, const char* seq_name, float time,
                                                float px, float py, float pz,
                                                float lx, float ly, float lz, float fov);
// interp: 0=Linear 1=Step 2=CubicBezier。
DSE_CAPI void  dse_cutscene_add_property_keyframe(int player_id, const char* seq_name,
                                                  const char* track_name, float time,
                                                  float value, int interp);
DSE_CAPI void  dse_cutscene_add_event(int player_id, const char* seq_name, float time,
                                      const char* event_name, const char* payload);
DSE_CAPI void  dse_cutscene_add_audio_cue(int player_id, const char* seq_name, float time,
                                          const char* path, float volume, int loop);

DSE_CAPI void  dse_cutscene_play(int player_id, const char* seq_name);
DSE_CAPI void  dse_cutscene_pause(int player_id);
DSE_CAPI void  dse_cutscene_resume(int player_id);
DSE_CAPI void  dse_cutscene_stop(int player_id);
DSE_CAPI void  dse_cutscene_seek(int player_id, float time);
DSE_CAPI float dse_cutscene_get_time(int player_id);
// 返回 0=stopped 1=playing 2=paused。
DSE_CAPI int   dse_cutscene_get_state(int player_id);
DSE_CAPI void  dse_cutscene_set_play_rate(int player_id, float rate);
DSE_CAPI void  dse_cutscene_update(int player_id, float dt);

DSE_CAPI void  dse_cutscene_set_camera_callback(int player_id, const char* seq_name,
                                                dse_cutscene_camera_fn fn, void* user_data);
DSE_CAPI void  dse_cutscene_set_event_callback(int player_id, const char* seq_name,
                                               dse_cutscene_event_fn fn, void* user_data);
DSE_CAPI void  dse_cutscene_set_finish_callback(int player_id,
                                                dse_cutscene_finish_fn fn, void* user_data);

// ============================================================
// AI（行为树 + GOAP 规划器）
// ============================================================

// 条件回调：返回 0=失败 1=成功。
typedef int (*dse_ai_condition_fn)(void* user_data);
// 动作回调：返回 0=failure 1=success 2=running。
typedef int (*dse_ai_action_fn)(float dt, void* user_data);
// user_data 释放回调（节点销毁时调用，可为 NULL）。
typedef void (*dse_ai_destroy_fn)(void* user_data);

DSE_CAPI int   dse_ai_tree_create(const char* name);
DSE_CAPI void  dse_ai_tree_destroy(int tree_id);
// 返回 0=failure 1=success 2=running。
DSE_CAPI int   dse_ai_tree_tick(int tree_id, float dt);
DSE_CAPI void  dse_ai_tree_reset(int tree_id);
DSE_CAPI void  dse_ai_shutdown(void);

// 黑板
DSE_CAPI void  dse_ai_bb_set_bool(int tree_id, const char* key, int v);
DSE_CAPI void  dse_ai_bb_set_int(int tree_id, const char* key, int v);
DSE_CAPI void  dse_ai_bb_set_float(int tree_id, const char* key, float v);
DSE_CAPI void  dse_ai_bb_set_string(int tree_id, const char* key, const char* v);
DSE_CAPI void  dse_ai_bb_set_vec3(int tree_id, const char* key, float x, float y, float z);
DSE_CAPI int   dse_ai_bb_get_bool(int tree_id, const char* key);
DSE_CAPI int   dse_ai_bb_get_int(int tree_id, const char* key);
DSE_CAPI float dse_ai_bb_get_float(int tree_id, const char* key);
DSE_CAPI int   dse_ai_bb_get_string(int tree_id, const char* key, char* out, int cap);
DSE_CAPI int   dse_ai_bb_get_vec3(int tree_id, const char* key, float* out_xyz);

// 树构建（栈式）
DSE_CAPI void  dse_ai_begin_sequence(int tree_id, const char* name);
DSE_CAPI void  dse_ai_begin_selector(int tree_id, const char* name);
// require_one: 0=RequireAll 1=RequireOne。
DSE_CAPI void  dse_ai_begin_parallel(int tree_id, int require_one, const char* name);
DSE_CAPI void  dse_ai_end_composite(int tree_id);
DSE_CAPI void  dse_ai_add_condition(int tree_id, const char* name, dse_ai_condition_fn fn,
                                    void* user_data, dse_ai_destroy_fn destroy);
DSE_CAPI void  dse_ai_add_action(int tree_id, const char* name, dse_ai_action_fn fn,
                                 void* user_data, dse_ai_destroy_fn destroy);
DSE_CAPI void  dse_ai_add_inverter(int tree_id, const char* name);
DSE_CAPI void  dse_ai_add_succeeder(int tree_id, const char* name);
// max_repeats<0 = 无限。
DSE_CAPI void  dse_ai_add_repeater(int tree_id, const char* name, int max_repeats);

// GOAP
DSE_CAPI int   dse_ai_goap_create(void);
DSE_CAPI void  dse_ai_goap_destroy(int planner_id);
DSE_CAPI void  dse_ai_goap_action_begin(int planner_id, const char* name, float cost);
DSE_CAPI void  dse_ai_goap_action_precondition(int planner_id, const char* key, int value);
DSE_CAPI void  dse_ai_goap_action_effect(int planner_id, const char* key, int value);
DSE_CAPI void  dse_ai_goap_action_commit(int planner_id);
DSE_CAPI void  dse_ai_goap_state_clear(int planner_id);
// which: 0=current_state 1=goal。
DSE_CAPI void  dse_ai_goap_state_set(int planner_id, int which, const char* key, int value);
// 规划：动作名以 '\n' 分隔写入 out（null 结尾，按 cap 截断）。返回 -1=无解，否则写入长度。
DSE_CAPI int   dse_ai_goap_plan(int planner_id, char* out, int cap);

#ifdef __cplusplus
}
#endif

// Codegen 生成的组件字段 C ABI 声明（dse_api_<prefix>.gen.cpp 实现）
#include "dse_api.gen.h"

#endif // DSE_API_H
