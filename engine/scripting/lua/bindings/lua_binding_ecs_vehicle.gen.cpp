/**
 * @file lua_binding_ecs_vehicle.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * VehicleComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_vehicle_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_vehicle_get_enabled(e));
    return 1;
}
int L_Set_vehicle_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_vehicle_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_vehicle_max_engine_force(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_vehicle_get_max_engine_force(e));
    return 1;
}
int L_Set_vehicle_max_engine_force(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_vehicle_set_max_engine_force(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_vehicle_max_brake_force(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_vehicle_get_max_brake_force(e));
    return 1;
}
int L_Set_vehicle_max_brake_force(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_vehicle_set_max_brake_force(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_vehicle_max_steer_angle(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_vehicle_get_max_steer_angle(e));
    return 1;
}
int L_Set_vehicle_max_steer_angle(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_vehicle_set_max_steer_angle(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterVehicleComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_vehicle_enabled", L_Get_vehicle_enabled},
        {"set_vehicle_enabled", L_Set_vehicle_enabled},
        {"get_vehicle_max_engine_force", L_Get_vehicle_max_engine_force},
        {"set_vehicle_max_engine_force", L_Set_vehicle_max_engine_force},
        {"get_vehicle_max_brake_force", L_Get_vehicle_max_brake_force},
        {"set_vehicle_max_brake_force", L_Set_vehicle_max_brake_force},
        {"get_vehicle_max_steer_angle", L_Get_vehicle_max_steer_angle},
        {"set_vehicle_max_steer_angle", L_Set_vehicle_max_steer_angle},
    });
}

} // namespace dse::runtime::lua_binding
