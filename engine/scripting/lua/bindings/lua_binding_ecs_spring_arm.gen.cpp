/**
 * @file lua_binding_ecs_spring_arm.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * SpringArm3DComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_spring_arm_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_spring_arm_get_enabled(e));
    return 1;
}
int L_Set_spring_arm_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spring_arm_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_spring_arm_target_offset(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_spring_arm_get_target_offset(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_spring_arm_target_offset(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spring_arm_set_target_offset(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_spring_arm_arm_length(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_spring_arm_get_arm_length(e));
    return 1;
}
int L_Set_spring_arm_arm_length(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spring_arm_set_arm_length(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_spring_arm_collision_test(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_spring_arm_get_collision_test(e));
    return 1;
}
int L_Set_spring_arm_collision_test(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spring_arm_set_collision_test(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_spring_arm_pitch(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_spring_arm_get_pitch(e));
    return 1;
}
int L_Set_spring_arm_pitch(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spring_arm_set_pitch(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_spring_arm_yaw(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_spring_arm_get_yaw(e));
    return 1;
}
int L_Set_spring_arm_yaw(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spring_arm_set_yaw(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_spring_arm_position_lag_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_spring_arm_get_position_lag_speed(e));
    return 1;
}
int L_Set_spring_arm_position_lag_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spring_arm_set_position_lag_speed(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_spring_arm_shake_trauma(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_spring_arm_get_shake_trauma(e));
    return 1;
}
int L_Set_spring_arm_shake_trauma(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spring_arm_set_shake_trauma(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterSpringArm3DComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_spring_arm_enabled", L_Get_spring_arm_enabled},
        {"set_spring_arm_enabled", L_Set_spring_arm_enabled},
        {"get_spring_arm_target_offset", L_Get_spring_arm_target_offset},
        {"set_spring_arm_target_offset", L_Set_spring_arm_target_offset},
        {"get_spring_arm_arm_length", L_Get_spring_arm_arm_length},
        {"set_spring_arm_arm_length", L_Set_spring_arm_arm_length},
        {"get_spring_arm_collision_test", L_Get_spring_arm_collision_test},
        {"set_spring_arm_collision_test", L_Set_spring_arm_collision_test},
        {"get_spring_arm_pitch", L_Get_spring_arm_pitch},
        {"set_spring_arm_pitch", L_Set_spring_arm_pitch},
        {"get_spring_arm_yaw", L_Get_spring_arm_yaw},
        {"set_spring_arm_yaw", L_Set_spring_arm_yaw},
        {"get_spring_arm_position_lag_speed", L_Get_spring_arm_position_lag_speed},
        {"set_spring_arm_position_lag_speed", L_Set_spring_arm_position_lag_speed},
        {"get_spring_arm_shake_trauma", L_Get_spring_arm_shake_trauma},
        {"set_spring_arm_shake_trauma", L_Set_spring_arm_shake_trauma},
    });
}

} // namespace dse::runtime::lua_binding
