/**
 * @file lua_binding_free_scene_gap.gen.cpp
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

int L_dse_scene_instantiate_prefab(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    int use_pos = static_cast<int>(luaL_checkinteger(L, 5));
    uint32_t _ret = dse_scene_instantiate_prefab(path, x, y, z, use_pos);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_scene_load_sub(lua_State* L) {
    int out_entity_count = 0;
    const char* path = luaL_checkstring(L, 1);
    int _ret = dse_scene_load_sub(path, &out_entity_count);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_entity_count);
    return 2;
}

} // namespace

void RegisterFreeSceneGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    helper::RegisterBindings(L, {
        {"scene_instantiate_prefab", L_dse_scene_instantiate_prefab},
        {"scene_load_sub", L_dse_scene_load_sub},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
