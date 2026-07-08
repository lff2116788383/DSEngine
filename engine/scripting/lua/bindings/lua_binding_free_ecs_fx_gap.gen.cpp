/**
 * @file lua_binding_free_ecs_fx_gap.gen.cpp
 * @brief auto-generated -- do not edit
 *        source: tools/codegen/function_defs.json
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}
#include <cmath>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>

namespace dse::runtime::lua_binding {
namespace {

int L_dse_steering_get_state(lua_State* L) {
    int out_flags = 0;
    float out_velocity[3] = {0, 0, 0};
    float out_params = 0;
    float out_targets = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_steering_get_state(e, &out_flags, out_velocity, &out_params, &out_targets);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_flags);
    lua_pushnumber(L, out_velocity[0]);
    lua_pushnumber(L, out_velocity[1]);
    lua_pushnumber(L, out_velocity[2]);
    lua_pushnumber(L, out_params);
    lua_pushnumber(L, out_targets);
    return 7;
}

int L_dse_steering_set_target(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int behavior = static_cast<int>(luaL_checkinteger(L, 2));
    float x = static_cast<float>(luaL_checknumber(L, 3));
    float y = static_cast<float>(luaL_checknumber(L, 4));
    float z = static_cast<float>(luaL_checknumber(L, 5));
    int _ret = dse_steering_set_target(e, behavior, x, y, z);
    lua_pushinteger(L, _ret);
    return 1;
}

} // namespace

void RegisterFreeFxGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "dse");
    }
    helper::RegisterBindings(L, {
        {"steering_get_state", L_dse_steering_get_state},
        {"steering_set_target", L_dse_steering_set_target},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
