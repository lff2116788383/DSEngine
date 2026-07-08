/**
 * @file lua_binding_free_ecs_mesh_gap.gen.cpp
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

int L_dse_mesh_renderer_set_material_from_dmat(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* dmat_path = luaL_checkstring(L, 2);
    uint32_t material_index = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    int _ret = dse_mesh_renderer_set_material_from_dmat(e, dmat_path, material_index);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_mesh_renderer_set_material_params(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float metallic = static_cast<float>(luaL_checknumber(L, 2));
    float roughness = static_cast<float>(luaL_checknumber(L, 3));
    float ao = static_cast<float>(luaL_checknumber(L, 4));
    float er = static_cast<float>(luaL_checknumber(L, 5));
    float eg = static_cast<float>(luaL_checknumber(L, 6));
    float eb = static_cast<float>(luaL_checknumber(L, 7));
    float normal_strength = static_cast<float>(luaL_checknumber(L, 8));
    int receive_shadow = static_cast<int>(luaL_checkinteger(L, 9));
    int double_sided = static_cast<int>(luaL_checkinteger(L, 10));
    float cr = static_cast<float>(luaL_checknumber(L, 11));
    float cg = static_cast<float>(luaL_checknumber(L, 12));
    float cb = static_cast<float>(luaL_checknumber(L, 13));
    float ca = static_cast<float>(luaL_checknumber(L, 14));
    dse_mesh_renderer_set_material_params(e, metallic, roughness, ao, er, eg, eb, normal_strength, receive_shadow, double_sided, cr, cg, cb, ca);
    return 0;
}

int L_dse_mesh_renderer_set_texture(lua_State* L) {
    uint32_t out_handle = 0;
    int out_width = 0;
    int out_height = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* slot = luaL_checkstring(L, 2);
    const char* path = luaL_checkstring(L, 3);
    int _ret = dse_mesh_renderer_set_texture(e, slot, path, &out_handle, &out_width, &out_height);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, static_cast<lua_Integer>(out_handle));
    lua_pushinteger(L, out_width);
    lua_pushinteger(L, out_height);
    return 4;
}

} // namespace

void RegisterFreeMeshGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "dse");
    }
    helper::RegisterBindings(L, {
        {"mesh_renderer_set_material_from_dmat", L_dse_mesh_renderer_set_material_from_dmat},
        {"mesh_renderer_set_material_params", L_dse_mesh_renderer_set_material_params},
        {"mesh_renderer_set_texture", L_dse_mesh_renderer_set_texture},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
