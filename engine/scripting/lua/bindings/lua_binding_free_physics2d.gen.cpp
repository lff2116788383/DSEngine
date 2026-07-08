/**
 * @file lua_binding_free_physics2d.gen.cpp
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

int L_dse_physics2d_add_polygon_collider(lua_State* L) {
    int e = static_cast<int>(luaL_checkinteger(L, 1));
    float verts = static_cast<float>(luaL_checknumber(L, 2));
    int count = static_cast<int>(luaL_checkinteger(L, 3));
    float density = static_cast<float>(luaL_checknumber(L, 4));
    float friction = static_cast<float>(luaL_checknumber(L, 5));
    float restitution = static_cast<float>(luaL_checknumber(L, 6));
    dse_physics2d_add_polygon_collider(e, verts, count, density, friction, restitution);
    return 0;
}

} // namespace

void RegisterFreeFn_physics2d(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "physics2d");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "physics2d");
    }
    helper::RegisterBindings(L, {
        {"physics2d_add_polygon_collider", L_dse_physics2d_add_polygon_collider},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
