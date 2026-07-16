/**
 * @file lua_binding_free_audio_full.gen.cpp
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

int L_dse_audio_source_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    int play_on_awake = helper::CheckBool(L, 3) ? 1 : 0;
    int loop = helper::CheckBool(L, 4) ? 1 : 0;
    float volume = static_cast<float>(luaL_checknumber(L, 5));
    dse_audio_source_add(e, path, play_on_awake, loop, volume);
    return 0;
}

int L_dse_audio_source_set_playing(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int playing = helper::CheckBool(L, 2) ? 1 : 0;
    dse_audio_source_set_playing(e, playing);
    return 0;
}

int L_dse_audio_source_set_volume(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float volume = static_cast<float>(luaL_checknumber(L, 2));
    dse_audio_source_set_volume(e, volume);
    return 0;
}

int L_dse_audio_source_set_pitch(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float pitch = static_cast<float>(luaL_checknumber(L, 2));
    dse_audio_source_set_pitch(e, pitch);
    return 0;
}

int L_dse_compat_audio_set_spatial(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    float min_distance = static_cast<float>(luaL_checknumber(L, 3));
    float max_distance = static_cast<float>(luaL_checknumber(L, 4));
    float rolloff = static_cast<float>(luaL_checknumber(L, 5));
    dse_audio_source_set_3d_mode(e, enabled);
    dse_audio_source_set_3d_distance(e, min_distance, max_distance, rolloff);
    return 0;
}

int L_dse_audio_source_restart(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_audio_source_restart(e);
    return 0;
}

int L_dse_audio_source_set_loop(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int loop = helper::CheckBool(L, 2) ? 1 : 0;
    dse_audio_source_set_loop(e, loop);
    return 0;
}

int L_dse_audio_source_set_3d_mode(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    dse_audio_source_set_3d_mode(e, enabled);
    return 0;
}

int L_dse_audio_listener_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::OptBool(L, 2, true) ? 1 : 0;
    dse_audio_listener_add(e, enabled);
    return 0;
}

int L_dse_audio_source_set_3d_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float min_distance = static_cast<float>(luaL_checknumber(L, 2));
    float max_distance = static_cast<float>(luaL_checknumber(L, 3));
    float rolloff = static_cast<float>(luaL_checknumber(L, 4));
    dse_audio_source_set_3d_distance(e, min_distance, max_distance, rolloff);
    return 0;
}

int L_dse_audio_bus_set_volume(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    float volume = static_cast<float>(luaL_checknumber(L, 2));
    int _ret = dse_audio_bus_set_volume(name, volume);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_audio_bus_set_muted(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    int muted = static_cast<int>(luaL_checkinteger(L, 2));
    int _ret = dse_audio_bus_set_muted(name, muted);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_audio_bus_create(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    const char* parent = luaL_checkstring(L, 2);
    float volume = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    int _ret = dse_audio_bus_create(name, parent, volume);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_audio_bus_remove(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    int _ret = dse_audio_bus_remove(name);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_audio_bus_add_effect(lua_State* L) {
    const char* bus_name = luaL_checkstring(L, 1);
    int type = static_cast<int>(luaL_checkinteger(L, 2));
    float cutoff_hz = static_cast<float>(luaL_optnumber(L, 3, 1000.0));
    float q = static_cast<float>(luaL_optnumber(L, 4, 0.707));
    float delay_time_ms = static_cast<float>(luaL_optnumber(L, 5, 250.0));
    float feedback = static_cast<float>(luaL_optnumber(L, 6, 0.3));
    float wet_mix = static_cast<float>(luaL_optnumber(L, 7, 0.5));
    float room_size = static_cast<float>(luaL_optnumber(L, 8, 0.5));
    float damping = static_cast<float>(luaL_optnumber(L, 9, 0.5));
    int _ret = dse_audio_bus_add_effect(bus_name, type, cutoff_hz, q, delay_time_ms, feedback, wet_mix, room_size, damping);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_audio_bus_remove_effect(lua_State* L) {
    const char* bus_name = luaL_checkstring(L, 1);
    int index = static_cast<int>(luaL_checkinteger(L, 2));
    int _ret = dse_audio_bus_remove_effect(bus_name, index);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_audio_play_bgm(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float volume = static_cast<float>(luaL_checknumber(L, 2));
    int loop = static_cast<int>(luaL_checkinteger(L, 3));
    int _ret = dse_audio_play_bgm(path, volume, loop);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_audio_pause_bgm(lua_State* L) {
    dse_audio_pause_bgm();
    return 0;
}

int L_dse_audio_resume_bgm(lua_State* L) {
    dse_audio_resume_bgm();
    return 0;
}

int L_dse_audio_stop_bgm(lua_State* L) {
    dse_audio_stop_bgm();
    return 0;
}

int L_dse_audio_play_sfx(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float volume = static_cast<float>(luaL_checknumber(L, 2));
    int loop = static_cast<int>(luaL_checkinteger(L, 3));
    dse_audio_play_sfx(path, volume, loop);
    return 0;
}

int L_dse_audio_crossfade_bgm(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float fade_sec = static_cast<float>(luaL_checknumber(L, 2));
    float volume = static_cast<float>(luaL_checknumber(L, 3));
    int loop = static_cast<int>(luaL_checkinteger(L, 4));
    int _ret = dse_audio_crossfade_bgm(path, fade_sec, volume, loop);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_audio_play_sfx_random(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float volume = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    float pitch_min = static_cast<float>(luaL_optnumber(L, 3, 0.9));
    float pitch_max = static_cast<float>(luaL_optnumber(L, 4, 1.1));
    dse_audio_play_sfx_random(path, volume, pitch_min, pitch_max);
    return 0;
}

int L_dse_audio_preload(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int _ret = dse_audio_preload(path);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_audio_stop_all_sfx(lua_State* L) {
    dse_audio_stop_all_sfx();
    return 0;
}

int L_dse_audio_set_master_volume(lua_State* L) {
    float volume = static_cast<float>(luaL_checknumber(L, 1));
    dse_audio_set_master_volume(volume);
    return 0;
}

int L_dse_audio_set_bgm_volume(lua_State* L) {
    float volume = static_cast<float>(luaL_checknumber(L, 1));
    dse_audio_set_bgm_volume(volume);
    return 0;
}

int L_dse_audio_set_sfx_volume(lua_State* L) {
    float volume = static_cast<float>(luaL_checknumber(L, 1));
    dse_audio_set_sfx_volume(volume);
    return 0;
}

int L_dse_audio_source_set_bus(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* bus_name = luaL_checkstring(L, 2);
    dse_audio_source_set_bus(e, bus_name);
    return 0;
}

int L_dse_audio_snapshot_save(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    int _ret = dse_audio_snapshot_save(name);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_audio_snapshot_load(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    int _ret = dse_audio_snapshot_load(name);
    lua_pushinteger(L, _ret);
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

void RegisterAudioBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "audio");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "audio");
    }
    helper::RegisterBindings(L, {
        {"add_source", L_dse_audio_source_add},
        {"set_playing", L_dse_audio_source_set_playing},
        {"set_volume", L_dse_audio_source_set_volume},
        {"set_pitch", L_dse_audio_source_set_pitch},
        {"set_spatial", L_dse_compat_audio_set_spatial},
        {"restart", L_dse_audio_source_restart},
        {"set_loop", L_dse_audio_source_set_loop},
        {"set_3d_mode", L_dse_audio_source_set_3d_mode},
        {"add_listener", L_dse_audio_listener_add},
        {"set_3d_distance", L_dse_audio_source_set_3d_distance},
        {"bus_set_volume", L_dse_audio_bus_set_volume},
        {"bus_set_muted", L_dse_audio_bus_set_muted},
        {"bus_create", L_dse_audio_bus_create},
        {"bus_remove", L_dse_audio_bus_remove},
        {"bus_add_effect", L_dse_audio_bus_add_effect},
        {"bus_remove_effect", L_dse_audio_bus_remove_effect},
        {"play_bgm", L_dse_audio_play_bgm},
        {"pause_bgm", L_dse_audio_pause_bgm},
        {"resume_bgm", L_dse_audio_resume_bgm},
        {"stop_bgm", L_dse_audio_stop_bgm},
        {"play_sfx", L_dse_audio_play_sfx},
        {"crossfade_bgm", L_dse_audio_crossfade_bgm},
        {"play_sfx_random", L_dse_audio_play_sfx_random},
        {"preload", L_dse_audio_preload},
        {"stop_all_sfx", L_dse_audio_stop_all_sfx},
        {"set_master_volume", L_dse_audio_set_master_volume},
        {"set_bgm_volume", L_dse_audio_set_bgm_volume},
        {"set_sfx_volume", L_dse_audio_set_sfx_volume},
        {"set_source_bus", L_dse_audio_source_set_bus},
        {"snapshot_save", L_dse_audio_snapshot_save},
        {"snapshot_load", L_dse_audio_snapshot_load},
        {"get_source_state", L_dse_audio_source_get_state_ex},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
