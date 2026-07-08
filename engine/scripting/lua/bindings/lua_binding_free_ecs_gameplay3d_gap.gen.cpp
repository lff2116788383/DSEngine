/**
 * @file lua_binding_free_ecs_gameplay3d_gap.gen.cpp
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

int L_dse_day_night_get_sun_direction(lua_State* L) {
    float out_xyz[3] = {0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_day_night_get_sun_direction(e, out_xyz);
    lua_pushnumber(L, out_xyz[0]);
    lua_pushnumber(L, out_xyz[1]);
    lua_pushnumber(L, out_xyz[2]);
    return 3;
}

int L_dse_snow_cover_get(lua_State* L) {
    float out_coverage = 0;
    float out_target = 0;
    int out_enabled = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_snow_cover_get(e, &out_coverage, &out_target, &out_enabled);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_coverage);
    lua_pushnumber(L, out_target);
    lua_pushinteger(L, out_enabled);
    return 4;
}

} // namespace

void RegisterFreeGameplay3dGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "dse");
    }
    helper::RegisterBindings(L, {
        {"day_night_get_sun_direction", L_dse_day_night_get_sun_direction},
        {"snow_cover_get", L_dse_snow_cover_get},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
