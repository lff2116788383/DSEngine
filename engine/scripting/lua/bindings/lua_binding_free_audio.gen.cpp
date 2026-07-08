/**
 * @file lua_binding_free_audio.gen.cpp
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

int L_dse_audio_fade_out_all_sfx(lua_State* L) {
    float duration_sec = static_cast<float>(luaL_checknumber(L, 1));
    dse_audio_fade_out_all_sfx(duration_sec);
    return 0;
}

int L_dse_audio_source_is_playing(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_audio_source_is_playing(e);
    lua_pushinteger(L, _ret);
    return 1;
}

} // namespace

void RegisterFreeFn_audio(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "audio");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "audio");
    }
    helper::RegisterBindings(L, {
        {"fade_out_all_sfx", L_dse_audio_fade_out_all_sfx},
        {"source_is_playing", L_dse_audio_source_is_playing},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
