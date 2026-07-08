/**
 * @file lua_binding_free_ecs_core_gap.gen.cpp
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

int L_dse_ecs_get_local_aabb(lua_State* L) {
    float out_min_max[6] = {0, 0, 0, 0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_ecs_get_local_aabb(e, out_min_max);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_min_max[0]);
    lua_pushnumber(L, out_min_max[1]);
    lua_pushnumber(L, out_min_max[2]);
    lua_pushnumber(L, out_min_max[3]);
    lua_pushnumber(L, out_min_max[4]);
    lua_pushnumber(L, out_min_max[5]);
    return 7;
}

int L_dse_ecs_get_world_aabb(lua_State* L) {
    float out_min_max[6] = {0, 0, 0, 0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_ecs_get_world_aabb(e, out_min_max);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_min_max[0]);
    lua_pushnumber(L, out_min_max[1]);
    lua_pushnumber(L, out_min_max[2]);
    lua_pushnumber(L, out_min_max[3]);
    lua_pushnumber(L, out_min_max[4]);
    lua_pushnumber(L, out_min_max[5]);
    return 7;
}

int L_dse_entity_valid(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_entity_valid(e);
    lua_pushinteger(L, _ret);
    return 1;
}

} // namespace

void RegisterFreeEcsCoreGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "dse");
    }
    helper::RegisterBindings(L, {
        {"ecs_get_local_aabb", L_dse_ecs_get_local_aabb},
        {"ecs_get_world_aabb", L_dse_ecs_get_world_aabb},
        {"entity_valid", L_dse_entity_valid},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
