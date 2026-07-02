/**
 * @file lua_binding_ecs_character_movement_cfg.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * CharacterMovementConfig 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_character_movement_cfg_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_character_movement_cfg_get_enabled(e));
    return 1;
}
int L_Set_character_movement_cfg_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_cfg_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_character_movement_cfg_max_walk_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_character_movement_cfg_get_max_walk_speed(e));
    return 1;
}
int L_Set_character_movement_cfg_max_walk_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_cfg_set_max_walk_speed(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_character_movement_cfg_max_sprint_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_character_movement_cfg_get_max_sprint_speed(e));
    return 1;
}
int L_Set_character_movement_cfg_max_sprint_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_cfg_set_max_sprint_speed(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_character_movement_cfg_max_crouch_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_character_movement_cfg_get_max_crouch_speed(e));
    return 1;
}
int L_Set_character_movement_cfg_max_crouch_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_cfg_set_max_crouch_speed(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_character_movement_cfg_ground_acceleration(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_character_movement_cfg_get_ground_acceleration(e));
    return 1;
}
int L_Set_character_movement_cfg_ground_acceleration(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_cfg_set_ground_acceleration(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_character_movement_cfg_ground_deceleration(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_character_movement_cfg_get_ground_deceleration(e));
    return 1;
}
int L_Set_character_movement_cfg_ground_deceleration(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_cfg_set_ground_deceleration(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_character_movement_cfg_ground_friction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_character_movement_cfg_get_ground_friction(e));
    return 1;
}
int L_Set_character_movement_cfg_ground_friction(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_cfg_set_ground_friction(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_character_movement_cfg_gravity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_character_movement_cfg_get_gravity(e));
    return 1;
}
int L_Set_character_movement_cfg_gravity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_cfg_set_gravity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_character_movement_cfg_jump_velocity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_character_movement_cfg_get_jump_velocity(e));
    return 1;
}
int L_Set_character_movement_cfg_jump_velocity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_cfg_set_jump_velocity(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_character_movement_cfg_max_jump_count(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_character_movement_cfg_get_max_jump_count(e));
    return 1;
}
int L_Set_character_movement_cfg_max_jump_count(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_cfg_set_max_jump_count(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_character_movement_cfg_coyote_time(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_character_movement_cfg_get_coyote_time(e));
    return 1;
}
int L_Set_character_movement_cfg_coyote_time(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_cfg_set_coyote_time(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_character_movement_cfg_jump_buffer_time(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_character_movement_cfg_get_jump_buffer_time(e));
    return 1;
}
int L_Set_character_movement_cfg_jump_buffer_time(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_cfg_set_jump_buffer_time(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_character_movement_cfg_air_control(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_character_movement_cfg_get_air_control(e));
    return 1;
}
int L_Set_character_movement_cfg_air_control(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_cfg_set_air_control(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_character_movement_cfg_rotation_rate(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_character_movement_cfg_get_rotation_rate(e));
    return 1;
}
int L_Set_character_movement_cfg_rotation_rate(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_cfg_set_rotation_rate(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_character_movement_cfg_publish_events(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_character_movement_cfg_get_publish_events(e));
    return 1;
}
int L_Set_character_movement_cfg_publish_events(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_character_movement_cfg_set_publish_events(e, lua_toboolean(L, 2));
    return 0;
}

} // namespace

void RegisterCharacterMovementConfigGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_character_movement_cfg_enabled", L_Get_character_movement_cfg_enabled},
        {"set_character_movement_cfg_enabled", L_Set_character_movement_cfg_enabled},
        {"get_character_movement_cfg_max_walk_speed", L_Get_character_movement_cfg_max_walk_speed},
        {"set_character_movement_cfg_max_walk_speed", L_Set_character_movement_cfg_max_walk_speed},
        {"get_character_movement_cfg_max_sprint_speed", L_Get_character_movement_cfg_max_sprint_speed},
        {"set_character_movement_cfg_max_sprint_speed", L_Set_character_movement_cfg_max_sprint_speed},
        {"get_character_movement_cfg_max_crouch_speed", L_Get_character_movement_cfg_max_crouch_speed},
        {"set_character_movement_cfg_max_crouch_speed", L_Set_character_movement_cfg_max_crouch_speed},
        {"get_character_movement_cfg_ground_acceleration", L_Get_character_movement_cfg_ground_acceleration},
        {"set_character_movement_cfg_ground_acceleration", L_Set_character_movement_cfg_ground_acceleration},
        {"get_character_movement_cfg_ground_deceleration", L_Get_character_movement_cfg_ground_deceleration},
        {"set_character_movement_cfg_ground_deceleration", L_Set_character_movement_cfg_ground_deceleration},
        {"get_character_movement_cfg_ground_friction", L_Get_character_movement_cfg_ground_friction},
        {"set_character_movement_cfg_ground_friction", L_Set_character_movement_cfg_ground_friction},
        {"get_character_movement_cfg_gravity", L_Get_character_movement_cfg_gravity},
        {"set_character_movement_cfg_gravity", L_Set_character_movement_cfg_gravity},
        {"get_character_movement_cfg_jump_velocity", L_Get_character_movement_cfg_jump_velocity},
        {"set_character_movement_cfg_jump_velocity", L_Set_character_movement_cfg_jump_velocity},
        {"get_character_movement_cfg_max_jump_count", L_Get_character_movement_cfg_max_jump_count},
        {"set_character_movement_cfg_max_jump_count", L_Set_character_movement_cfg_max_jump_count},
        {"get_character_movement_cfg_coyote_time", L_Get_character_movement_cfg_coyote_time},
        {"set_character_movement_cfg_coyote_time", L_Set_character_movement_cfg_coyote_time},
        {"get_character_movement_cfg_jump_buffer_time", L_Get_character_movement_cfg_jump_buffer_time},
        {"set_character_movement_cfg_jump_buffer_time", L_Set_character_movement_cfg_jump_buffer_time},
        {"get_character_movement_cfg_air_control", L_Get_character_movement_cfg_air_control},
        {"set_character_movement_cfg_air_control", L_Set_character_movement_cfg_air_control},
        {"get_character_movement_cfg_rotation_rate", L_Get_character_movement_cfg_rotation_rate},
        {"set_character_movement_cfg_rotation_rate", L_Set_character_movement_cfg_rotation_rate},
        {"get_character_movement_cfg_publish_events", L_Get_character_movement_cfg_publish_events},
        {"set_character_movement_cfg_publish_events", L_Set_character_movement_cfg_publish_events},
    });
}

} // namespace dse::runtime::lua_binding
