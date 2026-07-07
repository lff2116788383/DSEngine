/**
 * @file lua_binding_free_ecs_physics2d_gap.gen.cpp
 * @brief auto-generated -- do not edit
 *        source: tools/codegen/function_defs.json
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_dse_physics2d_poll_collision_event(lua_State* L) {
    uint32_t out_other = 0;
    int out_is_trigger = 0;
    int out_is_enter = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_physics2d_poll_collision_event(e, &out_other, &out_is_trigger, &out_is_enter);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, static_cast<lua_Integer>(out_other));
    lua_pushinteger(L, out_is_trigger);
    lua_pushinteger(L, out_is_enter);
    return 4;
}

int L_dse_physics2d_raycast(lua_State* L) {
    uint32_t out_entity = 0;
    float out_point[3] = {0, 0, 0};
    float out_normal[3] = {0, 0, 0};
    float sx = static_cast<float>(luaL_checknumber(L, 1));
    float sy = static_cast<float>(luaL_checknumber(L, 2));
    float ex = static_cast<float>(luaL_checknumber(L, 3));
    float ey = static_cast<float>(luaL_checknumber(L, 4));
    int _ret = dse_physics2d_raycast(sx, sy, ex, ey, &out_entity, out_point, out_normal);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, static_cast<lua_Integer>(out_entity));
    lua_pushnumber(L, out_point[0]);
    lua_pushnumber(L, out_point[1]);
    lua_pushnumber(L, out_point[2]);
    lua_pushnumber(L, out_normal[0]);
    lua_pushnumber(L, out_normal[1]);
    lua_pushnumber(L, out_normal[2]);
    return 8;
}

} // namespace

void RegisterFreePhysics2dGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    helper::RegisterBindings(L, {
        {"physics2d_poll_collision_event", L_dse_physics2d_poll_collision_event},
        {"physics2d_raycast", L_dse_physics2d_raycast},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
