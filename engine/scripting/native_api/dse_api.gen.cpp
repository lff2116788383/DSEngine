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
    if (auto* c = GC<dse::Camera3DComponent>(e)) c->fov = v;
}
extern "C" float dse_camera3d_get_near_clip(uint32_t e) {
    const auto* c = GCC<dse::Camera3DComponent>(e);
    return c ? c->near_clip : 0.1f;
}
extern "C" void dse_camera3d_set_near_clip(uint32_t e, float v) {
    if (auto* c = GC<dse::Camera3DComponent>(e)) c->near_clip = v;
}
extern "C" float dse_camera3d_get_far_clip(uint32_t e) {
    const auto* c = GCC<dse::Camera3DComponent>(e);
    return c ? c->far_clip : 1000.0f;
}
extern "C" void dse_camera3d_set_far_clip(uint32_t e, float v) {
    if (auto* c = GC<dse::Camera3DComponent>(e)) c->far_clip = v;
}
extern "C" int dse_camera3d_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::Camera3DComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_camera3d_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::Camera3DComponent>(e)) c->enabled = (v != 0);
}
extern "C" int dse_camera3d_get_priority(uint32_t e) {
    const auto* c = GCC<dse::Camera3DComponent>(e);
    return c ? c->priority : 0;
}
extern "C" void dse_camera3d_set_priority(uint32_t e, int v) {
    if (auto* c = GC<dse::Camera3DComponent>(e)) c->priority = v;
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
    if (auto* c = GC<dse::MeshRendererComponent>(e)) c->visible = (v != 0);
}
extern "C" float dse_mesh_renderer_get_metallic(uint32_t e) {
    const auto* c = GCC<dse::MeshRendererComponent>(e);
    return c ? c->metallic : 0.0f;
}
extern "C" void dse_mesh_renderer_set_metallic(uint32_t e, float v) {
    if (auto* c = GC<dse::MeshRendererComponent>(e)) c->metallic = v;
}
extern "C" float dse_mesh_renderer_get_roughness(uint32_t e) {
    const auto* c = GCC<dse::MeshRendererComponent>(e);
    return c ? c->roughness : 0.5f;
}
extern "C" void dse_mesh_renderer_set_roughness(uint32_t e, float v) {
    if (auto* c = GC<dse::MeshRendererComponent>(e)) c->roughness = v;
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
    if (auto* c = GC<dse::MeshRendererComponent>(e)) c->receive_shadow = (v != 0);
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
    if (auto* c = GC<dse::DirectionalLight3DComponent>(e)) c->intensity = v;
}
extern "C" float dse_dir_light_get_ambient_intensity(uint32_t e) {
    const auto* c = GCC<dse::DirectionalLight3DComponent>(e);
    return c ? c->ambient_intensity : 0.2f;
}
extern "C" void dse_dir_light_set_ambient_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::DirectionalLight3DComponent>(e)) c->ambient_intensity = v;
}
extern "C" int dse_dir_light_get_cast_shadow(uint32_t e) {
    const auto* c = GCC<dse::DirectionalLight3DComponent>(e);
    return (c && c->cast_shadow) ? 1 : 0;
}
extern "C" void dse_dir_light_set_cast_shadow(uint32_t e, int v) {
    if (auto* c = GC<dse::DirectionalLight3DComponent>(e)) c->cast_shadow = (v != 0);
}
extern "C" float dse_dir_light_get_shadow_strength(uint32_t e) {
    const auto* c = GCC<dse::DirectionalLight3DComponent>(e);
    return c ? c->shadow_strength : 0.35f;
}
extern "C" void dse_dir_light_set_shadow_strength(uint32_t e, float v) {
    if (auto* c = GC<dse::DirectionalLight3DComponent>(e)) c->shadow_strength = v;
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
    if (auto* c = GC<dse::PointLightComponent>(e)) c->intensity = v;
}
extern "C" float dse_point_light_get_radius(uint32_t e) {
    const auto* c = GCC<dse::PointLightComponent>(e);
    return c ? c->radius : 10.0f;
}
extern "C" void dse_point_light_set_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::PointLightComponent>(e)) c->radius = v;
}
extern "C" int dse_point_light_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::PointLightComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_point_light_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PointLightComponent>(e)) c->enabled = (v != 0);
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
    if (auto* c = GC<dse::SpotLightComponent>(e)) c->intensity = v;
}
extern "C" float dse_spot_light_get_radius(uint32_t e) {
    const auto* c = GCC<dse::SpotLightComponent>(e);
    return c ? c->radius : 20.0f;
}
extern "C" void dse_spot_light_set_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) c->radius = v;
}
extern "C" float dse_spot_light_get_inner_cone_angle(uint32_t e) {
    const auto* c = GCC<dse::SpotLightComponent>(e);
    return c ? c->inner_cone_angle : 12.5f;
}
extern "C" void dse_spot_light_set_inner_cone_angle(uint32_t e, float v) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) c->inner_cone_angle = v;
}
extern "C" float dse_spot_light_get_outer_cone_angle(uint32_t e) {
    const auto* c = GCC<dse::SpotLightComponent>(e);
    return c ? c->outer_cone_angle : 17.5f;
}
extern "C" void dse_spot_light_set_outer_cone_angle(uint32_t e, float v) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) c->outer_cone_angle = v;
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
    if (auto* c = GC<dse::SpotLightComponent>(e)) c->enabled = (v != 0);
}
extern "C" int dse_spot_light_get_cast_shadow(uint32_t e) {
    const auto* c = GCC<dse::SpotLightComponent>(e);
    return (c && c->cast_shadow) ? 1 : 0;
}
extern "C" void dse_spot_light_set_cast_shadow(uint32_t e, int v) {
    if (auto* c = GC<dse::SpotLightComponent>(e)) c->cast_shadow = (v != 0);
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
    if (auto* c = GC<dse::SkyLightComponent>(e)) c->intensity = v;
}
extern "C" int dse_sky_light_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::SkyLightComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_sky_light_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::SkyLightComponent>(e)) c->enabled = (v != 0);
}

