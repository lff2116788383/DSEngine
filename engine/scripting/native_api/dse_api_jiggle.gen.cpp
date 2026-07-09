/**
 * @file dse_api_jiggle.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：JiggleBoneComponent）
 *
 * 组件字段 get/set 由 Codegen 生成（每组件一个 TU，与 Lua 拆分边界对齐）；
 * 手写 dse_api.cpp 仅保留 add/字符串/Input 等非字段 API。
 * 依赖 dse_api.h 提供的 dse_get_world_ptr() 访问已初始化的 World 指针。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_animation.h"
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

/* ---- JiggleBoneComponent ---- */
extern "C" int dse_jiggle_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::JiggleBoneComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_jiggle_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::JiggleBoneComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" float dse_jiggle_get_stiffness_scale(uint32_t e) {
    const auto* c = GCC<dse::JiggleBoneComponent>(e);
    return c ? c->stiffness_scale : 1.0f;
}
extern "C" void dse_jiggle_set_stiffness_scale(uint32_t e, float v) {
    if (auto* c = GC<dse::JiggleBoneComponent>(e)) {
        c->stiffness_scale = v;
    }
}
extern "C" float dse_jiggle_get_damping_scale(uint32_t e) {
    const auto* c = GCC<dse::JiggleBoneComponent>(e);
    return c ? c->damping_scale : 1.0f;
}
extern "C" void dse_jiggle_set_damping_scale(uint32_t e, float v) {
    if (auto* c = GC<dse::JiggleBoneComponent>(e)) {
        c->damping_scale = v;
    }
}
extern "C" float dse_jiggle_get_gravity_scale(uint32_t e) {
    const auto* c = GCC<dse::JiggleBoneComponent>(e);
    return c ? c->gravity_scale : 1.0f;
}
extern "C" void dse_jiggle_set_gravity_scale(uint32_t e, float v) {
    if (auto* c = GC<dse::JiggleBoneComponent>(e)) {
        c->gravity_scale = v;
    }
}
