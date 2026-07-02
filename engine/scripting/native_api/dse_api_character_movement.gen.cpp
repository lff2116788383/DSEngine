/**
 * @file dse_api_character_movement.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：CharacterMovementState）
 *
 * 组件字段 get/set 由 Codegen 生成（每组件一个 TU，与 Lua 拆分边界对齐）；
 * 手写 dse_api.cpp 仅保留 add/字符串/Input 等非字段 API。
 * 依赖 dse_api.h 提供的 dse_get_world_ptr() 访问已初始化的 World 指针。
 */

#include "engine/scripting/native_api/dse_api.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_character.h"
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

/* ---- CharacterMovementState ---- */
extern "C" void dse_character_movement_get_input_direction(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::CharacterMovementState>(e)) { *x = c->input_direction.x; *y = c->input_direction.y; *z = c->input_direction.z; }
}
extern "C" void dse_character_movement_set_input_direction(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::CharacterMovementState>(e)) {
        c->input_direction = glm::vec3(x, y, z);
    }
}
extern "C" int dse_character_movement_get_input_jump(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementState>(e);
    return (c && c->input_jump) ? 1 : 0;
}
extern "C" void dse_character_movement_set_input_jump(uint32_t e, int v) {
    if (auto* c = GC<dse::CharacterMovementState>(e)) {
        c->input_jump = (v != 0);
    }
}
extern "C" int dse_character_movement_get_input_sprint(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementState>(e);
    return (c && c->input_sprint) ? 1 : 0;
}
extern "C" void dse_character_movement_set_input_sprint(uint32_t e, int v) {
    if (auto* c = GC<dse::CharacterMovementState>(e)) {
        c->input_sprint = (v != 0);
    }
}
extern "C" int dse_character_movement_get_input_crouch(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementState>(e);
    return (c && c->input_crouch) ? 1 : 0;
}
extern "C" void dse_character_movement_set_input_crouch(uint32_t e, int v) {
    if (auto* c = GC<dse::CharacterMovementState>(e)) {
        c->input_crouch = (v != 0);
    }
}
extern "C" void dse_character_movement_get_velocity(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::CharacterMovementState>(e)) { *x = c->velocity.x; *y = c->velocity.y; *z = c->velocity.z; }
}
extern "C" int dse_character_movement_get_is_grounded(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementState>(e);
    return (c && c->is_grounded) ? 1 : 0;
}
extern "C" int dse_character_movement_get_is_jumping(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementState>(e);
    return (c && c->is_jumping) ? 1 : 0;
}
extern "C" int dse_character_movement_get_jump_count(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementState>(e);
    return c ? static_cast<int>(c->jump_count) : 0;
}
