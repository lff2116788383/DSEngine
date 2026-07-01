/**
 * @file dse_api_sphere_collider3d.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：SphereCollider3DComponent）
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

/* ---- SphereCollider3DComponent ---- */
extern "C" float dse_sphere_collider3d_get_radius(uint32_t e) {
    const auto* c = GCC<dse::SphereCollider3DComponent>(e);
    return c ? c->radius : 0.0f;
}
extern "C" void dse_sphere_collider3d_set_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::SphereCollider3DComponent>(e)) {
        c->radius = v;
    }
}
extern "C" void dse_sphere_collider3d_get_center(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::SphereCollider3DComponent>(e)) { *x = c->center.x; *y = c->center.y; *z = c->center.z; }
}
extern "C" void dse_sphere_collider3d_set_center(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::SphereCollider3DComponent>(e)) {
        c->center = glm::vec3(x, y, z);
    }
}
extern "C" int dse_sphere_collider3d_get_is_trigger(uint32_t e) {
    const auto* c = GCC<dse::SphereCollider3DComponent>(e);
    return (c && c->is_trigger) ? 1 : 0;
}
extern "C" void dse_sphere_collider3d_set_is_trigger(uint32_t e, int v) {
    if (auto* c = GC<dse::SphereCollider3DComponent>(e)) {
        c->is_trigger = (v != 0);
    }
}
extern "C" float dse_sphere_collider3d_get_bounciness(uint32_t e) {
    const auto* c = GCC<dse::SphereCollider3DComponent>(e);
    return c ? c->bounciness : 0.0f;
}
extern "C" void dse_sphere_collider3d_set_bounciness(uint32_t e, float v) {
    if (auto* c = GC<dse::SphereCollider3DComponent>(e)) {
        c->bounciness = v;
    }
}
extern "C" float dse_sphere_collider3d_get_friction(uint32_t e) {
    const auto* c = GCC<dse::SphereCollider3DComponent>(e);
    return c ? c->friction : 0.0f;
}
extern "C" void dse_sphere_collider3d_set_friction(uint32_t e, float v) {
    if (auto* c = GC<dse::SphereCollider3DComponent>(e)) {
        c->friction = v;
    }
}
