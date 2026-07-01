/**
 * @file dse_api_gi_probe.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：GIProbeVolumeComponent）
 *
 * 组件字段 get/set 由 Codegen 生成（每组件一个 TU，与 Lua 拆分边界对齐）；
 * 手写 dse_api.cpp 仅保留 add/字符串/Input 等非字段 API。
 * 依赖 dse_api.h 提供的 dse_get_world_ptr() 访问已初始化的 World 指针。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_render.h"
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

/* ---- GIProbeVolumeComponent ---- */
extern "C" int dse_gi_probe_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::GIProbeVolumeComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_gi_probe_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::GIProbeVolumeComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" void dse_gi_probe_get_origin(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::GIProbeVolumeComponent>(e)) { *x = c->origin.x; *y = c->origin.y; *z = c->origin.z; }
}
extern "C" void dse_gi_probe_set_origin(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::GIProbeVolumeComponent>(e)) {
        c->origin = glm::vec3(x, y, z);
    }
}
extern "C" void dse_gi_probe_get_extent(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::GIProbeVolumeComponent>(e)) { *x = c->extent.x; *y = c->extent.y; *z = c->extent.z; }
}
extern "C" void dse_gi_probe_set_extent(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::GIProbeVolumeComponent>(e)) {
        c->extent = glm::vec3(x, y, z);
    }
}
extern "C" int dse_gi_probe_get_resolution_x(uint32_t e) {
    const auto* c = GCC<dse::GIProbeVolumeComponent>(e);
    return c ? static_cast<int>(c->resolution_x) : 0;
}
extern "C" void dse_gi_probe_set_resolution_x(uint32_t e, int v) {
    if (auto* c = GC<dse::GIProbeVolumeComponent>(e)) {
        c->resolution_x = v;
    }
}
extern "C" int dse_gi_probe_get_resolution_y(uint32_t e) {
    const auto* c = GCC<dse::GIProbeVolumeComponent>(e);
    return c ? static_cast<int>(c->resolution_y) : 0;
}
extern "C" void dse_gi_probe_set_resolution_y(uint32_t e, int v) {
    if (auto* c = GC<dse::GIProbeVolumeComponent>(e)) {
        c->resolution_y = v;
    }
}
extern "C" int dse_gi_probe_get_resolution_z(uint32_t e) {
    const auto* c = GCC<dse::GIProbeVolumeComponent>(e);
    return c ? static_cast<int>(c->resolution_z) : 0;
}
extern "C" void dse_gi_probe_set_resolution_z(uint32_t e, int v) {
    if (auto* c = GC<dse::GIProbeVolumeComponent>(e)) {
        c->resolution_z = v;
    }
}
extern "C" int dse_gi_probe_get_irradiance_texels(uint32_t e) {
    const auto* c = GCC<dse::GIProbeVolumeComponent>(e);
    return c ? static_cast<int>(c->irradiance_texels) : 0;
}
extern "C" void dse_gi_probe_set_irradiance_texels(uint32_t e, int v) {
    if (auto* c = GC<dse::GIProbeVolumeComponent>(e)) {
        c->irradiance_texels = v;
    }
}
extern "C" int dse_gi_probe_get_visibility_texels(uint32_t e) {
    const auto* c = GCC<dse::GIProbeVolumeComponent>(e);
    return c ? static_cast<int>(c->visibility_texels) : 0;
}
extern "C" void dse_gi_probe_set_visibility_texels(uint32_t e, int v) {
    if (auto* c = GC<dse::GIProbeVolumeComponent>(e)) {
        c->visibility_texels = v;
    }
}
extern "C" int dse_gi_probe_get_rays_per_probe(uint32_t e) {
    const auto* c = GCC<dse::GIProbeVolumeComponent>(e);
    return c ? static_cast<int>(c->rays_per_probe) : 0;
}
extern "C" void dse_gi_probe_set_rays_per_probe(uint32_t e, int v) {
    if (auto* c = GC<dse::GIProbeVolumeComponent>(e)) {
        c->rays_per_probe = v;
    }
}
extern "C" float dse_gi_probe_get_hysteresis(uint32_t e) {
    const auto* c = GCC<dse::GIProbeVolumeComponent>(e);
    return c ? c->hysteresis : 0.0f;
}
extern "C" void dse_gi_probe_set_hysteresis(uint32_t e, float v) {
    if (auto* c = GC<dse::GIProbeVolumeComponent>(e)) {
        c->hysteresis = v;
    }
}
extern "C" float dse_gi_probe_get_gi_intensity(uint32_t e) {
    const auto* c = GCC<dse::GIProbeVolumeComponent>(e);
    return c ? c->gi_intensity : 0.0f;
}
extern "C" void dse_gi_probe_set_gi_intensity(uint32_t e, float v) {
    if (auto* c = GC<dse::GIProbeVolumeComponent>(e)) {
        c->gi_intensity = v;
    }
}
extern "C" float dse_gi_probe_get_normal_bias(uint32_t e) {
    const auto* c = GCC<dse::GIProbeVolumeComponent>(e);
    return c ? c->normal_bias : 0.0f;
}
extern "C" void dse_gi_probe_set_normal_bias(uint32_t e, float v) {
    if (auto* c = GC<dse::GIProbeVolumeComponent>(e)) {
        c->normal_bias = v;
    }
}
extern "C" float dse_gi_probe_get_view_bias(uint32_t e) {
    const auto* c = GCC<dse::GIProbeVolumeComponent>(e);
    return c ? c->view_bias : 0.0f;
}
extern "C" void dse_gi_probe_set_view_bias(uint32_t e, float v) {
    if (auto* c = GC<dse::GIProbeVolumeComponent>(e)) {
        c->view_bias = v;
    }
}
extern "C" int dse_gi_probe_get_show_debug_probes(uint32_t e) {
    const auto* c = GCC<dse::GIProbeVolumeComponent>(e);
    return (c && c->show_debug_probes) ? 1 : 0;
}
extern "C" void dse_gi_probe_set_show_debug_probes(uint32_t e, int v) {
    if (auto* c = GC<dse::GIProbeVolumeComponent>(e)) {
        c->show_debug_probes = (v != 0);
    }
}
