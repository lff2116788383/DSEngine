/**
 * @file lua_binding_audio.cpp
 * @brief 音频系统管理，封装底层音频库，提供音效和背景音乐的播放控制
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/assets/asset_manager.h"
#include "engine/ecs/audio.h"
#include <algorithm>
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
int L_AudioSet3DMode(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    bool enabled = lua_toboolean(L, 2) != 0;
    if (world->registry().valid(e)) {
        auto& audio = world->registry().get_or_emplace<AudioSourceComponent>(e);
        audio.spatial_enabled = enabled;
    }
    return 0;
}

int L_AudioAddListener(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (world->registry().valid(e)) {
        auto& listener = world->registry().emplace_or_replace<AudioListenerComponent>(e);
        listener.enabled = true;
        listener.listener_index = 0;
    }
    return 0;
}

int L_AudioSet3DDistance(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        return 0;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    float min_distance = std::max(0.01f, static_cast<float>(luaL_optnumber(L, 2, 1.0)));
    float max_distance = std::max(min_distance, static_cast<float>(luaL_optnumber(L, 3, 20.0)));
    float rolloff = std::max(0.0f, static_cast<float>(luaL_optnumber(L, 4, 1.0)));
    if (world->registry().valid(e)) {
        auto& audio = world->registry().get_or_emplace<AudioSourceComponent>(e);
        audio.min_distance = min_distance;
        audio.max_distance = max_distance;
        audio.rolloff = rolloff;
    }
    return 0;
}

int L_AudioGetSourceState(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (!world->registry().valid(e) || !world->registry().all_of<AudioSourceComponent>(e)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const auto& audio = world->registry().get<AudioSourceComponent>(e);
    lua_pushboolean(L, 1);
    lua_pushboolean(L, audio.clip != nullptr);
    lua_pushboolean(L, audio.is_playing);
    lua_pushboolean(L, audio.spatial_enabled);
    lua_pushnumber(L, audio.min_distance);
    lua_pushnumber(L, audio.max_distance);
    lua_pushnumber(L, audio.rolloff);
    lua_pushnumber(L, audio.volume);
    lua_pushnumber(L, audio.pitch);
    lua_pushinteger(L, static_cast<lua_Integer>(audio.runtime_handle));
    lua_pushinteger(L, audio.clip ? static_cast<lua_Integer>(audio.clip->GetData().size()) : 0);
    lua_pushstring(L, audio.clip ? audio.clip->GetPath().c_str() : "");
    return 12;
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
    set_fn("set_3d_mode", L_AudioSet3DMode);
    set_fn("add_listener", L_AudioAddListener);
    set_fn("set_3d_distance", L_AudioSet3DDistance);
    set_fn("get_source_state", L_AudioGetSourceState);
}

}
