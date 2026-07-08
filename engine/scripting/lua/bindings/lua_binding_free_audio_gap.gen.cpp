/**
 * @file lua_binding_free_audio_gap.gen.cpp
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

int L_dse_audio_lod_get_stats(lua_State* L) {
    int out_stats = 0;
    int _ret = dse_audio_lod_get_stats(&out_stats);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_stats);
    return 2;
}

} // namespace

void RegisterFreeAudioGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "dse");
    }
    helper::RegisterBindings(L, {
        {"audio_lod_get_stats", L_dse_audio_lod_get_stats},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
