/**
 * @file dse_api_joint3d.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：Joint3DComponent）
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

/* ---- Joint3DComponent ---- */
extern "C" int dse_joint3d_get_type(uint32_t e) {
    const auto* c = GCC<dse::Joint3DComponent>(e);
    return c ? static_cast<int>(c->type) : 0;
}
extern "C" void dse_joint3d_set_type(uint32_t e, int v) {
    if (auto* c = GC<dse::Joint3DComponent>(e)) {
        c->type = v;
    }
}
extern "C" int dse_joint3d_get_connected_entity_id(uint32_t e) {
    const auto* c = GCC<dse::Joint3DComponent>(e);
    return c ? static_cast<int>(c->connected_entity_id) : 0;
}
extern "C" void dse_joint3d_set_connected_entity_id(uint32_t e, int v) {
    if (auto* c = GC<dse::Joint3DComponent>(e)) {
        c->connected_entity_id = v;
    }
}
extern "C" void dse_joint3d_get_anchor(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::Joint3DComponent>(e)) { *x = c->anchor.x; *y = c->anchor.y; *z = c->anchor.z; }
}
extern "C" void dse_joint3d_set_anchor(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::Joint3DComponent>(e)) {
        c->anchor = glm::vec3(x, y, z);
    }
}
extern "C" void dse_joint3d_get_connected_anchor(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::Joint3DComponent>(e)) { *x = c->connected_anchor.x; *y = c->connected_anchor.y; *z = c->connected_anchor.z; }
}
extern "C" void dse_joint3d_set_connected_anchor(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::Joint3DComponent>(e)) {
        c->connected_anchor = glm::vec3(x, y, z);
    }
}
extern "C" void dse_joint3d_get_axis(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::Joint3DComponent>(e)) { *x = c->axis.x; *y = c->axis.y; *z = c->axis.z; }
}
extern "C" void dse_joint3d_set_axis(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::Joint3DComponent>(e)) {
        c->axis = glm::vec3(x, y, z);
    }
}
extern "C" int dse_joint3d_get_use_limits(uint32_t e) {
    const auto* c = GCC<dse::Joint3DComponent>(e);
    return (c && c->use_limits) ? 1 : 0;
}
extern "C" void dse_joint3d_set_use_limits(uint32_t e, int v) {
    if (auto* c = GC<dse::Joint3DComponent>(e)) {
        c->use_limits = (v != 0);
    }
}
extern "C" float dse_joint3d_get_lower_limit(uint32_t e) {
    const auto* c = GCC<dse::Joint3DComponent>(e);
    return c ? c->lower_limit : 0.0f;
}
extern "C" void dse_joint3d_set_lower_limit(uint32_t e, float v) {
    if (auto* c = GC<dse::Joint3DComponent>(e)) {
        c->lower_limit = v;
    }
}
extern "C" float dse_joint3d_get_upper_limit(uint32_t e) {
    const auto* c = GCC<dse::Joint3DComponent>(e);
    return c ? c->upper_limit : 0.0f;
}
extern "C" void dse_joint3d_set_upper_limit(uint32_t e, float v) {
    if (auto* c = GC<dse::Joint3DComponent>(e)) {
        c->upper_limit = v;
    }
}
extern "C" float dse_joint3d_get_min_distance(uint32_t e) {
    const auto* c = GCC<dse::Joint3DComponent>(e);
    return c ? c->min_distance : 0.0f;
}
extern "C" void dse_joint3d_set_min_distance(uint32_t e, float v) {
    if (auto* c = GC<dse::Joint3DComponent>(e)) {
        c->min_distance = v;
    }
}
extern "C" float dse_joint3d_get_max_distance(uint32_t e) {
    const auto* c = GCC<dse::Joint3DComponent>(e);
    return c ? c->max_distance : 0.0f;
}
extern "C" void dse_joint3d_set_max_distance(uint32_t e, float v) {
    if (auto* c = GC<dse::Joint3DComponent>(e)) {
        c->max_distance = v;
    }
}
extern "C" float dse_joint3d_get_spring_stiffness(uint32_t e) {
    const auto* c = GCC<dse::Joint3DComponent>(e);
    return c ? c->spring_stiffness : 0.0f;
}
extern "C" void dse_joint3d_set_spring_stiffness(uint32_t e, float v) {
    if (auto* c = GC<dse::Joint3DComponent>(e)) {
        c->spring_stiffness = v;
    }
}
extern "C" float dse_joint3d_get_spring_damping(uint32_t e) {
    const auto* c = GCC<dse::Joint3DComponent>(e);
    return c ? c->spring_damping : 0.0f;
}
extern "C" void dse_joint3d_set_spring_damping(uint32_t e, float v) {
    if (auto* c = GC<dse::Joint3DComponent>(e)) {
        c->spring_damping = v;
    }
}
extern "C" float dse_joint3d_get_break_force(uint32_t e) {
    const auto* c = GCC<dse::Joint3DComponent>(e);
    return c ? c->break_force : 0.0f;
}
extern "C" void dse_joint3d_set_break_force(uint32_t e, float v) {
    if (auto* c = GC<dse::Joint3DComponent>(e)) {
        c->break_force = v;
    }
}
extern "C" float dse_joint3d_get_break_torque(uint32_t e) {
    const auto* c = GCC<dse::Joint3DComponent>(e);
    return c ? c->break_torque : 0.0f;
}
extern "C" void dse_joint3d_set_break_torque(uint32_t e, float v) {
    if (auto* c = GC<dse::Joint3DComponent>(e)) {
        c->break_torque = v;
    }
}
