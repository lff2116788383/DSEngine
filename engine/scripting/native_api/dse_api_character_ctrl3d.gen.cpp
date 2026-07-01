/**
 * @file dse_api_character_ctrl3d.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：CharacterController3DComponent）
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

/* ---- CharacterController3DComponent ---- */
extern "C" float dse_character_ctrl3d_get_radius(uint32_t e) {
    const auto* c = GCC<dse::CharacterController3DComponent>(e);
    return c ? c->radius : 0.0f;
}
extern "C" void dse_character_ctrl3d_set_radius(uint32_t e, float v) {
    if (auto* c = GC<dse::CharacterController3DComponent>(e)) {
        c->radius = v;
    }
}
extern "C" float dse_character_ctrl3d_get_height(uint32_t e) {
    const auto* c = GCC<dse::CharacterController3DComponent>(e);
    return c ? c->height : 0.0f;
}
extern "C" void dse_character_ctrl3d_set_height(uint32_t e, float v) {
    if (auto* c = GC<dse::CharacterController3DComponent>(e)) {
        c->height = v;
    }
}
extern "C" float dse_character_ctrl3d_get_slope_limit(uint32_t e) {
    const auto* c = GCC<dse::CharacterController3DComponent>(e);
    return c ? c->slope_limit : 0.0f;
}
extern "C" void dse_character_ctrl3d_set_slope_limit(uint32_t e, float v) {
    if (auto* c = GC<dse::CharacterController3DComponent>(e)) {
        c->slope_limit = v;
    }
}
extern "C" float dse_character_ctrl3d_get_step_offset(uint32_t e) {
    const auto* c = GCC<dse::CharacterController3DComponent>(e);
    return c ? c->step_offset : 0.0f;
}
extern "C" void dse_character_ctrl3d_set_step_offset(uint32_t e, float v) {
    if (auto* c = GC<dse::CharacterController3DComponent>(e)) {
        c->step_offset = v;
    }
}
extern "C" float dse_character_ctrl3d_get_skin_width(uint32_t e) {
    const auto* c = GCC<dse::CharacterController3DComponent>(e);
    return c ? c->skin_width : 0.0f;
}
extern "C" void dse_character_ctrl3d_set_skin_width(uint32_t e, float v) {
    if (auto* c = GC<dse::CharacterController3DComponent>(e)) {
        c->skin_width = v;
    }
}
extern "C" float dse_character_ctrl3d_get_min_move_distance(uint32_t e) {
    const auto* c = GCC<dse::CharacterController3DComponent>(e);
    return c ? c->min_move_distance : 0.0f;
}
extern "C" void dse_character_ctrl3d_set_min_move_distance(uint32_t e, float v) {
    if (auto* c = GC<dse::CharacterController3DComponent>(e)) {
        c->min_move_distance = v;
    }
}
