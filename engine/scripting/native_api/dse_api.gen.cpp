/**
 * @file dse_api.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * 注意：本文件当前被 CMakeLists.txt 过滤（*.gen.cpp 不参与构建）。
 * 仅当手写 dse_api.cpp 中对应函数被删除后，才将本文件 opt-in 进构建。
 * 依赖 dse_api.h 提供的 dse_get_world_ptr() 访问已初始化的 World 指针。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_tree.h"
#include "engine/ecs/components_3d_terrain_tile.h"
#include "engine/ecs/components_3d_navmesh.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <entt/entt.hpp>

using Entity = entt::entity;

namespace {
inline World* GW() { return static_cast<World*>(dse_get_world_ptr()); }
inline bool V(uint32_t e) { World* w = GW(); return w && w->registry().valid(static_cast<Entity>(static_cast<entt::id_type>(e))); }
inline Entity TE(uint32_t e) { return static_cast<Entity>(static_cast<entt::id_type>(e)); }
template<typename T> T* GC(uint32_t e) { World* w = GW(); if (!V(e)) return nullptr; return w->registry().try_get<T>(TE(e)); }
template<typename T> const T* GCC(uint32_t e) { return GC<T>(e); }
}

/* ---- TransformComponent ---- */
extern "C" void dse_transform_get_position(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<TransformComponent>(e)) { *x = c->position.x; *y = c->position.y; *z = c->position.z; }
}
extern "C" void dse_transform_set_position(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<TransformComponent>(e)) {
        c->position = glm::vec3(x, y, z);
        c->dirty = true;
    }
}
extern "C" void dse_transform_get_rotation(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<TransformComponent>(e)) {
        glm::vec3 euler = glm::degrees(glm::eulerAngles(c->rotation));
        *x = euler.x; *y = euler.y; *z = euler.z;
    }
}
extern "C" void dse_transform_set_rotation(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<TransformComponent>(e)) {
        c->rotation = glm::quat(glm::vec3(glm::radians(x), glm::radians(y), glm::radians(z)));
        c->dirty = true;
    }
}
extern "C" void dse_transform_get_scale(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<TransformComponent>(e)) { *x = c->scale.x; *y = c->scale.y; *z = c->scale.z; }
}
extern "C" void dse_transform_set_scale(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<TransformComponent>(e)) {
        c->scale = glm::vec3(x, y, z);
        c->dirty = true;
    }
}

/* ---- Camera3DComponent ---- */
extern "C" float dse_camera3d_get_fov(uint32_t e) {
    const auto* c = GCC<dse::Camera3DComponent>(e);
    return c ? c->fov : 60.0f;
}
extern "C" void dse_camera3d_set_fov(uint32_t e, float v) {
    if (auto* c = GC<dse::Camera3DComponent>(e)) {
        c->fov = v;
    }
}
extern "C" float dse_camera3d_get_near_clip(uint32_t e) {
    const auto* c = GCC<dse::Camera3DComponent>(e);
    return c ? c->near_clip : 0.1f;
}
extern "C" void dse_camera3d_set_near_clip(uint32_t e, float v) {
    if (auto* c = GC<dse::Camera3DComponent>(e)) {
        c->near_clip = v;
    }
}
extern "C" float dse_camera3d_get_far_clip(uint32_t e) {
    const auto* c = GCC<dse::Camera3DComponent>(e);
    return c ? c->far_clip : 1000.0f;
}
extern "C" void dse_camera3d_set_far_clip(uint32_t e, float v) {
    if (auto* c = GC<dse::Camera3DComponent>(e)) {
        c->far_clip = v;
    }
}
extern "C" int dse_camera3d_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::Camera3DComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_camera3d_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::Camera3DComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" int dse_camera3d_get_priority(uint32_t e) {
    const auto* c = GCC<dse::Camera3DComponent>(e);
    return c ? c->priority : 0;
}
extern "C" void dse_camera3d_set_priority(uint32_t e, int v) {
    if (auto* c = GC<dse::Camera3DComponent>(e)) {
        c->priority = v;
    }
}

/* ---- MeshRendererComponent ---- */
extern "C" void dse_mesh_renderer_get_color(uint32_t e, float* x, float* y, float* z, float* w) {
    if (const auto* c = GCC<dse::MeshRendererComponent>(e)) { *x = c->color.x; *y = c->color.y; *z = c->color.z; *w = c->color.w; }
}
extern "C" void dse_mesh_renderer_set_color(uint32_t e, float x, float y, float z, float w) {
    if (auto* c = GC<dse::MeshRendererComponent>(e)) c->color = glm::vec4(x, y, z, w);
}
extern "C" int dse_mesh_renderer_get_visible(uint32_t e) {
    const auto* c = GCC<dse::MeshRendererComponent>(e);
    return (c && c->visible) ? 1 : 0;
}
extern "C" void dse_mesh_renderer_set_visible(uint32_t e, int v) {
    if (auto* c = GC<dse::MeshRendererComponent>(e)) {
        c->visible = (v != 0);
    }
}
extern "C" float dse_mesh_renderer_get_metallic(uint32_t e) {
    const auto* c = GCC<dse::MeshRendererComponent>(e);
    return c ? c->metallic : 0.0f;
}
extern "C" void dse_mesh_renderer_set_metallic(uint32_t e, float v) {
    if (auto* c = GC<dse::MeshRendererComponent>(e)) {
        c->metallic = v;
    }
}
extern "C" float dse_mesh_renderer_get_roughness(uint32_t e) {
    const auto* c = GCC<dse::MeshRendererComponent>(e);
    return c ? c->roughness : 0.5f;
}
extern "C" void dse_mesh_renderer_set_roughness(uint32_t e, float v) {
    if (auto* c = GC<dse::MeshRendererComponent>(e)) {
        c->roughness = v;
    }
}
extern "C" void dse_mesh_renderer_get_emissive(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::MeshRendererComponent>(e)) { *x = c->emissive.x; *y = c->emissive.y; *z = c->emissive.z; }
}
extern "C" void dse_mesh_renderer_set_emissive(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::MeshRendererComponent>(e)) {
        c->emissive = glm::vec3(x, y, z);
    }
}
extern "C" int dse_mesh_renderer_get_receive_shadow(uint32_t e) {
    const auto* c = GCC<dse::MeshRendererComponent>(e);
    return (c && c->receive_shadow) ? 1 : 0;
}
extern "C" void dse_mesh_renderer_set_receive_shadow(uint32_t e, int v) {
    if (auto* c = GC<dse::MeshRendererComponent>(e)) {
        c->receive_shadow = (v != 0);
    }
}

/* ---- DirectionalLight3DComponent ---- */
extern "C" void dse_dir_light_get_direction(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::DirectionalLight3DComponent>(e)) { *x = c->direction.x; *y = c->direction.y; *z = c->direction.z; }
}
extern "C" void dse_dir_light_set_direction(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::DirectionalLight3DComponent>(e)) {
        c->direction = glm::vec3(x, y, z);
    }
}
extern "C" void dse_dir_light_get_color(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::DirectionalLight3DComponent>(e)) { *x = c->color.x; *y = c->color.y; *z = c->color.z; }
}
extern "C" void dse_dir_light_set_color(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::DirectionalLight3DComponent>(e)) {
        c->color = glm::vec3(x, y, z);
    }
}
extern "C" float dse_dir_light_get_intensity(uint32_t e) {
    const auto* c = GCC<dse::DirectionalLight3DComponent>(e);
    return c ? c->intensity : 1.0f;
}
extern "C" void dse_dir_light_set_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::DirectionalLight3DComponent>(e)) {
        c->intensity = v;
    }
}
extern "C" float dse_dir_light_get_ambient_intensity(uint32_t e) {
    const auto* c = GCC<dse::DirectionalLight3DComponent>(e);
    return c ? c->ambient_intensity : 0.2f;
}
extern "C" void dse_dir_light_set_ambient_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::DirectionalLight3DComponent>(e)) {
        c->ambient_intensity = v;
    }
}
extern "C" int dse_dir_light_get_cast_shadow(uint32_t e) {
    const auto* c = GCC<dse::DirectionalLight3DComponent>(e);
    return (c && c->cast_shadow) ? 1 : 0;
}
extern "C" void dse_dir_light_set_cast_shadow(uint32_t e, int v) {
    if (auto* c = GC<dse::DirectionalLight3DComponent>(e)) {
        c->cast_shadow = (v != 0);
    }
}
extern "C" float dse_dir_light_get_shadow_strength(uint32_t e) {
    const auto* c = GCC<dse::DirectionalLight3DComponent>(e);
    return c ? c->shadow_strength : 0.35f;
}
extern "C" void dse_dir_light_set_shadow_strength(uint32_t e, float v) {
    if (auto* c = GC<dse::DirectionalLight3DComponent>(e)) {
        c->shadow_strength = v;
    }
}

/* ---- PointLightComponent ---- */
extern "C" void dse_point_light_get_color(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::PointLightComponent>(e)) { *x = c->color.x; *y = c->color.y; *z = c->color.z; }
}
extern "C" void dse_point_light_set_color(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::PointLightComponent>(e)) {
        c->color = glm::vec3(x, y, z);
    }
}
extern "C" float dse_point_light_get_intensity(uint32_t e) {
    const auto* c = GCC<dse::PointLightComponent>(e);
    return c ? c->intensity : 1.0f;
}
extern "C" void dse_point_light_set_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::PointLightComponent>(e)) {
        c->intensity = v;
    }
}
extern "C" float dse_point_light_get_radius(uint32_t e) {
    const auto* c = GCC<dse::PointLightComponent>(e);
    return c ? c->radius : 10.0f;
}
extern "C" void dse_point_light_set_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::PointLightComponent>(e)) {
        c->radius = v;
    }
}
extern "C" int dse_point_light_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::PointLightComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_point_light_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PointLightComponent>(e)) {
        c->enabled = (v != 0);
    }
}

/* ---- SpotLightComponent ---- */
extern "C" void dse_spot_light_get_color(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::SpotLightComponent>(e)) { *x = c->color.x; *y = c->color.y; *z = c->color.z; }
}
extern "C" void dse_spot_light_set_color(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) {
        c->color = glm::vec3(x, y, z);
    }
}
extern "C" float dse_spot_light_get_intensity(uint32_t e) {
    const auto* c = GCC<dse::SpotLightComponent>(e);
    return c ? c->intensity : 1.0f;
}
extern "C" void dse_spot_light_set_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) {
        c->intensity = v;
    }
}
extern "C" float dse_spot_light_get_radius(uint32_t e) {
    const auto* c = GCC<dse::SpotLightComponent>(e);
    return c ? c->radius : 20.0f;
}
extern "C" void dse_spot_light_set_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) {
        c->radius = v;
    }
}
extern "C" float dse_spot_light_get_inner_cone_angle(uint32_t e) {
    const auto* c = GCC<dse::SpotLightComponent>(e);
    return c ? c->inner_cone_angle : 12.5f;
}
extern "C" void dse_spot_light_set_inner_cone_angle(uint32_t e, float v) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) {
        c->inner_cone_angle = v;
    }
}
extern "C" float dse_spot_light_get_outer_cone_angle(uint32_t e) {
    const auto* c = GCC<dse::SpotLightComponent>(e);
    return c ? c->outer_cone_angle : 17.5f;
}
extern "C" void dse_spot_light_set_outer_cone_angle(uint32_t e, float v) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) {
        c->outer_cone_angle = v;
    }
}
extern "C" void dse_spot_light_get_direction(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::SpotLightComponent>(e)) { *x = c->direction.x; *y = c->direction.y; *z = c->direction.z; }
}
extern "C" void dse_spot_light_set_direction(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) {
        c->direction = glm::vec3(x, y, z);
    }
}
extern "C" int dse_spot_light_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::SpotLightComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_spot_light_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" int dse_spot_light_get_cast_shadow(uint32_t e) {
    const auto* c = GCC<dse::SpotLightComponent>(e);
    return (c && c->cast_shadow) ? 1 : 0;
}
extern "C" void dse_spot_light_set_cast_shadow(uint32_t e, int v) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) {
        c->cast_shadow = (v != 0);
    }
}

/* ---- SkyLightComponent ---- */
extern "C" void dse_sky_light_get_up_color(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::SkyLightComponent>(e)) { *x = c->up_color.x; *y = c->up_color.y; *z = c->up_color.z; }
}
extern "C" void dse_sky_light_set_up_color(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::SkyLightComponent>(e)) {
        c->up_color = glm::vec3(x, y, z);
    }
}
extern "C" void dse_sky_light_get_down_color(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::SkyLightComponent>(e)) { *x = c->down_color.x; *y = c->down_color.y; *z = c->down_color.z; }
}
extern "C" void dse_sky_light_set_down_color(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::SkyLightComponent>(e)) {
        c->down_color = glm::vec3(x, y, z);
    }
}
extern "C" float dse_sky_light_get_intensity(uint32_t e) {
    const auto* c = GCC<dse::SkyLightComponent>(e);
    return c ? c->intensity : 1.0f;
}
extern "C" void dse_sky_light_set_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::SkyLightComponent>(e)) {
        c->intensity = v;
    }
}
extern "C" int dse_sky_light_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::SkyLightComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_sky_light_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::SkyLightComponent>(e)) {
        c->enabled = (v != 0);
    }
}

/* ---- TreeComponent ---- */
extern "C" int dse_tree_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::TreeComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_tree_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" float dse_tree_get_density(uint32_t e) {
    const auto* c = GCC<dse::TreeComponent>(e);
    return c ? c->density : 0.02f;
}
extern "C" void dse_tree_set_density(uint32_t e, float v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->density = v;
    }
}
extern "C" float dse_tree_get_spawn_radius(uint32_t e) {
    const auto* c = GCC<dse::TreeComponent>(e);
    return c ? c->spawn_radius : 120.0f;
}
extern "C" void dse_tree_set_spawn_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->spawn_radius = v;
    }
}
extern "C" float dse_tree_get_chunk_size(uint32_t e) {
    const auto* c = GCC<dse::TreeComponent>(e);
    return c ? c->chunk_size : 32.0f;
}
extern "C" void dse_tree_set_chunk_size(uint32_t e, float v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->chunk_size = v;
    }
}
extern "C" float dse_tree_get_min_scale(uint32_t e) {
    const auto* c = GCC<dse::TreeComponent>(e);
    return c ? c->min_scale : 0.8f;
}
extern "C" void dse_tree_set_min_scale(uint32_t e, float v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->min_scale = v;
    }
}
extern "C" float dse_tree_get_max_scale(uint32_t e) {
    const auto* c = GCC<dse::TreeComponent>(e);
    return c ? c->max_scale : 1.3f;
}
extern "C" void dse_tree_set_max_scale(uint32_t e, float v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->max_scale = v;
    }
}
extern "C" float dse_tree_get_lod1_distance(uint32_t e) {
    const auto* c = GCC<dse::TreeComponent>(e);
    return c ? c->lod1_distance : 60.0f;
}
extern "C" void dse_tree_set_lod1_distance(uint32_t e, float v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->lod1_distance = v;
    }
}
extern "C" float dse_tree_get_cull_distance(uint32_t e) {
    const auto* c = GCC<dse::TreeComponent>(e);
    return c ? c->cull_distance : 200.0f;
}
extern "C" void dse_tree_set_cull_distance(uint32_t e, float v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->cull_distance = v;
    }
}
extern "C" float dse_tree_get_wind_strength(uint32_t e) {
    const auto* c = GCC<dse::TreeComponent>(e);
    return c ? c->wind_strength : 0.3f;
}
extern "C" void dse_tree_set_wind_strength(uint32_t e, float v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->wind_strength = v;
    }
}
extern "C" float dse_tree_get_wind_speed(uint32_t e) {
    const auto* c = GCC<dse::TreeComponent>(e);
    return c ? c->wind_speed : 1.0f;
}
extern "C" void dse_tree_set_wind_speed(uint32_t e, float v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->wind_speed = v;
    }
}
extern "C" int dse_tree_get_cast_shadow(uint32_t e) {
    const auto* c = GCC<dse::TreeComponent>(e);
    return (c && c->cast_shadow) ? 1 : 0;
}
extern "C" void dse_tree_set_cast_shadow(uint32_t e, int v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->cast_shadow = (v != 0);
    }
}
extern "C" float dse_tree_get_shadow_distance(uint32_t e) {
    const auto* c = GCC<dse::TreeComponent>(e);
    return c ? c->shadow_distance : 80.0f;
}
extern "C" void dse_tree_set_shadow_distance(uint32_t e, float v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->shadow_distance = v;
    }
}
extern "C" int dse_tree_get_seed(uint32_t e) {
    const auto* c = GCC<dse::TreeComponent>(e);
    return c ? c->seed : 12345;
}
extern "C" void dse_tree_set_seed(uint32_t e, int v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->seed = v;
    }
}
extern "C" float dse_tree_get_height_variation(uint32_t e) {
    const auto* c = GCC<dse::TreeComponent>(e);
    return c ? c->height_variation : 0.2f;
}
extern "C" void dse_tree_set_height_variation(uint32_t e, float v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->height_variation = v;
    }
}
extern "C" int dse_tree_get_random_rotation(uint32_t e) {
    const auto* c = GCC<dse::TreeComponent>(e);
    return (c && c->random_rotation) ? 1 : 0;
}
extern "C" void dse_tree_set_random_rotation(uint32_t e, int v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->random_rotation = (v != 0);
    }
}
extern "C" float dse_tree_get_billboard_distance(uint32_t e) {
    const auto* c = GCC<dse::TreeComponent>(e);
    return c ? c->billboard_distance : 150.0f;
}
extern "C" void dse_tree_set_billboard_distance(uint32_t e, float v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->billboard_distance = v;
    }
}

/* ---- TerrainTileManagerComponent ---- */
extern "C" int dse_terrain_tile_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::TerrainTileManagerComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_terrain_tile_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::TerrainTileManagerComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" float dse_terrain_tile_get_tile_world_size(uint32_t e) {
    const auto* c = GCC<dse::TerrainTileManagerComponent>(e);
    return c ? c->tile_world_size : 64.0f;
}
extern "C" void dse_terrain_tile_set_tile_world_size(uint32_t e, float v) {
    if (auto* c = GC<dse::TerrainTileManagerComponent>(e)) {
        c->tile_world_size = v;
    }
}
extern "C" int dse_terrain_tile_get_tile_resolution(uint32_t e) {
    const auto* c = GCC<dse::TerrainTileManagerComponent>(e);
    return c ? c->tile_resolution : 64;
}
extern "C" void dse_terrain_tile_set_tile_resolution(uint32_t e, int v) {
    if (auto* c = GC<dse::TerrainTileManagerComponent>(e)) {
        c->tile_resolution = v;
    }
}
extern "C" float dse_terrain_tile_get_max_height(uint32_t e) {
    const auto* c = GCC<dse::TerrainTileManagerComponent>(e);
    return c ? c->max_height : 20.0f;
}
extern "C" void dse_terrain_tile_set_max_height(uint32_t e, float v) {
    if (auto* c = GC<dse::TerrainTileManagerComponent>(e)) {
        c->max_height = v;
    }
}
extern "C" float dse_terrain_tile_get_load_radius(uint32_t e) {
    const auto* c = GCC<dse::TerrainTileManagerComponent>(e);
    return c ? c->load_radius : 200.0f;
}
extern "C" void dse_terrain_tile_set_load_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::TerrainTileManagerComponent>(e)) {
        c->load_radius = v;
    }
}
extern "C" float dse_terrain_tile_get_unload_radius(uint32_t e) {
    const auto* c = GCC<dse::TerrainTileManagerComponent>(e);
    return c ? c->unload_radius : 250.0f;
}
extern "C" void dse_terrain_tile_set_unload_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::TerrainTileManagerComponent>(e)) {
        c->unload_radius = v;
    }
}
extern "C" int dse_terrain_tile_get_use_procedural(uint32_t e) {
    const auto* c = GCC<dse::TerrainTileManagerComponent>(e);
    return (c && c->use_procedural) ? 1 : 0;
}
extern "C" void dse_terrain_tile_set_use_procedural(uint32_t e, int v) {
    if (auto* c = GC<dse::TerrainTileManagerComponent>(e)) {
        c->use_procedural = (v != 0);
    }
}
extern "C" float dse_terrain_tile_get_procedural_base_height(uint32_t e) {
    const auto* c = GCC<dse::TerrainTileManagerComponent>(e);
    return c ? c->procedural_base_height : 0.0f;
}
extern "C" void dse_terrain_tile_set_procedural_base_height(uint32_t e, float v) {
    if (auto* c = GC<dse::TerrainTileManagerComponent>(e)) {
        c->procedural_base_height = v;
    }
}
extern "C" int dse_terrain_tile_get_max_lod_levels(uint32_t e) {
    const auto* c = GCC<dse::TerrainTileManagerComponent>(e);
    return c ? c->max_lod_levels : 4;
}
extern "C" void dse_terrain_tile_set_max_lod_levels(uint32_t e, int v) {
    if (auto* c = GC<dse::TerrainTileManagerComponent>(e)) {
        c->max_lod_levels = v;
    }
}
extern "C" float dse_terrain_tile_get_lod_distance_factor(uint32_t e) {
    const auto* c = GCC<dse::TerrainTileManagerComponent>(e);
    return c ? c->lod_distance_factor : 50.0f;
}
extern "C" void dse_terrain_tile_set_lod_distance_factor(uint32_t e, float v) {
    if (auto* c = GC<dse::TerrainTileManagerComponent>(e)) {
        c->lod_distance_factor = v;
    }
}

/* ---- DynamicObstacleComponent ---- */
extern "C" int dse_dyn_obstacle_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::DynamicObstacleComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_dyn_obstacle_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::DynamicObstacleComponent>(e)) {
        c->enabled = (v != 0);
        c->dirty_ = true;
    }
}
extern "C" int dse_dyn_obstacle_get_shape(uint32_t e) {
    const auto* c = GCC<dse::DynamicObstacleComponent>(e);
    return c ? c->shape : 0;
}
extern "C" void dse_dyn_obstacle_set_shape(uint32_t e, int v) {
    if (auto* c = GC<dse::DynamicObstacleComponent>(e)) {
        c->shape = v;
        c->dirty_ = true;
    }
}
extern "C" void dse_dyn_obstacle_get_box_extents(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::DynamicObstacleComponent>(e)) { *x = c->box_extents.x; *y = c->box_extents.y; *z = c->box_extents.z; }
}
extern "C" void dse_dyn_obstacle_set_box_extents(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::DynamicObstacleComponent>(e)) {
        c->box_extents = glm::vec3(x, y, z);
        c->dirty_ = true;
    }
}
extern "C" float dse_dyn_obstacle_get_cylinder_radius(uint32_t e) {
    const auto* c = GCC<dse::DynamicObstacleComponent>(e);
    return c ? c->cylinder_radius : 1.0f;
}
extern "C" void dse_dyn_obstacle_set_cylinder_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::DynamicObstacleComponent>(e)) {
        c->cylinder_radius = v;
        c->dirty_ = true;
    }
}
extern "C" float dse_dyn_obstacle_get_cylinder_height(uint32_t e) {
    const auto* c = GCC<dse::DynamicObstacleComponent>(e);
    return c ? c->cylinder_height : 2.0f;
}
extern "C" void dse_dyn_obstacle_set_cylinder_height(uint32_t e, float v) {
    if (auto* c = GC<dse::DynamicObstacleComponent>(e)) {
        c->cylinder_height = v;
        c->dirty_ = true;
    }
}

/* ---- NavMeshAutoRebakeComponent ---- */
extern "C" int dse_navmesh_rebake_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::NavMeshAutoRebakeComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_navmesh_rebake_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::NavMeshAutoRebakeComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" float dse_navmesh_rebake_get_tile_size(uint32_t e) {
    const auto* c = GCC<dse::NavMeshAutoRebakeComponent>(e);
    return c ? c->tile_size : 48.0f;
}
extern "C" void dse_navmesh_rebake_set_tile_size(uint32_t e, float v) {
    if (auto* c = GC<dse::NavMeshAutoRebakeComponent>(e)) {
        c->tile_size = v;
    }
}
extern "C" float dse_navmesh_rebake_get_rebake_cooldown(uint32_t e) {
    const auto* c = GCC<dse::NavMeshAutoRebakeComponent>(e);
    return c ? c->rebake_cooldown : 1.0f;
}
extern "C" void dse_navmesh_rebake_set_rebake_cooldown(uint32_t e, float v) {
    if (auto* c = GC<dse::NavMeshAutoRebakeComponent>(e)) {
        c->rebake_cooldown = v;
    }
}
extern "C" int dse_navmesh_rebake_get_collect_terrain(uint32_t e) {
    const auto* c = GCC<dse::NavMeshAutoRebakeComponent>(e);
    return (c && c->collect_terrain) ? 1 : 0;
}
extern "C" void dse_navmesh_rebake_set_collect_terrain(uint32_t e, int v) {
    if (auto* c = GC<dse::NavMeshAutoRebakeComponent>(e)) {
        c->collect_terrain = (v != 0);
    }
}
extern "C" int dse_navmesh_rebake_get_collect_mesh_renderers(uint32_t e) {
    const auto* c = GCC<dse::NavMeshAutoRebakeComponent>(e);
    return (c && c->collect_mesh_renderers) ? 1 : 0;
}
extern "C" void dse_navmesh_rebake_set_collect_mesh_renderers(uint32_t e, int v) {
    if (auto* c = GC<dse::NavMeshAutoRebakeComponent>(e)) {
        c->collect_mesh_renderers = (v != 0);
    }
}
extern "C" float dse_navmesh_rebake_get_agent_height(uint32_t e) {
    const auto* c = GCC<dse::NavMeshAutoRebakeComponent>(e);
    return c ? c->agent_height : 2.0f;
}
extern "C" void dse_navmesh_rebake_set_agent_height(uint32_t e, float v) {
    if (auto* c = GC<dse::NavMeshAutoRebakeComponent>(e)) {
        c->agent_height = v;
    }
}
extern "C" float dse_navmesh_rebake_get_agent_radius(uint32_t e) {
    const auto* c = GCC<dse::NavMeshAutoRebakeComponent>(e);
    return c ? c->agent_radius : 0.6f;
}
extern "C" void dse_navmesh_rebake_set_agent_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::NavMeshAutoRebakeComponent>(e)) {
        c->agent_radius = v;
    }
}
extern "C" float dse_navmesh_rebake_get_agent_max_climb(uint32_t e) {
    const auto* c = GCC<dse::NavMeshAutoRebakeComponent>(e);
    return c ? c->agent_max_climb : 0.9f;
}
extern "C" void dse_navmesh_rebake_set_agent_max_climb(uint32_t e, float v) {
    if (auto* c = GC<dse::NavMeshAutoRebakeComponent>(e)) {
        c->agent_max_climb = v;
    }
}
extern "C" float dse_navmesh_rebake_get_agent_max_slope(uint32_t e) {
    const auto* c = GCC<dse::NavMeshAutoRebakeComponent>(e);
    return c ? c->agent_max_slope : 45.0f;
}
extern "C" void dse_navmesh_rebake_set_agent_max_slope(uint32_t e, float v) {
    if (auto* c = GC<dse::NavMeshAutoRebakeComponent>(e)) {
        c->agent_max_slope = v;
    }
}
extern "C" float dse_navmesh_rebake_get_cell_size(uint32_t e) {
    const auto* c = GCC<dse::NavMeshAutoRebakeComponent>(e);
    return c ? c->cell_size : 0.3f;
}
extern "C" void dse_navmesh_rebake_set_cell_size(uint32_t e, float v) {
    if (auto* c = GC<dse::NavMeshAutoRebakeComponent>(e)) {
        c->cell_size = v;
    }
}
extern "C" float dse_navmesh_rebake_get_cell_height(uint32_t e) {
    const auto* c = GCC<dse::NavMeshAutoRebakeComponent>(e);
    return c ? c->cell_height : 0.2f;
}
extern "C" void dse_navmesh_rebake_set_cell_height(uint32_t e, float v) {
    if (auto* c = GC<dse::NavMeshAutoRebakeComponent>(e)) {
        c->cell_height = v;
    }
}

