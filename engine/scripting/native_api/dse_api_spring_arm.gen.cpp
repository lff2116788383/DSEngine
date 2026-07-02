/**
 * @file dse_api_spring_arm.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：SpringArm3DComponent）
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

/* ---- SpringArm3DComponent ---- */
extern "C" int dse_spring_arm_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::SpringArm3DComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_spring_arm_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::SpringArm3DComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" void dse_spring_arm_get_target_offset(uint32_t e, float* x, float* y, float* z) {
    if (const auto* c = GCC<dse::SpringArm3DComponent>(e)) { *x = c->target_offset.x; *y = c->target_offset.y; *z = c->target_offset.z; }
}
extern "C" void dse_spring_arm_set_target_offset(uint32_t e, float x, float y, float z) {
    if (auto* c = GC<dse::SpringArm3DComponent>(e)) {
        c->target_offset = glm::vec3(x, y, z);
    }
}
extern "C" float dse_spring_arm_get_arm_length(uint32_t e) {
    const auto* c = GCC<dse::SpringArm3DComponent>(e);
    return c ? c->arm_length : 4.0f;
}
extern "C" void dse_spring_arm_set_arm_length(uint32_t e, float v) {
    if (auto* c = GC<dse::SpringArm3DComponent>(e)) {
        c->arm_length = v;
    }
}
extern "C" int dse_spring_arm_get_collision_test(uint32_t e) {
    const auto* c = GCC<dse::SpringArm3DComponent>(e);
    return (c && c->collision_test) ? 1 : 0;
}
extern "C" void dse_spring_arm_set_collision_test(uint32_t e, int v) {
    if (auto* c = GC<dse::SpringArm3DComponent>(e)) {
        c->collision_test = (v != 0);
    }
}
extern "C" float dse_spring_arm_get_pitch(uint32_t e) {
    const auto* c = GCC<dse::SpringArm3DComponent>(e);
    return c ? c->pitch : -20.0f;
}
extern "C" void dse_spring_arm_set_pitch(uint32_t e, float v) {
    if (auto* c = GC<dse::SpringArm3DComponent>(e)) {
        c->pitch = v;
    }
}
extern "C" float dse_spring_arm_get_yaw(uint32_t e) {
    const auto* c = GCC<dse::SpringArm3DComponent>(e);
    return c ? c->yaw : 0.0f;
}
extern "C" void dse_spring_arm_set_yaw(uint32_t e, float v) {
    if (auto* c = GC<dse::SpringArm3DComponent>(e)) {
        c->yaw = v;
    }
}
extern "C" float dse_spring_arm_get_position_lag_speed(uint32_t e) {
    const auto* c = GCC<dse::SpringArm3DComponent>(e);
    return c ? c->position_lag_speed : 10.0f;
}
extern "C" void dse_spring_arm_set_position_lag_speed(uint32_t e, float v) {
    if (auto* c = GC<dse::SpringArm3DComponent>(e)) {
        c->position_lag_speed = v;
    }
}
extern "C" float dse_spring_arm_get_shake_trauma(uint32_t e) {
    const auto* c = GCC<dse::SpringArm3DComponent>(e);
    return c ? c->shake_trauma : 0.0f;
}
extern "C" void dse_spring_arm_set_shake_trauma(uint32_t e, float v) {
    if (auto* c = GC<dse::SpringArm3DComponent>(e)) {
        c->shake_trauma = v;
    }
}
