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

int L_dse_audio_bus_get_names(lua_State* L) {
    char _buf[4096];
    int _count = dse_audio_bus_get_names(_buf, sizeof(_buf));
    lua_newtable(L);
    int _offset = 0;
    int _idx = 1;
    for (int _i = 0; _i < _count && _offset < static_cast<int>(sizeof(_buf)); ++_i) {
        const char* _name = _buf + _offset;
        lua_pushstring(L, _name);
        lua_rawseti(L, -2, _idx++);
        _offset += static_cast<int>(strlen(_name)) + 1;
    }
    return 1;
}

int L_dse_audio_snapshot_list(lua_State* L) {
    char _buf[4096];
    int _count = dse_audio_snapshot_list(_buf, sizeof(_buf));
    lua_newtable(L);
    int _offset = 0;
    int _idx = 1;
    for (int _i = 0; _i < _count && _offset < static_cast<int>(sizeof(_buf)); ++_i) {
        const char* _name = _buf + _offset;
        lua_pushstring(L, _name);
        lua_rawseti(L, -2, _idx++);
        _offset += static_cast<int>(strlen(_name)) + 1;
    }
    return 1;
}

int L_dse_audio_source_get_state_ex(lua_State* L) {
    int out_clip_loaded = 0;
    int out_is_playing = 0;
    int out_spatial = 0;
    float out_min_dist = 0;
    float out_max_dist = 0;
    float out_rolloff = 0;
    float out_volume = 0;
    float out_pitch = 0;
    double out_runtime_handle = 0;
    double out_clip_bytes = 0;
    char out_path[256] = {0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_audio_source_get_state_ex(e, &out_clip_loaded, &out_is_playing, &out_spatial, &out_min_dist, &out_max_dist, &out_rolloff, &out_volume, &out_pitch, &out_runtime_handle, &out_clip_bytes, out_path, sizeof(out_path));
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_clip_loaded);
    lua_pushinteger(L, out_is_playing);
    lua_pushinteger(L, out_spatial);
    lua_pushnumber(L, out_min_dist);
    lua_pushnumber(L, out_max_dist);
    lua_pushnumber(L, out_rolloff);
    lua_pushnumber(L, out_volume);
    lua_pushnumber(L, out_pitch);
    lua_pushnumber(L, static_cast<lua_Number>(out_runtime_handle));
    lua_pushnumber(L, static_cast<lua_Number>(out_clip_bytes));
    lua_pushstring(L, out_path);
    return 12;
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
        {"bus_get_names", L_dse_audio_bus_get_names},
        {"snapshot_list", L_dse_audio_snapshot_list},
        {"audio_source_get_state", L_dse_audio_source_get_state_ex},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
