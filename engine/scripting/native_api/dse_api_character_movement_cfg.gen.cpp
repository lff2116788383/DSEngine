/**
 * @file dse_api_character_movement_cfg.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：CharacterMovementConfig）
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

/* ---- CharacterMovementConfig ---- */
extern "C" int dse_character_movement_cfg_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementConfig>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_character_movement_cfg_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::CharacterMovementConfig>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" float dse_character_movement_cfg_get_max_walk_speed(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementConfig>(e);
    return c ? c->max_walk_speed : 6.0f;
}
extern "C" void dse_character_movement_cfg_set_max_walk_speed(uint32_t e, float v) {
    if (auto* c = GC<dse::CharacterMovementConfig>(e)) {
        c->max_walk_speed = v;
    }
}
extern "C" float dse_character_movement_cfg_get_max_sprint_speed(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementConfig>(e);
    return c ? c->max_sprint_speed : 10.0f;
}
extern "C" void dse_character_movement_cfg_set_max_sprint_speed(uint32_t e, float v) {
    if (auto* c = GC<dse::CharacterMovementConfig>(e)) {
        c->max_sprint_speed = v;
    }
}
extern "C" float dse_character_movement_cfg_get_max_crouch_speed(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementConfig>(e);
    return c ? c->max_crouch_speed : 3.0f;
}
extern "C" void dse_character_movement_cfg_set_max_crouch_speed(uint32_t e, float v) {
    if (auto* c = GC<dse::CharacterMovementConfig>(e)) {
        c->max_crouch_speed = v;
    }
}
extern "C" float dse_character_movement_cfg_get_ground_acceleration(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementConfig>(e);
    return c ? c->ground_acceleration : 20.0f;
}
extern "C" void dse_character_movement_cfg_set_ground_acceleration(uint32_t e, float v) {
    if (auto* c = GC<dse::CharacterMovementConfig>(e)) {
        c->ground_acceleration = v;
    }
}
extern "C" float dse_character_movement_cfg_get_ground_deceleration(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementConfig>(e);
    return c ? c->ground_deceleration : 12.0f;
}
extern "C" void dse_character_movement_cfg_set_ground_deceleration(uint32_t e, float v) {
    if (auto* c = GC<dse::CharacterMovementConfig>(e)) {
        c->ground_deceleration = v;
    }
}
extern "C" float dse_character_movement_cfg_get_ground_friction(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementConfig>(e);
    return c ? c->ground_friction : 8.0f;
}
extern "C" void dse_character_movement_cfg_set_ground_friction(uint32_t e, float v) {
    if (auto* c = GC<dse::CharacterMovementConfig>(e)) {
        c->ground_friction = v;
    }
}
extern "C" float dse_character_movement_cfg_get_gravity(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementConfig>(e);
    return c ? c->gravity : -19.62f;
}
extern "C" void dse_character_movement_cfg_set_gravity(uint32_t e, float v) {
    if (auto* c = GC<dse::CharacterMovementConfig>(e)) {
        c->gravity = v;
    }
}
extern "C" float dse_character_movement_cfg_get_jump_velocity(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementConfig>(e);
    return c ? c->jump_velocity : 8.0f;
}
extern "C" void dse_character_movement_cfg_set_jump_velocity(uint32_t e, float v) {
    if (auto* c = GC<dse::CharacterMovementConfig>(e)) {
        c->jump_velocity = v;
    }
}
extern "C" int dse_character_movement_cfg_get_max_jump_count(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementConfig>(e);
    return c ? static_cast<int>(c->max_jump_count) : 2;
}
extern "C" void dse_character_movement_cfg_set_max_jump_count(uint32_t e, int v) {
    if (auto* c = GC<dse::CharacterMovementConfig>(e)) {
        c->max_jump_count = v;
    }
}
extern "C" float dse_character_movement_cfg_get_coyote_time(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementConfig>(e);
    return c ? c->coyote_time : 0.1f;
}
extern "C" void dse_character_movement_cfg_set_coyote_time(uint32_t e, float v) {
    if (auto* c = GC<dse::CharacterMovementConfig>(e)) {
        c->coyote_time = v;
    }
}
extern "C" float dse_character_movement_cfg_get_jump_buffer_time(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementConfig>(e);
    return c ? c->jump_buffer_time : 0.15f;
}
extern "C" void dse_character_movement_cfg_set_jump_buffer_time(uint32_t e, float v) {
    if (auto* c = GC<dse::CharacterMovementConfig>(e)) {
        c->jump_buffer_time = v;
    }
}
extern "C" float dse_character_movement_cfg_get_air_control(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementConfig>(e);
    return c ? c->air_control : 0.3f;
}
extern "C" void dse_character_movement_cfg_set_air_control(uint32_t e, float v) {
    if (auto* c = GC<dse::CharacterMovementConfig>(e)) {
        c->air_control = v;
    }
}
extern "C" float dse_character_movement_cfg_get_rotation_rate(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementConfig>(e);
    return c ? c->rotation_rate : 720.0f;
}
extern "C" void dse_character_movement_cfg_set_rotation_rate(uint32_t e, float v) {
    if (auto* c = GC<dse::CharacterMovementConfig>(e)) {
        c->rotation_rate = v;
    }
}
extern "C" int dse_character_movement_cfg_get_publish_events(uint32_t e) {
    const auto* c = GCC<dse::CharacterMovementConfig>(e);
    return (c && c->publish_events) ? 1 : 0;
}
extern "C" void dse_character_movement_cfg_set_publish_events(uint32_t e, int v) {
    if (auto* c = GC<dse::CharacterMovementConfig>(e)) {
        c->publish_events = (v != 0);
    }
}
