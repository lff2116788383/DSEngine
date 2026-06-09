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

#ifdef __cplusplus
}
#endif

#endif // DSE_API_H
