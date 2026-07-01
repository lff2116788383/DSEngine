/**
 * @file dse_api_rope.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：RopeComponent）
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

/* ---- RopeComponent ---- */
extern "C" int dse_rope_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::RopeComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_rope_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::RopeComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" int dse_rope_get_segment_count(uint32_t e) {
    const auto* c = GCC<dse::RopeComponent>(e);
    return c ? static_cast<int>(c->segment_count) : 0;
}
extern "C" void dse_rope_set_segment_count(uint32_t e, int v) {
    if (auto* c = GC<dse::RopeComponent>(e)) {
        c->segment_count = v;
    }
}
extern "C" float dse_rope_get_segment_length(uint32_t e) {
    const auto* c = GCC<dse::RopeComponent>(e);
    return c ? c->segment_length : 0.0f;
}
extern "C" void dse_rope_set_segment_length(uint32_t e, float v) {
    if (auto* c = GC<dse::RopeComponent>(e)) {
        c->segment_length = v;
    }
}
extern "C" float dse_rope_get_radius(uint32_t e) {
    const auto* c = GCC<dse::RopeComponent>(e);
    return c ? c->radius : 0.0f;
}
extern "C" void dse_rope_set_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::RopeComponent>(e)) {
        c->radius = v;
    }
}
extern "C" float dse_rope_get_damping(uint32_t e) {
    const auto* c = GCC<dse::RopeComponent>(e);
    return c ? c->damping : 0.0f;
}
extern "C" void dse_rope_set_damping(uint32_t e, float v) {
    if (auto* c = GC<dse::RopeComponent>(e)) {
        c->damping = v;
    }
}
extern "C" int dse_rope_get_solver_iterations(uint32_t e) {
    const auto* c = GCC<dse::RopeComponent>(e);
    return c ? static_cast<int>(c->solver_iterations) : 0;
}
extern "C" void dse_rope_set_solver_iterations(uint32_t e, int v) {
    if (auto* c = GC<dse::RopeComponent>(e)) {
        c->solver_iterations = v;
    }
}
extern "C" int dse_rope_get_use_gravity(uint32_t e) {
    const auto* c = GCC<dse::RopeComponent>(e);
    return (c && c->use_gravity) ? 1 : 0;
}
extern "C" void dse_rope_set_use_gravity(uint32_t e, int v) {
    if (auto* c = GC<dse::RopeComponent>(e)) {
        c->use_gravity = (v != 0);
    }
}
extern "C" float dse_rope_get_gravity_scale(uint32_t e) {
    const auto* c = GCC<dse::RopeComponent>(e);
    return c ? c->gravity_scale : 0.0f;
}
extern "C" void dse_rope_set_gravity_scale(uint32_t e, float v) {
    if (auto* c = GC<dse::RopeComponent>(e)) {
        c->gravity_scale = v;
    }
}
extern "C" int dse_rope_get_anchor_entity_a(uint32_t e) {
    const auto* c = GCC<dse::RopeComponent>(e);
    return c ? static_cast<int>(c->anchor_entity_a) : 0;
}
extern "C" void dse_rope_set_anchor_entity_a(uint32_t e, int v) {
    if (auto* c = GC<dse::RopeComponent>(e)) {
        c->anchor_entity_a = v;
    }
}
extern "C" int dse_rope_get_anchor_entity_b(uint32_t e) {
    const auto* c = GCC<dse::RopeComponent>(e);
    return c ? static_cast<int>(c->anchor_entity_b) : 0;
}
extern "C" void dse_rope_set_anchor_entity_b(uint32_t e, int v) {
    if (auto* c = GC<dse::RopeComponent>(e)) {
        c->anchor_entity_b = v;
    }
}
extern "C" void dse_rope_get_anchor_offset_a(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::RopeComponent>(e)) { *x = c->anchor_offset_a.x; *y = c->anchor_offset_a.y; *z = c->anchor_offset_a.z; }
}
extern "C" void dse_rope_set_anchor_offset_a(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::RopeComponent>(e)) {
        c->anchor_offset_a = glm::vec3(x, y, z);
    }
}
extern "C" void dse_rope_get_anchor_offset_b(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::RopeComponent>(e)) { *x = c->anchor_offset_b.x; *y = c->anchor_offset_b.y; *z = c->anchor_offset_b.z; }
}
extern "C" void dse_rope_set_anchor_offset_b(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::RopeComponent>(e)) {
        c->anchor_offset_b = glm::vec3(x, y, z);
    }
}
extern "C" void dse_rope_get_start_position(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::RopeComponent>(e)) { *x = c->start_position.x; *y = c->start_position.y; *z = c->start_position.z; }
}
extern "C" void dse_rope_set_start_position(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::RopeComponent>(e)) {
        c->start_position = glm::vec3(x, y, z);
    }
}
