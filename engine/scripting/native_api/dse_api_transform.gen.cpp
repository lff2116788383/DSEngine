/**
 * @file dse_api_transform.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：TransformComponent）
 *
 * 组件字段 get/set 由 Codegen 生成（每组件一个 TU，与 Lua 拆分边界对齐）；
 * 手写 dse_api.cpp 仅保留 add/字符串/Input 等非字段 API。
 * 依赖 dse_api.h 提供的 dse_get_world_ptr() 访问已初始化的 World 指针。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
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

/* ---- TransformComponent ---- */
extern "C" void dse_transform_get_position(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<TransformComponent>(e)) { *x = c->position.x; *y = c->position.y; *z = c->position.z; }
}
extern "C" void dse_transform_set_position(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<TransformComponent>(e)) {
        c->position = glm::vec3(x, y, z);
        c->dirty = true;
    }
}
extern "C" void dse_transform_get_rotation(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<TransformComponent>(e)) {
        glm::vec3 euler = glm::degrees(glm::eulerAngles(c->rotation));
        *x = euler.x; *y = euler.y; *z = euler.z;
    }
}
extern "C" void dse_transform_set_rotation(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<TransformComponent>(e)) {
        c->rotation = glm::quat(glm::vec3(glm::radians(x), glm::radians(y), glm::radians(z)));
        c->dirty = true;
    }
}
extern "C" void dse_transform_get_scale(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<TransformComponent>(e)) { *x = c->scale.x; *y = c->scale.y; *z = c->scale.z; }
}
extern "C" void dse_transform_set_scale(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<TransformComponent>(e)) {
        c->scale = glm::vec3(x, y, z);
        c->dirty = true;
    }
}
