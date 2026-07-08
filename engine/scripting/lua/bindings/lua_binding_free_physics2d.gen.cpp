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
    std::vector<float> _verts; if (lua_istable(L, 2)) { lua_Integer _n = static_cast<lua_Integer>(lua_rawlen(L, 2)); for (lua_Integer _i = 1; _i <= _n; ++_i) { lua_rawgeti(L, 2, _i); if (lua_isnumber(L, -1)) _verts.push_back(static_cast<float>(lua_tonumber(L, -1))); lua_pop(L, 1); } }
    dse_physics2d_add_polygon_collider(static_cast<uint32_t>(luaL_checkinteger(L, 1)), _verts.data(), static_cast<int>(_verts.size()), static_cast<float>(luaL_checknumber(L, 3)), static_cast<float>(luaL_checknumber(L, 4)), static_cast<float>(luaL_checknumber(L, 5)));
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
