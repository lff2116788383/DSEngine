/**
 * @file lua_binding_audio.cpp
 * @brief 音频系统管理，封装底层音频库，提供音效和背景音乐的播放控制
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/assets/asset_manager.h"
#include "engine/audio/audio_system.h"
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

// 音频组件简单 setter — 使用宏替代手写样板
DSE_LUA_COMPONENT_SETTER(AudioLoop, AudioSourceComponent, loop, bool, helper::CheckBool(L, 2))
DSE_LUA_COMPONENT_SETTER(AudioVolume, AudioSourceComponent, volume, float, helper::CheckFloat(L, 2))
DSE_LUA_COMPONENT_SETTER(AudioPitch, AudioSourceComponent, pitch, float, std::max(0.01f, helper::CheckFloat(L, 2)))
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

// ============================================================
// 混音总线 + DSP 效果链 Lua API
// ============================================================

static dse::gameplay2d::AudioSystem* GetAudioSys() {
    const auto& ctx = GetBindingContext();
    return static_cast<dse::gameplay2d::AudioSystem*>(ctx.audio_system);
}

int L_BusSetVolume(lua_State* L) {
    auto* sys = GetAudioSys();
    if (!sys) { lua_pushboolean(L, 0); return 1; }
    const char* name = luaL_checkstring(L, 1);
    float vol = static_cast<float>(luaL_checknumber(L, 2));
    sys->GetBusManager().SetBusVolume(name, vol);
    lua_pushboolean(L, 1);
    return 1;
}

int L_BusSetMuted(lua_State* L) {
    auto* sys = GetAudioSys();
    if (!sys) { lua_pushboolean(L, 0); return 1; }
    const char* name = luaL_checkstring(L, 1);
    bool muted = lua_toboolean(L, 2) != 0;
    sys->GetBusManager().SetBusMuted(name, muted);
    lua_pushboolean(L, 1);
    return 1;
}

int L_BusCreate(lua_State* L) {
    auto* sys = GetAudioSys();
    if (!sys) { lua_pushboolean(L, 0); return 1; }
    const char* name = luaL_checkstring(L, 1);
    const char* parent = luaL_optstring(L, 2, "master");
    float vol = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    bool ok = sys->GetBusManager().CreateBus(name, parent, vol);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int L_BusRemove(lua_State* L) {
    auto* sys = GetAudioSys();
    if (!sys) { lua_pushboolean(L, 0); return 1; }
    const char* name = luaL_checkstring(L, 1);
    bool ok = sys->GetBusManager().RemoveBus(name);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int L_BusAddEffect(lua_State* L) {
    using dse::gameplay2d::DspEffectType;
    using dse::gameplay2d::DspEffectParams;
    auto* sys = GetAudioSys();
    if (!sys) { lua_pushboolean(L, 0); return 1; }
    const char* bus_name = luaL_checkstring(L, 1);
    int type_int = static_cast<int>(luaL_checkinteger(L, 2));
    DspEffectParams p;
    p.type = static_cast<DspEffectType>(std::clamp(type_int, 0, (int)DspEffectType::Count - 1));
    p.cutoff_hz = static_cast<float>(luaL_optnumber(L, 3, 1000.0));
    p.q = static_cast<float>(luaL_optnumber(L, 4, 0.707));
    p.delay_time_ms = static_cast<float>(luaL_optnumber(L, 5, 250.0));
    p.feedback = static_cast<float>(luaL_optnumber(L, 6, 0.3));
    p.wet_mix = static_cast<float>(luaL_optnumber(L, 7, 0.5));
    bool ok = sys->GetBusManager().AddEffect(bus_name, p);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int L_BusRemoveEffect(lua_State* L) {
    auto* sys = GetAudioSys();
    if (!sys) { lua_pushboolean(L, 0); return 1; }
    const char* bus_name = luaL_checkstring(L, 1);
    size_t index = static_cast<size_t>(luaL_checkinteger(L, 2));
    bool ok = sys->GetBusManager().RemoveEffect(bus_name, index);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

int L_BusGetNames(lua_State* L) {
    auto* sys = GetAudioSys();
    if (!sys) { lua_newtable(L); return 1; }
    auto names = sys->GetBusManager().GetBusNames();
    lua_createtable(L, static_cast<int>(names.size()), 0);
    for (size_t i = 0; i < names.size(); ++i) {
        lua_pushstring(L, names[i].c_str());
        lua_rawseti(L, -2, static_cast<int>(i + 1));
    }
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
