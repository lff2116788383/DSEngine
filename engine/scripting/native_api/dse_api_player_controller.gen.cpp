/**
 * @file dse_api_player_controller.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json（组件：PlayerControllerComponent）
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

/* ---- PlayerControllerComponent ---- */
extern "C" int dse_player_controller_get_enabled(uint32_t e) {
    const auto* c = GCC<dse::PlayerControllerComponent>(e);
    return (c && c->enabled) ? 1 : 0;
}
extern "C" void dse_player_controller_set_enabled(uint32_t e, int v) {
    if (auto* c = GC<dse::PlayerControllerComponent>(e)) {
        c->enabled = (v != 0);
    }
}
extern "C" float dse_player_controller_get_mouse_sensitivity(uint32_t e) {
    const auto* c = GCC<dse::PlayerControllerComponent>(e);
    return c ? c->mouse_sensitivity : 0.15f;
}
extern "C" void dse_player_controller_set_mouse_sensitivity(uint32_t e, float v) {
    if (auto* c = GC<dse::PlayerControllerComponent>(e)) {
        c->mouse_sensitivity = v;
    }
}
extern "C" float dse_player_controller_get_gamepad_sensitivity(uint32_t e) {
    const auto* c = GCC<dse::PlayerControllerComponent>(e);
    return c ? c->gamepad_sensitivity : 2.0f;
}
extern "C" void dse_player_controller_set_gamepad_sensitivity(uint32_t e, float v) {
    if (auto* c = GC<dse::PlayerControllerComponent>(e)) {
        c->gamepad_sensitivity = v;
    }
}
extern "C" int dse_player_controller_get_invert_y(uint32_t e) {
    const auto* c = GCC<dse::PlayerControllerComponent>(e);
    return (c && c->invert_y) ? 1 : 0;
}
extern "C" void dse_player_controller_set_invert_y(uint32_t e, int v) {
    if (auto* c = GC<dse::PlayerControllerComponent>(e)) {
        c->invert_y = (v != 0);
    }
}
extern "C" float dse_player_controller_get_stick_dead_zone(uint32_t e) {
    const auto* c = GCC<dse::PlayerControllerComponent>(e);
    return c ? c->stick_dead_zone : 0.15f;
}
extern "C" void dse_player_controller_set_stick_dead_zone(uint32_t e, float v) {
    if (auto* c = GC<dse::PlayerControllerComponent>(e)) {
        c->stick_dead_zone = v;
    }
}
extern "C" float dse_player_controller_get_move_response_curve(uint32_t e) {
    const auto* c = GCC<dse::PlayerControllerComponent>(e);
    return c ? c->move_response_curve : 1.5f;
}
extern "C" void dse_player_controller_set_move_response_curve(uint32_t e, float v) {
    if (auto* c = GC<dse::PlayerControllerComponent>(e)) {
        c->move_response_curve = v;
    }
}
extern "C" float dse_player_controller_get_look_response_curve(uint32_t e) {
    const auto* c = GCC<dse::PlayerControllerComponent>(e);
    return c ? c->look_response_curve : 1.0f;
}
extern "C" void dse_player_controller_set_look_response_curve(uint32_t e, float v) {
    if (auto* c = GC<dse::PlayerControllerComponent>(e)) {
        c->look_response_curve = v;
    }
}
