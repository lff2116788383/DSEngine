/**
 * @file lua_binding_audio.cpp
 * @brief 音频系统管理，封装底层音频库，提供音效和背景音乐的播放控制
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/assets/asset_manager.h"
#include "engine/ecs/audio.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {
int L_AudioAddSource(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    bool play_on_awake = lua_toboolean(L, 3);
    bool loop = lua_toboolean(L, 4);
    float volume = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    auto& audio = world->registry().emplace_or_replace<AudioSourceComponent>(e);
    audio.clip = GetAssetManager().LoadAudioClip(path);
    audio.play_on_awake = play_on_awake;
    audio.loop = loop;
    audio.volume = volume;
    return 0;
}

int L_AudioSetPlaying(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    bool playing = lua_toboolean(L, 2);
    if (world->registry().valid(e) && world->registry().all_of<AudioSourceComponent>(e)) {
        auto& audio = world->registry().get<AudioSourceComponent>(e);
        audio.is_playing = playing;
        if (!playing) {
            audio.restart_requested = false;
        }
    }
    return 0;
}

int L_AudioRestart(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (world->registry().valid(e) && world->registry().all_of<AudioSourceComponent>(e)) {
        auto& audio = world->registry().get<AudioSourceComponent>(e);
        audio.is_playing = true;
        audio.restart_requested = true;
    }
    return 0;
}

int L_AudioSetLoop(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    bool loop = lua_toboolean(L, 2);
    if (world->registry().valid(e) && world->registry().all_of<AudioSourceComponent>(e)) {
        auto& audio = world->registry().get<AudioSourceComponent>(e);
        audio.loop = loop;
    }
    return 0;
}

int L_AudioSetVolume(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float volume = static_cast<float>(luaL_checknumber(L, 2));
    if (world->registry().valid(e) && world->registry().all_of<AudioSourceComponent>(e)) {
        auto& audio = world->registry().get<AudioSourceComponent>(e);
        audio.volume = volume;
    }
    return 0;
}

int L_AudioSetPitch(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float pitch = static_cast<float>(luaL_checknumber(L, 2));
    if (pitch <= 0.01f) {
        pitch = 0.01f;
    }
    if (world->registry().valid(e) && world->registry().all_of<AudioSourceComponent>(e)) {
        auto& audio = world->registry().get<AudioSourceComponent>(e);
        audio.pitch = pitch;
    }
    return 0;
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
    set_fn("set_loop", L_AudioSetLoop);
    set_fn("set_volume", L_AudioSetVolume);
    set_fn("set_pitch", L_AudioSetPitch);
}

}
