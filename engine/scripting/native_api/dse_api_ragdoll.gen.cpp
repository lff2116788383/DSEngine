/**
 * @file dse_api_ragdoll.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：RagdollComponent）
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

/* ---- RagdollComponent ---- */
extern "C" int dse_ragdoll_get_active(uint32_t e) {
    const auto* c = GCC<dse::RagdollComponent>(e);
    return (c && c->active) ? 1 : 0;
}
extern "C" void dse_ragdoll_set_active(uint32_t e, int v) {
    if (auto* c = GC<dse::RagdollComponent>(e)) {
        c->active = (v != 0);
    }
}
extern "C" int dse_ragdoll_get_auto_setup(uint32_t e) {
    const auto* c = GCC<dse::RagdollComponent>(e);
    return (c && c->auto_setup) ? 1 : 0;
}
extern "C" void dse_ragdoll_set_auto_setup(uint32_t e, int v) {
    if (auto* c = GC<dse::RagdollComponent>(e)) {
        c->auto_setup = (v != 0);
    }
}
extern "C" float dse_ragdoll_get_total_mass(uint32_t e) {
    const auto* c = GCC<dse::RagdollComponent>(e);
    return c ? c->total_mass : 0.0f;
}
extern "C" void dse_ragdoll_set_total_mass(uint32_t e, float v) {
    if (auto* c = GC<dse::RagdollComponent>(e)) {
        c->total_mass = v;
    }
}
extern "C" float dse_ragdoll_get_joint_stiffness(uint32_t e) {
    const auto* c = GCC<dse::RagdollComponent>(e);
    return c ? c->joint_stiffness : 0.0f;
}
extern "C" void dse_ragdoll_set_joint_stiffness(uint32_t e, float v) {
    if (auto* c = GC<dse::RagdollComponent>(e)) {
        c->joint_stiffness = v;
    }
}
extern "C" float dse_ragdoll_get_joint_damping(uint32_t e) {
    const auto* c = GCC<dse::RagdollComponent>(e);
    return c ? c->joint_damping : 0.0f;
}
extern "C" void dse_ragdoll_set_joint_damping(uint32_t e, float v) {
    if (auto* c = GC<dse::RagdollComponent>(e)) {
        c->joint_damping = v;
    }
}
extern "C" int dse_ragdoll_get_collision_layer(uint32_t e) {
    const auto* c = GCC<dse::RagdollComponent>(e);
    return c ? static_cast<int>(c->collision_layer) : 0;
}
extern "C" void dse_ragdoll_set_collision_layer(uint32_t e, int v) {
    if (auto* c = GC<dse::RagdollComponent>(e)) {
        c->collision_layer = v;
    }
}
extern "C" int dse_ragdoll_get_collision_mask(uint32_t e) {
    const auto* c = GCC<dse::RagdollComponent>(e);
    return c ? static_cast<int>(c->collision_mask) : 0;
}
extern "C" void dse_ragdoll_set_collision_mask(uint32_t e, int v) {
    if (auto* c = GC<dse::RagdollComponent>(e)) {
        c->collision_mask = v;
    }
}
