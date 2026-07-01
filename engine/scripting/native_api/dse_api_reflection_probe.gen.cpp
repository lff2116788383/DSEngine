/**
 * @file dse_api_reflection_probe.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：ReflectionProbeComponent）
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

/* ---- ReflectionProbeComponent ---- */
extern "C" int dse_reflection_probe_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::ReflectionProbeComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_reflection_probe_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::ReflectionProbeComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" float dse_reflection_probe_get_influence_radius(uint32_t e) {
    const auto* c = GCC<dse::ReflectionProbeComponent>(e);
    return c ? c->influence_radius : 0.0f;
}
extern "C" void dse_reflection_probe_set_influence_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::ReflectionProbeComponent>(e)) {
        c->influence_radius = v;
    }
}
extern "C" float dse_reflection_probe_get_box_size_x(uint32_t e) {
    const auto* c = GCC<dse::ReflectionProbeComponent>(e);
    return c ? c->box_size_x : 0.0f;
}
extern "C" void dse_reflection_probe_set_box_size_x(uint32_t e, float v) {
    if (auto* c = GC<dse::ReflectionProbeComponent>(e)) {
        c->box_size_x = v;
    }
}
extern "C" float dse_reflection_probe_get_box_size_y(uint32_t e) {
    const auto* c = GCC<dse::ReflectionProbeComponent>(e);
    return c ? c->box_size_y : 0.0f;
}
extern "C" void dse_reflection_probe_set_box_size_y(uint32_t e, float v) {
    if (auto* c = GC<dse::ReflectionProbeComponent>(e)) {
        c->box_size_y = v;
    }
}
extern "C" float dse_reflection_probe_get_box_size_z(uint32_t e) {
    const auto* c = GCC<dse::ReflectionProbeComponent>(e);
    return c ? c->box_size_z : 0.0f;
}
extern "C" void dse_reflection_probe_set_box_size_z(uint32_t e, float v) {
    if (auto* c = GC<dse::ReflectionProbeComponent>(e)) {
        c->box_size_z = v;
    }
}
extern "C" int dse_reflection_probe_get_use_box_projection(uint32_t e) {
    const auto* c = GCC<dse::ReflectionProbeComponent>(e);
    return (c && c->use_box_projection) ? 1 : 0;
}
extern "C" void dse_reflection_probe_set_use_box_projection(uint32_t e, int v) {
    if (auto* c = GC<dse::ReflectionProbeComponent>(e)) {
        c->use_box_projection = (v != 0);
    }
}
extern "C" int dse_reflection_probe_get_resolution(uint32_t e) {
    const auto* c = GCC<dse::ReflectionProbeComponent>(e);
    return c ? static_cast<int>(c->resolution) : 0;
}
extern "C" void dse_reflection_probe_set_resolution(uint32_t e, int v) {
    if (auto* c = GC<dse::ReflectionProbeComponent>(e)) {
        c->resolution = v;
    }
}
extern "C" int dse_reflection_probe_get_show_debug(uint32_t e) {
    const auto* c = GCC<dse::ReflectionProbeComponent>(e);
    return (c && c->show_debug) ? 1 : 0;
}
extern "C" void dse_reflection_probe_set_show_debug(uint32_t e, int v) {
    if (auto* c = GC<dse::ReflectionProbeComponent>(e)) {
        c->show_debug = (v != 0);
    }
}
