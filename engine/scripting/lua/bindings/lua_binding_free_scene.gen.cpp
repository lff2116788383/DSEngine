/**
 * @file lua_binding_free_scene.gen.cpp
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

int L_dse_scene_get_active(lua_State* L) {
    const char* out = luaL_checkstring(L, 1);
    int cap = static_cast<int>(luaL_checkinteger(L, 2));
    int _ret = dse_scene_get_active(out, cap);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_scene_get_loaded_subs(lua_State* L) {
    const char* out = luaL_checkstring(L, 1);
    int cap = static_cast<int>(luaL_checkinteger(L, 2));
    int _ret = dse_scene_get_loaded_subs(out, cap);
    lua_pushinteger(L, _ret);
    return 1;
}

} // namespace

void RegisterFreeFn_scene(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "scene");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "scene");
    }
    helper::RegisterBindings(L, {
        {"scene_get_active", L_dse_scene_get_active},
        {"scene_get_loaded_subs", L_dse_scene_get_loaded_subs},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
