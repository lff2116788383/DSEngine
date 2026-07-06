/**
 * @file lua_binding_audio.cpp
 * @brief 音频系统管理，封装底层音频库，提供音效和背景音乐的播放控制。薄包装委托至 C ABI。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

inline uint32_t EID(Entity e) { return static_cast<uint32_t>(static_cast<entt::id_type>(e)); }

int L_AudioAddSource(lua_State* L) {
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    int play_on_awake = lua_toboolean(L, 3);
    int loop = lua_toboolean(L, 4);
    float volume = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    dse_audio_source_add(EID(e), path, play_on_awake, loop, volume);
    return 0;
}

int L_AudioSetPlaying(lua_State* L) {
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    dse_audio_source_set_playing(EID(e), lua_toboolean(L, 2));
    return 0;
}

int L_AudioRestart(lua_State* L) {
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    dse_audio_source_restart(EID(e));
    return 0;
}

int L_EcsSetAudioLoop(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_audio_source_set_loop(EID(e), helper::CheckBool(L, 2) ? 1 : 0);
    return 0;
}

int L_EcsSetAudioVolume(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_audio_source_set_volume(EID(e), helper::CheckFloat(L, 2));
    return 0;
}

int L_EcsSetAudioPitch(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    dse_audio_source_set_pitch(EID(e), helper::CheckFloat(L, 2));
    return 0;
}

int L_AudioSet3DMode(lua_State* L) {
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    dse_audio_source_set_3d_mode(EID(e), lua_toboolean(L, 2) != 0 ? 1 : 0);
    return 0;
}

int L_AudioAddListener(lua_State* L) {
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    dse_audio_listener_add(EID(e), 1);
    return 0;
}

int L_AudioSet3DDistance(lua_State* L) {
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float min_distance = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    float max_distance = static_cast<float>(luaL_optnumber(L, 3, 20.0));
    float rolloff = static_cast<float>(luaL_optnumber(L, 4, 1.0));
    dse_audio_source_set_3d_distance(EID(e), min_distance, max_distance, rolloff);
    return 0;
}

int L_AudioGetSourceState(lua_State* L) {
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    int flags[3] = {0, 0, 0};
    float params[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    long long runtime_handle = 0;
    long long clip_size = 0;
    char path[512] = {0};
    if (!dse_audio_source_get_state(EID(e), flags, params, &runtime_handle, &clip_size,
                                    path, static_cast<int>(sizeof(path)))) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, 1);
    lua_pushboolean(L, flags[0]);
    lua_pushboolean(L, flags[1]);
    lua_pushboolean(L, flags[2]);
    lua_pushnumber(L, params[0]);
    lua_pushnumber(L, params[1]);
    lua_pushnumber(L, params[2]);
    lua_pushnumber(L, params[3]);
    lua_pushnumber(L, params[4]);
    lua_pushinteger(L, static_cast<lua_Integer>(runtime_handle));
    lua_pushinteger(L, static_cast<lua_Integer>(clip_size));
    lua_pushstring(L, path);
    return 12;
}

// ============================================================
// 混音总线 + DSP 效果链 Lua API
// ============================================================

int L_BusSetVolume(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    float vol = static_cast<float>(luaL_checknumber(L, 2));
    lua_pushboolean(L, dse_audio_bus_set_volume(name, vol));
    return 1;
}

int L_BusSetMuted(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_pushboolean(L, dse_audio_bus_set_muted(name, lua_toboolean(L, 2) != 0 ? 1 : 0));
    return 1;
}

int L_BusCreate(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    const char* parent = luaL_optstring(L, 2, "master");
    float vol = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    lua_pushboolean(L, dse_audio_bus_create(name, parent, vol));
    return 1;
}

int L_BusRemove(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_pushboolean(L, dse_audio_bus_remove(name));
    return 1;
}

int L_BusAddEffect(lua_State* L) {
    const char* bus_name = luaL_checkstring(L, 1);
    int type = static_cast<int>(luaL_checkinteger(L, 2));
    float cutoff_hz = static_cast<float>(luaL_optnumber(L, 3, 1000.0));
    float q = static_cast<float>(luaL_optnumber(L, 4, 0.707));
    float delay_time_ms = static_cast<float>(luaL_optnumber(L, 5, 250.0));
    float feedback = static_cast<float>(luaL_optnumber(L, 6, 0.3));
    float wet_mix = static_cast<float>(luaL_optnumber(L, 7, 0.5));
    float room_size = static_cast<float>(luaL_optnumber(L, 8, 0.5));
    float damping = static_cast<float>(luaL_optnumber(L, 9, 0.5));
    lua_pushboolean(L, dse_audio_bus_add_effect(bus_name, type, cutoff_hz, q, delay_time_ms,
                                                feedback, wet_mix, room_size, damping));
    return 1;
}

int L_BusRemoveEffect(lua_State* L) {
    const char* bus_name = luaL_checkstring(L, 1);
    int index = static_cast<int>(luaL_checkinteger(L, 2));
    lua_pushboolean(L, dse_audio_bus_remove_effect(bus_name, index));
    return 1;
}

// ============================================================
// 全局 BGM / SFX Lua API
// ============================================================

// audio.play_bgm(filepath, [volume, loop])
int L_AudioPlayBgm(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float vol = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    int loop = lua_isnoneornil(L, 3) ? 1 : (lua_toboolean(L, 3) != 0 ? 1 : 0);
    lua_pushboolean(L, dse_audio_play_bgm(path, vol, loop));
    return 1;
}

// audio.pause_bgm()
int L_AudioPauseBgm(lua_State*) {
    dse_audio_pause_bgm();
    return 0;
}

// audio.resume_bgm()
int L_AudioResumeBgm(lua_State*) {
    dse_audio_resume_bgm();
    return 0;
}

// audio.stop_bgm()
int L_AudioStopBgm(lua_State*) {
    dse_audio_stop_bgm();
    return 0;
}

// audio.play_sfx(filepath, [volume, loop])
int L_AudioPlaySfx(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float vol = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    int loop = lua_isnoneornil(L, 3) ? 0 : (lua_toboolean(L, 3) != 0 ? 1 : 0);
    dse_audio_play_sfx(path, vol, loop);
    return 0;
}

// audio.crossfade_bgm(filepath, fade_sec, [volume, loop])
int L_AudioCrossfadeBgm(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float fade = static_cast<float>(luaL_checknumber(L, 2));
    float vol = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    int loop = lua_isnoneornil(L, 4) ? 1 : (lua_toboolean(L, 4) != 0 ? 1 : 0);
    lua_pushboolean(L, dse_audio_crossfade_bgm(path, fade, vol, loop));
    return 1;
}

// audio.play_sfx_random(filepath, [volume, pitch_min, pitch_max])
int L_AudioPlaySfxRandom(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float vol = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    float pmin = static_cast<float>(luaL_optnumber(L, 3, 0.9));
    float pmax = static_cast<float>(luaL_optnumber(L, 4, 1.1));
    dse_audio_play_sfx_random(path, vol, pmin, pmax);
    return 0;
}

// audio.preload(filepath)
int L_AudioPreload(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    lua_pushboolean(L, dse_audio_preload(path));
    return 1;
}

// audio.stop_all_sfx()
int L_AudioStopAllSfx(lua_State*) {
    dse_audio_stop_all_sfx();
    return 0;
}

// audio.set_master_volume(volume)
int L_AudioSetMasterVolume(lua_State* L) {
    dse_audio_set_master_volume(static_cast<float>(luaL_checknumber(L, 1)));
    return 0;
}

// audio.set_bgm_volume(volume)
int L_AudioSetBgmVolume(lua_State* L) {
    dse_audio_set_bgm_volume(static_cast<float>(luaL_checknumber(L, 1)));
    return 0;
}

// audio.set_sfx_volume(volume)
int L_AudioSetSfxVolume(lua_State* L) {
    dse_audio_set_sfx_volume(static_cast<float>(luaL_checknumber(L, 1)));
    return 0;
}

// audio.set_source_bus(entity, bus_name)
int L_AudioSetSourceBus(lua_State* L) {
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* bus = luaL_checkstring(L, 2);
    dse_audio_source_set_bus(EID(e), bus);
    return 0;
}

// audio.snapshot_save(name)
int L_AudioSnapshotSave(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_pushboolean(L, dse_audio_snapshot_save(name));
    return 1;
}

// audio.snapshot_load(name)
int L_AudioSnapshotLoad(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    lua_pushboolean(L, dse_audio_snapshot_load(name));
    return 1;
}

// '\n' 分隔的名称列表转 Lua 数组表
void PushNameList(lua_State* L, const char* joined, int len) {
    lua_newtable(L);
    if (len <= 0) return;
    int index = 1;
    int start = 0;
    for (int i = 0; i <= len; ++i) {
        if (i == len || joined[i] == '\n') {
            if (i > start) {
                lua_pushlstring(L, joined + start, static_cast<size_t>(i - start));
                lua_rawseti(L, -2, index++);
            }
            start = i + 1;
        }
    }
}

// audio.snapshot_list() -> table
int L_AudioSnapshotList(lua_State* L) {
    char buf[4096] = {0};
    const int len = dse_audio_snapshot_list(buf, static_cast<int>(sizeof(buf)));
    PushNameList(L, buf, len);
    return 1;
}

int L_BusGetNames(lua_State* L) {
    char buf[4096] = {0};
    const int len = dse_audio_bus_get_names(buf, static_cast<int>(sizeof(buf)));
    PushNameList(L, buf, len);
    return 1;
}
}

void RegisterAudioBindings(lua_State* L) {
    auto set_fn = [L](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    };

    lua_newtable(L);
    set_fn("add_source", L_AudioAddSource);
    set_fn("set_playing", L_AudioSetPlaying);
    set_fn("restart", L_AudioRestart);
    set_fn("set_loop", L_EcsSetAudioLoop);
    set_fn("set_volume", L_EcsSetAudioVolume);
    set_fn("set_pitch", L_EcsSetAudioPitch);
    set_fn("set_3d_mode", L_AudioSet3DMode);
    set_fn("add_listener", L_AudioAddListener);
    set_fn("set_3d_distance", L_AudioSet3DDistance);
    set_fn("get_source_state", L_AudioGetSourceState);

    // 全局 BGM / SFX
    set_fn("play_bgm", L_AudioPlayBgm);
    set_fn("pause_bgm", L_AudioPauseBgm);
    set_fn("resume_bgm", L_AudioResumeBgm);
    set_fn("stop_bgm", L_AudioStopBgm);
    set_fn("crossfade_bgm", L_AudioCrossfadeBgm);
    set_fn("play_sfx", L_AudioPlaySfx);
    set_fn("play_sfx_random", L_AudioPlaySfxRandom);
    set_fn("stop_all_sfx", L_AudioStopAllSfx);
    set_fn("preload", L_AudioPreload);

    // 音量控制
    set_fn("set_master_volume", L_AudioSetMasterVolume);
    set_fn("set_bgm_volume", L_AudioSetBgmVolume);
    set_fn("set_sfx_volume", L_AudioSetSfxVolume);

    // AudioSource 总线路由
    set_fn("set_source_bus", L_AudioSetSourceBus);

    // 快照系统
    set_fn("snapshot_save", L_AudioSnapshotSave);
    set_fn("snapshot_load", L_AudioSnapshotLoad);
    set_fn("snapshot_list", L_AudioSnapshotList);

    // 混音总线 + DSP 效果链
    set_fn("bus_set_volume", L_BusSetVolume);
    set_fn("bus_set_muted", L_BusSetMuted);
    set_fn("bus_create", L_BusCreate);
    set_fn("bus_remove", L_BusRemove);
    set_fn("bus_add_effect", L_BusAddEffect);
    set_fn("bus_remove_effect", L_BusRemoveEffect);
    set_fn("bus_get_names", L_BusGetNames);
}

}
