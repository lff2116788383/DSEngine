/**
 * @file lua_binding_free_ecs_2d_gap.gen.cpp
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

int L_dse_atlas_get_entry_uv(lua_State* L) {
    float out_uv[4] = {0, 0, 0, 0};
    int atlas = static_cast<int>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    dse_atlas_get_entry_uv(atlas, name, out_uv);
    lua_pushnumber(L, out_uv[0]);
    lua_pushnumber(L, out_uv[1]);
    lua_pushnumber(L, out_uv[2]);
    lua_pushnumber(L, out_uv[3]);
    return 4;
}

int L_dse_sprite_sheet_get_frame_uv(lua_State* L) {
    float out_uv[4] = {0, 0, 0, 0};
    int sheet = static_cast<int>(luaL_checkinteger(L, 1));
    int frame = static_cast<int>(luaL_checkinteger(L, 2));
    dse_sprite_sheet_get_frame_uv(sheet, frame, out_uv);
    lua_pushnumber(L, out_uv[0]);
    lua_pushnumber(L, out_uv[1]);
    lua_pushnumber(L, out_uv[2]);
    lua_pushnumber(L, out_uv[3]);
    return 4;
}

} // namespace

void RegisterFree2dGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    helper::RegisterBindings(L, {
        {"atlas_get_entry_uv", L_dse_atlas_get_entry_uv},
        {"sprite_sheet_get_frame_uv", L_dse_sprite_sheet_get_frame_uv},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
