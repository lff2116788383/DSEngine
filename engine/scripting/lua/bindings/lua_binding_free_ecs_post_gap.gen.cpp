/**
 * @file lua_binding_free_ecs_post_gap.gen.cpp
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

int L_dse_post_process_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_add(e);
    return 0;
}

int L_dse_post_process_get_state(lua_State* L) {
    int out_enabled = 0;
    int out_bloom = 0;
    int out_ssao = 0;
    int out_ssr = 0;
    int out_fxaa = 0;
    int out_dof = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_post_process_get_state(e, &out_enabled, &out_bloom, &out_ssao, &out_ssr, &out_fxaa, &out_dof);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_enabled);
    lua_pushinteger(L, out_bloom);
    lua_pushinteger(L, out_ssao);
    lua_pushinteger(L, out_ssr);
    lua_pushinteger(L, out_fxaa);
    lua_pushinteger(L, out_dof);
    return 7;
}

} // namespace

void RegisterFreePostGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    helper::RegisterBindings(L, {
        {"post_process_add", L_dse_post_process_add},
        {"post_process_get_state", L_dse_post_process_get_state},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
