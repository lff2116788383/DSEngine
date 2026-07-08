/**
 * @file lua_binding_free_ecs_animation_gap.gen.cpp
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

int L_dse_anim3d_get_root_motion_delta(lua_State* L) {
    float out_xyz[3] = {0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_anim3d_get_root_motion_delta(e, out_xyz);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_xyz[0]);
    lua_pushnumber(L, out_xyz[1]);
    lua_pushnumber(L, out_xyz[2]);
    return 4;
}

int L_dse_bone_attach_get_world_pos(lua_State* L) {
    float out_xyz[3] = {0, 0, 0};
    uint32_t target = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* bone_name = luaL_checkstring(L, 2);
    int _ret = dse_bone_attach_get_world_pos(target, bone_name, out_xyz);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_xyz[0]);
    lua_pushnumber(L, out_xyz[1]);
    lua_pushnumber(L, out_xyz[2]);
    return 4;
}

int L_dse_morph_simple_get_weight(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float _ret = dse_morph_simple_get_weight(e, name);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_morph_simple_get_weight_index(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int idx = static_cast<int>(luaL_checkinteger(L, 2));
    float _ret = dse_morph_simple_get_weight_index(e, idx);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_morph_simple_set_weight(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float w = static_cast<float>(luaL_checknumber(L, 3));
    dse_morph_simple_set_weight(e, name, w);
    return 0;
}

int L_dse_morph_simple_set_weight_index(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int idx = static_cast<int>(luaL_checkinteger(L, 2));
    float w = static_cast<float>(luaL_checknumber(L, 3));
    dse_morph_simple_set_weight_index(e, idx, w);
    return 0;
}

} // namespace

void RegisterFreeAnimationGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "dse");
    }
    helper::RegisterBindings(L, {
        {"anim3d_get_root_motion_delta", L_dse_anim3d_get_root_motion_delta},
        {"bone_attach_get_world_pos", L_dse_bone_attach_get_world_pos},
        {"morph_simple_get_weight", L_dse_morph_simple_get_weight},
        {"morph_simple_get_weight_index", L_dse_morph_simple_get_weight_index},
        {"morph_simple_set_weight", L_dse_morph_simple_set_weight},
        {"morph_simple_set_weight_index", L_dse_morph_simple_set_weight_index},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
