/**
 * @file dse_api_bounding_box.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：BoundingBoxComponent）
 *
 * 组件字段 get/set 由 Codegen 生成（每组件一个 TU，与 Lua 拆分边界对齐）；
 * 手写 dse_api.cpp 仅保留 add/字符串/Input 等非字段 API。
 * 依赖 dse_api.h 提供的 dse_get_world_ptr() 访问已初始化的 World 指针。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_render.h"
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

/* ---- BoundingBoxComponent ---- */
extern "C" void dse_bounding_box_get_min_extents(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::BoundingBoxComponent>(e)) { *x = c->min_extents.x; *y = c->min_extents.y; *z = c->min_extents.z; }
}
extern "C" void dse_bounding_box_set_min_extents(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::BoundingBoxComponent>(e)) {
        c->min_extents = glm::vec3(x, y, z);
    }
}
extern "C" void dse_bounding_box_get_max_extents(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::BoundingBoxComponent>(e)) { *x = c->max_extents.x; *y = c->max_extents.y; *z = c->max_extents.z; }
}
extern "C" void dse_bounding_box_set_max_extents(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::BoundingBoxComponent>(e)) {
        c->max_extents = glm::vec3(x, y, z);
    }
}
