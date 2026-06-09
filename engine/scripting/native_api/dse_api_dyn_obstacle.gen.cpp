/**
 * @file dse_api_dyn_obstacle.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：DynamicObstacleComponent）
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

/* ---- DynamicObstacleComponent ---- */
extern "C" int dse_dyn_obstacle_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::DynamicObstacleComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_dyn_obstacle_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::DynamicObstacleComponent>(e)) {
        c->enabled = (v != 0);
        c->dirty_ = true;
    }
}
extern "C" int dse_dyn_obstacle_get_shape(uint32_t e) {
    const auto* c = GCC<dse::DynamicObstacleComponent>(e);
    return c ? static_cast<int>(c->shape) : 0;
}
extern "C" void dse_dyn_obstacle_set_shape(uint32_t e, int v) {
    if (auto* c = GC<dse::DynamicObstacleComponent>(e)) {
        c->shape = static_cast<dse::DynamicObstacleComponent::Shape>(v);
        c->dirty_ = true;
    }
}
extern "C" void dse_dyn_obstacle_get_box_extents(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::DynamicObstacleComponent>(e)) { *x = c->box_extents.x; *y = c->box_extents.y; *z = c->box_extents.z; }
}
extern "C" void dse_dyn_obstacle_set_box_extents(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::DynamicObstacleComponent>(e)) {
        c->box_extents = glm::vec3(x, y, z);
        c->dirty_ = true;
    }
}
extern "C" float dse_dyn_obstacle_get_cylinder_radius(uint32_t e) {
    const auto* c = GCC<dse::DynamicObstacleComponent>(e);
    return c ? c->cylinder_radius : 1.0f;
}
extern "C" void dse_dyn_obstacle_set_cylinder_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::DynamicObstacleComponent>(e)) {
        c->cylinder_radius = v;
        c->dirty_ = true;
    }
}
extern "C" float dse_dyn_obstacle_get_cylinder_height(uint32_t e) {
    const auto* c = GCC<dse::DynamicObstacleComponent>(e);
    return c ? c->cylinder_height : 2.0f;
}
extern "C" void dse_dyn_obstacle_set_cylinder_height(uint32_t e, float v) {
    if (auto* c = GC<dse::DynamicObstacleComponent>(e)) {
        c->cylinder_height = v;
        c->dirty_ = true;
    }
}
