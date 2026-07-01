/**
 * @file dse_api_buoyancy.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：BuoyancyComponent）
 *
 * 组件字段 get/set 由 Codegen 生成（每组件一个 TU，与 Lua 拆分边界对齐）；
 * 手写 dse_api.cpp 仅保留 add/字符串/Input 等非字段 API。
 * 依赖 dse_api.h 提供的 dse_get_world_ptr() 访问已初始化的 World 指针。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_physics.h"
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

/* ---- BuoyancyComponent ---- */
extern "C" int dse_buoyancy_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::BuoyancyComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_buoyancy_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::BuoyancyComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" float dse_buoyancy_get_water_level(uint32_t e) {
    const auto* c = GCC<dse::BuoyancyComponent>(e);
    return c ? c->water_level : 0.0f;
}
extern "C" void dse_buoyancy_set_water_level(uint32_t e, float v) {
    if (auto* c = GC<dse::BuoyancyComponent>(e)) {
        c->water_level = v;
    }
}
extern "C" int dse_buoyancy_get_use_fluid_system(uint32_t e) {
    const auto* c = GCC<dse::BuoyancyComponent>(e);
    return (c && c->use_fluid_system) ? 1 : 0;
}
extern "C" void dse_buoyancy_set_use_fluid_system(uint32_t e, int v) {
    if (auto* c = GC<dse::BuoyancyComponent>(e)) {
        c->use_fluid_system = (v != 0);
    }
}
extern "C" float dse_buoyancy_get_buoyancy_force(uint32_t e) {
    const auto* c = GCC<dse::BuoyancyComponent>(e);
    return c ? c->buoyancy_force : 0.0f;
}
extern "C" void dse_buoyancy_set_buoyancy_force(uint32_t e, float v) {
    if (auto* c = GC<dse::BuoyancyComponent>(e)) {
        c->buoyancy_force = v;
    }
}
extern "C" float dse_buoyancy_get_water_drag(uint32_t e) {
    const auto* c = GCC<dse::BuoyancyComponent>(e);
    return c ? c->water_drag : 0.0f;
}
extern "C" void dse_buoyancy_set_water_drag(uint32_t e, float v) {
    if (auto* c = GC<dse::BuoyancyComponent>(e)) {
        c->water_drag = v;
    }
}
extern "C" float dse_buoyancy_get_water_angular_drag(uint32_t e) {
    const auto* c = GCC<dse::BuoyancyComponent>(e);
    return c ? c->water_angular_drag : 0.0f;
}
extern "C" void dse_buoyancy_set_water_angular_drag(uint32_t e, float v) {
    if (auto* c = GC<dse::BuoyancyComponent>(e)) {
        c->water_angular_drag = v;
    }
}
extern "C" float dse_buoyancy_get_submerge_depth(uint32_t e) {
    const auto* c = GCC<dse::BuoyancyComponent>(e);
    return c ? c->submerge_depth : 0.0f;
}
extern "C" void dse_buoyancy_set_submerge_depth(uint32_t e, float v) {
    if (auto* c = GC<dse::BuoyancyComponent>(e)) {
        c->submerge_depth = v;
    }
}
