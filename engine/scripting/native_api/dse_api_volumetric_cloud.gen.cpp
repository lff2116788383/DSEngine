/**
 * @file dse_api_volumetric_cloud.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：VolumetricCloudComponent）
 *
 * 组件字段 get/set 由 Codegen 生成（每组件一个 TU，与 Lua 拆分边界对齐）；
 * 手写 dse_api.cpp 仅保留 add/字符串/Input 等非字段 API。
 * 依赖 dse_api.h 提供的 dse_get_world_ptr() 访问已初始化的 World 指针。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_sky.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <entt/entt.hpp>
#include <cstring>

using Entity = entt::entity;

namespace {
inline World* GW() { return static_cast<World*>(dse_get_world_ptr()); }
inline bool V(uint32_t e) { World* w = GW(); return w && w->registry().valid(static_cast<Entity>(static_cast<entt::id_type>(e))); }
inline Entity TE(uint32_t e) { return static_cast<Entity>(static_cast<entt::id_type>(e)); }
template<typename T> T* GC(uint32_t e) { World* w = GW(); if (!V(e)) return nullptr; return w->registry().try_get<T>(TE(e)); }
template<typename T> const T* GCC(uint32_t e) { return GC<T>(e); }
}

/* ---- VolumetricCloudComponent ---- */
extern "C" int dse_volumetric_cloud_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::VolumetricCloudComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_volumetric_cloud_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::VolumetricCloudComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" float dse_volumetric_cloud_get_cloud_bottom(uint32_t e) {
    const auto* c = GCC<dse::VolumetricCloudComponent>(e);
    return c ? c->cloud_bottom : 0.0f;
}
extern "C" void dse_volumetric_cloud_set_cloud_bottom(uint32_t e, float v) {
    if (auto* c = GC<dse::VolumetricCloudComponent>(e)) {
        c->cloud_bottom = v;
    }
}
extern "C" float dse_volumetric_cloud_get_cloud_top(uint32_t e) {
    const auto* c = GCC<dse::VolumetricCloudComponent>(e);
    return c ? c->cloud_top : 0.0f;
}
extern "C" void dse_volumetric_cloud_set_cloud_top(uint32_t e, float v) {
    if (auto* c = GC<dse::VolumetricCloudComponent>(e)) {
        c->cloud_top = v;
    }
}
extern "C" float dse_volumetric_cloud_get_coverage(uint32_t e) {
    const auto* c = GCC<dse::VolumetricCloudComponent>(e);
    return c ? c->coverage : 0.0f;
}
extern "C" void dse_volumetric_cloud_set_coverage(uint32_t e, float v) {
    if (auto* c = GC<dse::VolumetricCloudComponent>(e)) {
        c->coverage = v;
    }
}
extern "C" float dse_volumetric_cloud_get_density(uint32_t e) {
    const auto* c = GCC<dse::VolumetricCloudComponent>(e);
    return c ? c->density : 0.0f;
}
extern "C" void dse_volumetric_cloud_set_density(uint32_t e, float v) {
    if (auto* c = GC<dse::VolumetricCloudComponent>(e)) {
        c->density = v;
    }
}
extern "C" float dse_volumetric_cloud_get_shape_scale(uint32_t e) {
    const auto* c = GCC<dse::VolumetricCloudComponent>(e);
    return c ? c->shape_scale : 0.0f;
}
extern "C" void dse_volumetric_cloud_set_shape_scale(uint32_t e, float v) {
    if (auto* c = GC<dse::VolumetricCloudComponent>(e)) {
        c->shape_scale = v;
    }
}
extern "C" float dse_volumetric_cloud_get_detail_scale(uint32_t e) {
    const auto* c = GCC<dse::VolumetricCloudComponent>(e);
    return c ? c->detail_scale : 0.0f;
}
extern "C" void dse_volumetric_cloud_set_detail_scale(uint32_t e, float v) {
    if (auto* c = GC<dse::VolumetricCloudComponent>(e)) {
        c->detail_scale = v;
    }
}
extern "C" float dse_volumetric_cloud_get_detail_strength(uint32_t e) {
    const auto* c = GCC<dse::VolumetricCloudComponent>(e);
    return c ? c->detail_strength : 0.0f;
}
extern "C" void dse_volumetric_cloud_set_detail_strength(uint32_t e, float v) {
    if (auto* c = GC<dse::VolumetricCloudComponent>(e)) {
        c->detail_strength = v;
    }
}
extern "C" float dse_volumetric_cloud_get_erosion(uint32_t e) {
    const auto* c = GCC<dse::VolumetricCloudComponent>(e);
    return c ? c->erosion : 0.0f;
}
extern "C" void dse_volumetric_cloud_set_erosion(uint32_t e, float v) {
    if (auto* c = GC<dse::VolumetricCloudComponent>(e)) {
        c->erosion = v;
    }
}
extern "C" void dse_volumetric_cloud_get_wind_direction(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::VolumetricCloudComponent>(e)) { *x = c->wind_direction.x; *y = c->wind_direction.y; *z = c->wind_direction.z; }
}
extern "C" void dse_volumetric_cloud_set_wind_direction(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::VolumetricCloudComponent>(e)) {
        c->wind_direction = glm::vec3(x, y, z);
    }
}
extern "C" float dse_volumetric_cloud_get_wind_speed(uint32_t e) {
    const auto* c = GCC<dse::VolumetricCloudComponent>(e);
    return c ? c->wind_speed : 0.0f;
}
extern "C" void dse_volumetric_cloud_set_wind_speed(uint32_t e, float v) {
    if (auto* c = GC<dse::VolumetricCloudComponent>(e)) {
        c->wind_speed = v;
    }
}
extern "C" float dse_volumetric_cloud_get_silver_intensity(uint32_t e) {
    const auto* c = GCC<dse::VolumetricCloudComponent>(e);
    return c ? c->silver_intensity : 0.0f;
}
extern "C" void dse_volumetric_cloud_set_silver_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::VolumetricCloudComponent>(e)) {
        c->silver_intensity = v;
    }
}
extern "C" float dse_volumetric_cloud_get_silver_spread(uint32_t e) {
    const auto* c = GCC<dse::VolumetricCloudComponent>(e);
    return c ? c->silver_spread : 0.0f;
}
extern "C" void dse_volumetric_cloud_set_silver_spread(uint32_t e, float v) {
    if (auto* c = GC<dse::VolumetricCloudComponent>(e)) {
        c->silver_spread = v;
    }
}
extern "C" float dse_volumetric_cloud_get_powder_strength(uint32_t e) {
    const auto* c = GCC<dse::VolumetricCloudComponent>(e);
    return c ? c->powder_strength : 0.0f;
}
extern "C" void dse_volumetric_cloud_set_powder_strength(uint32_t e, float v) {
    if (auto* c = GC<dse::VolumetricCloudComponent>(e)) {
        c->powder_strength = v;
    }
}
extern "C" float dse_volumetric_cloud_get_ambient_strength(uint32_t e) {
    const auto* c = GCC<dse::VolumetricCloudComponent>(e);
    return c ? c->ambient_strength : 0.0f;
}
extern "C" void dse_volumetric_cloud_set_ambient_strength(uint32_t e, float v) {
    if (auto* c = GC<dse::VolumetricCloudComponent>(e)) {
        c->ambient_strength = v;
    }
}
extern "C" int dse_volumetric_cloud_get_half_resolution(uint32_t e) {
    const auto* c = GCC<dse::VolumetricCloudComponent>(e);
    return (c && c->half_resolution) ? 1 : 0;
}
extern "C" void dse_volumetric_cloud_set_half_resolution(uint32_t e, int v) {
    if (auto* c = GC<dse::VolumetricCloudComponent>(e)) {
        c->half_resolution = (v != 0);
    }
}
extern "C" int dse_volumetric_cloud_get_temporal_reprojection(uint32_t e) {
    const auto* c = GCC<dse::VolumetricCloudComponent>(e);
    return (c && c->temporal_reprojection) ? 1 : 0;
}
extern "C" void dse_volumetric_cloud_set_temporal_reprojection(uint32_t e, int v) {
    if (auto* c = GC<dse::VolumetricCloudComponent>(e)) {
        c->temporal_reprojection = (v != 0);
    }
}
extern "C" int dse_volumetric_cloud_get_cloud_shadow_enabled(uint32_t e) {
    const auto* c = GCC<dse::VolumetricCloudComponent>(e);
    return (c && c->cloud_shadow_enabled) ? 1 : 0;
}
extern "C" void dse_volumetric_cloud_set_cloud_shadow_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::VolumetricCloudComponent>(e)) {
        c->cloud_shadow_enabled = (v != 0);
    }
}
