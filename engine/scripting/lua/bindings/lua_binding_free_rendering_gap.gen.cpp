/**
 * @file lua_binding_free_rendering_gap.gen.cpp
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

int L_dse_render_screen_to_world_ray(lua_State* L) {
    float out_origin[3] = {0, 0, 0};
    float out_dir[3] = {0, 0, 0};
    float sx = static_cast<float>(luaL_checknumber(L, 1));
    float sy = static_cast<float>(luaL_checknumber(L, 2));
    int _ret = dse_render_screen_to_world_ray(sx, sy, out_origin, out_dir);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_origin[0]);
    lua_pushnumber(L, out_origin[1]);
    lua_pushnumber(L, out_origin[2]);
    lua_pushnumber(L, out_dir[0]);
    lua_pushnumber(L, out_dir[1]);
    lua_pushnumber(L, out_dir[2]);
    return 7;
}

int L_dse_render_world_to_screen(lua_State* L) {
    float out_sx = 0;
    float out_sy = 0;
    float wx = static_cast<float>(luaL_checknumber(L, 1));
    float wy = static_cast<float>(luaL_checknumber(L, 2));
    float wz = static_cast<float>(luaL_checknumber(L, 3));
    int _ret = dse_render_world_to_screen(wx, wy, wz, &out_sx, &out_sy);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_sx);
    lua_pushnumber(L, out_sy);
    return 3;
}

} // namespace

void RegisterFreeRenderingGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    helper::RegisterBindings(L, {
        {"render_screen_to_world_ray", L_dse_render_screen_to_world_ray},
        {"render_world_to_screen", L_dse_render_world_to_screen},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
