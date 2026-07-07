/**
 * @file lua_binding_free_ai_gap.gen.cpp
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

int L_dse_ai_lod_get_config(lua_State* L) {
    float out_near_dist = 0;
    float out_far_dist = 0;
    int out_max_level = 0;
    dse_ai_lod_get_config(&out_near_dist, &out_far_dist, &out_max_level);
    lua_pushnumber(L, out_near_dist);
    lua_pushnumber(L, out_far_dist);
    lua_pushinteger(L, out_max_level);
    return 3;
}

} // namespace

void RegisterFreeAiGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    helper::RegisterBindings(L, {
        {"ai_lod_get_config", L_dse_ai_lod_get_config},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
