/**
 * @file dse_api_navmesh_rebake.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：NavMeshAutoRebakeComponent）
 *
 * 组件字段 get/set 由 Codegen 生成（每组件一个 TU，与 Lua 拆分边界对齐）；
 * 手写 dse_api.cpp 仅保留 add/字符串/Input 等非字段 API。
 * 依赖 dse_api.h 提供的 dse_get_world_ptr() 访问已初始化的 World 指针。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_navmesh.h"
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
