/**
 * @file dse_api_rigidbody3d.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：RigidBody3DComponent）
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

/* ---- RigidBody3DComponent ---- */
extern "C" int dse_rigidbody3d_get_type(uint32_t e) {
    const auto* c = GCC<dse::RigidBody3DComponent>(e);
    return c ? static_cast<int>(c->type) : 0;
}
extern "C" void dse_rigidbody3d_set_type(uint32_t e, int v) {
    if (auto* c = GC<dse::RigidBody3DComponent>(e)) {
        c->type = v;
    }
}
extern "C" float dse_rigidbody3d_get_mass(uint32_t e) {
    const auto* c = GCC<dse::RigidBody3DComponent>(e);
    return c ? c->mass : 0.0f;
}
extern "C" void dse_rigidbody3d_set_mass(uint32_t e, float v) {
    if (auto* c = GC<dse::RigidBody3DComponent>(e)) {
        c->mass = v;
    }
}
extern "C" float dse_rigidbody3d_get_drag(uint32_t e) {
    const auto* c = GCC<dse::RigidBody3DComponent>(e);
    return c ? c->drag : 0.0f;
}
extern "C" void dse_rigidbody3d_set_drag(uint32_t e, float v) {
    if (auto* c = GC<dse::RigidBody3DComponent>(e)) {
        c->drag = v;
    }
}
extern "C" float dse_rigidbody3d_get_angular_drag(uint32_t e) {
    const auto* c = GCC<dse::RigidBody3DComponent>(e);
    return c ? c->angular_drag : 0.0f;
}
extern "C" void dse_rigidbody3d_set_angular_drag(uint32_t e, float v) {
    if (auto* c = GC<dse::RigidBody3DComponent>(e)) {
        c->angular_drag = v;
    }
}
extern "C" int dse_rigidbody3d_get_use_gravity(uint32_t e) {
    const auto* c = GCC<dse::RigidBody3DComponent>(e);
    return (c && c->use_gravity) ? 1 : 0;
}
extern "C" void dse_rigidbody3d_set_use_gravity(uint32_t e, int v) {
    if (auto* c = GC<dse::RigidBody3DComponent>(e)) {
        c->use_gravity = (v != 0);
    }
}
extern "C" float dse_rigidbody3d_get_gravity_scale(uint32_t e) {
    const auto* c = GCC<dse::RigidBody3DComponent>(e);
    return c ? c->gravity_scale : 0.0f;
}
extern "C" void dse_rigidbody3d_set_gravity_scale(uint32_t e, float v) {
    if (auto* c = GC<dse::RigidBody3DComponent>(e)) {
        c->gravity_scale = v;
    }
}
extern "C" int dse_rigidbody3d_get_is_kinematic(uint32_t e) {
    const auto* c = GCC<dse::RigidBody3DComponent>(e);
    return (c && c->is_kinematic) ? 1 : 0;
}
extern "C" void dse_rigidbody3d_set_is_kinematic(uint32_t e, int v) {
    if (auto* c = GC<dse::RigidBody3DComponent>(e)) {
        c->is_kinematic = (v != 0);
    }
}
extern "C" int dse_rigidbody3d_get_collision_layer(uint32_t e) {
    const auto* c = GCC<dse::RigidBody3DComponent>(e);
    return c ? static_cast<int>(c->collision_layer) : 0;
}
extern "C" void dse_rigidbody3d_set_collision_layer(uint32_t e, int v) {
    if (auto* c = GC<dse::RigidBody3DComponent>(e)) {
        c->collision_layer = v;
    }
}
extern "C" int dse_rigidbody3d_get_collision_mask(uint32_t e) {
    const auto* c = GCC<dse::RigidBody3DComponent>(e);
    return c ? static_cast<int>(c->collision_mask) : 0;
}
extern "C" void dse_rigidbody3d_set_collision_mask(uint32_t e, int v) {
    if (auto* c = GC<dse::RigidBody3DComponent>(e)) {
        c->collision_mask = v;
    }
}
