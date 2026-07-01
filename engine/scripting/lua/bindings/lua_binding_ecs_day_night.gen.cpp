/**
 * @file lua_binding_ecs_day_night.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * DayNightCycleComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_day_night_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_day_night_get_enabled(e));
    return 1;
}
int L_Set_day_night_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_day_night_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_day_night_time_of_day(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_day_night_get_time_of_day(e));
    return 1;
}
int L_Set_day_night_time_of_day(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_day_night_set_time_of_day(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_day_night_time_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_day_night_get_time_speed(e));
    return 1;
}
int L_Set_day_night_time_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_day_night_set_time_speed(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_day_night_auto_advance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_day_night_get_auto_advance(e));
    return 1;
}
int L_Set_day_night_auto_advance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_day_night_set_auto_advance(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_day_night_latitude(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_day_night_get_latitude(e));
    return 1;
}
int L_Set_day_night_latitude(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_day_night_set_latitude(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_day_night_longitude(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_day_night_get_longitude(e));
    return 1;
}
int L_Set_day_night_longitude(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_day_night_set_longitude(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_day_night_day_of_year(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_day_night_get_day_of_year(e));
    return 1;
}
int L_Set_day_night_day_of_year(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_day_night_set_day_of_year(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}

} // namespace

void RegisterDayNightCycleComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_day_night_enabled", L_Get_day_night_enabled},
        {"set_day_night_enabled", L_Set_day_night_enabled},
        {"get_day_night_time_of_day", L_Get_day_night_time_of_day},
        {"set_day_night_time_of_day", L_Set_day_night_time_of_day},
        {"get_day_night_time_speed", L_Get_day_night_time_speed},
        {"set_day_night_time_speed", L_Set_day_night_time_speed},
        {"get_day_night_auto_advance", L_Get_day_night_auto_advance},
        {"set_day_night_auto_advance", L_Set_day_night_auto_advance},
        {"get_day_night_latitude", L_Get_day_night_latitude},
        {"set_day_night_latitude", L_Set_day_night_latitude},
        {"get_day_night_longitude", L_Get_day_night_longitude},
        {"set_day_night_longitude", L_Set_day_night_longitude},
        {"get_day_night_day_of_year", L_Get_day_night_day_of_year},
        {"set_day_night_day_of_year", L_Set_day_night_day_of_year},
    });
}

} // namespace dse::runtime::lua_binding
