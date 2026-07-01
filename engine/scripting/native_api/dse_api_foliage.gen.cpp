/**
 * @file dse_api_foliage.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：FoliageComponent）
 *
 * 组件字段 get/set 由 Codegen 生成（每组件一个 TU，与 Lua 拆分边界对齐）；
 * 手写 dse_api.cpp 仅保留 add/字符串/Input 等非字段 API。
 * 依赖 dse_api.h 提供的 dse_get_world_ptr() 访问已初始化的 World 指针。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_foliage.h"
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

/* ---- FoliageComponent ---- */
extern "C" int dse_foliage_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::FoliageComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_foliage_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::FoliageComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" float dse_foliage_get_wind_strength(uint32_t e) {
    const auto* c = GCC<dse::FoliageComponent>(e);
    return c ? c->wind_strength : 0.0f;
}
extern "C" void dse_foliage_set_wind_strength(uint32_t e, float v) {
    if (auto* c = GC<dse::FoliageComponent>(e)) {
        c->wind_strength = v;
    }
}
extern "C" float dse_foliage_get_stiffness(uint32_t e) {
    const auto* c = GCC<dse::FoliageComponent>(e);
    return c ? c->stiffness : 0.0f;
}
extern "C" void dse_foliage_set_stiffness(uint32_t e, float v) {
    if (auto* c = GC<dse::FoliageComponent>(e)) {
        c->stiffness = v;
    }
}
extern "C" float dse_foliage_get_phase_offset(uint32_t e) {
    const auto* c = GCC<dse::FoliageComponent>(e);
    return c ? c->phase_offset : 0.0f;
}
extern "C" void dse_foliage_set_phase_offset(uint32_t e, float v) {
    if (auto* c = GC<dse::FoliageComponent>(e)) {
        c->phase_offset = v;
    }
}
extern "C" float dse_foliage_get_push_response(uint32_t e) {
    const auto* c = GCC<dse::FoliageComponent>(e);
    return c ? c->push_response : 0.0f;
}
extern "C" void dse_foliage_set_push_response(uint32_t e, float v) {
    if (auto* c = GC<dse::FoliageComponent>(e)) {
        c->push_response = v;
    }
}
