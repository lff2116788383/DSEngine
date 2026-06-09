/**
 * @file dse_api_tree.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：TreeComponent）
 *
 * 组件字段 get/set 由 Codegen 生成（每组件一个 TU，与 Lua 拆分边界对齐）；
 * 手写 dse_api.cpp 仅保留 add/字符串/Input 等非字段 API。
 * 依赖 dse_api.h 提供的 dse_get_world_ptr() 访问已初始化的 World 指针。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_tree.h"
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
    return c ? static_cast<int>(c->seed) : 12345;
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
extern "C" void dse_tree_set_mesh_path(uint32_t e, const char* v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->mesh_path = v ? v : "";
    }
}
extern "C" int dse_tree_get_mesh_path(uint32_t e, char* buf, int buf_size) {
    if (!buf || buf_size <= 0) return 0;
    buf[0] = '\0';
    const auto* c = GCC<dse::TreeComponent>(e);
    if (!c || c->mesh_path.empty()) return 0;
    std::strncpy(buf, c->mesh_path.c_str(), static_cast<std::size_t>(buf_size - 1));
    buf[buf_size - 1] = '\0';
    return static_cast<int>(std::strlen(buf));
}
extern "C" void dse_tree_set_lod1_mesh_path(uint32_t e, const char* v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->lod1_mesh_path = v ? v : "";
    }
}
extern "C" int dse_tree_get_lod1_mesh_path(uint32_t e, char* buf, int buf_size) {
    if (!buf || buf_size <= 0) return 0;
    buf[0] = '\0';
    const auto* c = GCC<dse::TreeComponent>(e);
    if (!c || c->lod1_mesh_path.empty()) return 0;
    std::strncpy(buf, c->lod1_mesh_path.c_str(), static_cast<std::size_t>(buf_size - 1));
    buf[buf_size - 1] = '\0';
    return static_cast<int>(std::strlen(buf));
}
extern "C" void dse_tree_set_billboard_texture_path(uint32_t e, const char* v) {
    if (auto* c = GC<dse::TreeComponent>(e)) {
        c->billboard_texture_path = v ? v : "";
    }
}
extern "C" int dse_tree_get_billboard_texture_path(uint32_t e, char* buf, int buf_size) {
    if (!buf || buf_size <= 0) return 0;
    buf[0] = '\0';
    const auto* c = GCC<dse::TreeComponent>(e);
    if (!c || c->billboard_texture_path.empty()) return 0;
    std::strncpy(buf, c->billboard_texture_path.c_str(), static_cast<std::size_t>(buf_size - 1));
    buf[buf_size - 1] = '\0';
    return static_cast<int>(std::strlen(buf));
}
