/**
 * @file dse_api_terrain_tile.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：TerrainTileManagerComponent）
 *
 * 组件字段 get/set 由 Codegen 生成（每组件一个 TU，与 Lua 拆分边界对齐）；
 * 手写 dse_api.cpp 仅保留 add/字符串/Input 等非字段 API。
 * 依赖 dse_api.h 提供的 dse_get_world_ptr() 访问已初始化的 World 指针。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_terrain_tile.h"
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
    return c ? static_cast<int>(c->tile_resolution) : 64;
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
    return c ? static_cast<int>(c->max_lod_levels) : 4;
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
