/**
 * @file lua_binding_cutscene.cpp
 * @brief 过场/导演系统 Lua 绑定。薄包装委托至 C ABI。
 *
 * 全局表 `cutscene`:
 *   cutscene.create_player()                      → player_id
 *   cutscene.destroy_player(player_id)
 *
 *   -- Sequence 管理
 *   cutscene.add_sequence(player_id, name, duration)
 *   cutscene.remove_sequence(player_id, name)
 *
 *   -- 轨道添加
 *   cutscene.add_camera_keyframe(player_id, seq_name, time, px,py,pz, lx,ly,lz, fov)
 *   cutscene.add_property_keyframe(player_id, seq_name, track_name, time, value, interp)
 *   cutscene.add_event(player_id, seq_name, time, event_name, payload)
 *   cutscene.add_audio_cue(player_id, seq_name, time, path, volume, loop)
 *
 *   -- 播放控制
 *   cutscene.play(player_id, seq_name)
 *   cutscene.pause(player_id)
 *   cutscene.resume(player_id)
 *   cutscene.stop(player_id)
 *   cutscene.seek(player_id, time)
 *   cutscene.get_time(player_id)                  → float
 *   cutscene.get_state(player_id)                 → "stopped"|"playing"|"paused"
 *   cutscene.set_play_rate(player_id, rate)
 *   cutscene.update(player_id, dt)
 *
 *   -- 回调设置
 *   cutscene.set_camera_callback(player_id, seq_name, lua_func)  lua_func(px,py,pz,lx,ly,lz,fov)
 *   cutscene.set_event_callback(player_id, seq_name, lua_func)   lua_func(event_name, payload)
 *   cutscene.set_finish_callback(player_id, lua_func)            lua_func(seq_name)
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"

#include <unordered_map>
#include <memory>
#include <vector>

extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

using namespace helper;

// 回调上下文：由本层持有，player 销毁时释放对应的 registry ref。
struct CSCallbackCtx {
    lua_State* L = nullptr;
    int func_ref = LUA_NOREF;
};

// 按 player_id 记录所有回调上下文，便于 destroy 时统一释放。
std::unordered_map<int, std::vector<std::unique_ptr<CSCallbackCtx>>> s_cs_callbacks;

CSCallbackCtx* MakeCallbackCtx(lua_State* L, int player_id, int func_index) {
    lua_pushvalue(L, func_index);
    auto ctx = std::make_unique<CSCallbackCtx>();
    ctx->L = L;
    ctx->func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
    CSCallbackCtx* raw = ctx.get();
    s_cs_callbacks[player_id].push_back(std::move(ctx));
    return raw;
}

void ReleaseCallbacks(int player_id) {
    auto it = s_cs_callbacks.find(player_id);
    if (it == s_cs_callbacks.end()) return;
    for (auto& ctx : it->second) {
        if (ctx->L && ctx->func_ref != LUA_NOREF) {
            luaL_unref(ctx->L, LUA_REGISTRYINDEX, ctx->func_ref);
        }
    }
    s_cs_callbacks.erase(it);
}

extern "C" void CSCameraTrampoline(float px, float py, float pz,
                                   float lx, float ly, float lz,
                                   float fov, void* user_data) {
    auto* ctx = static_cast<CSCallbackCtx*>(user_data);
    if (!ctx || !ctx->L || ctx->func_ref == LUA_NOREF) return;
    lua_State* L = ctx->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ctx->func_ref);
    lua_pushnumber(L, px); lua_pushnumber(L, py); lua_pushnumber(L, pz);
    lua_pushnumber(L, lx); lua_pushnumber(L, ly); lua_pushnumber(L, lz);
    lua_pushnumber(L, fov);
    if (lua_pcall(L, 7, 0, 0) != LUA_OK) lua_pop(L, 1);
}

extern "C" void CSEventTrampoline(const char* event_name, const char* payload, void* user_data) {
    auto* ctx = static_cast<CSCallbackCtx*>(user_data);
    if (!ctx || !ctx->L || ctx->func_ref == LUA_NOREF) return;
    lua_State* L = ctx->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ctx->func_ref);
    lua_pushstring(L, event_name ? event_name : "");
    lua_pushstring(L, payload ? payload : "");
    if (lua_pcall(L, 2, 0, 0) != LUA_OK) lua_pop(L, 1);
}

extern "C" void CSFinishTrampoline(const char* seq_name, void* user_data) {
    auto* ctx = static_cast<CSCallbackCtx*>(user_data);
    if (!ctx || !ctx->L || ctx->func_ref == LUA_NOREF) return;
    lua_State* L = ctx->L;
    lua_rawgeti(L, LUA_REGISTRYINDEX, ctx->func_ref);
    lua_pushstring(L, seq_name ? seq_name : "");
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) lua_pop(L, 1);
}

// ============================================================
// Player management
// ============================================================

int L_CSCreatePlayer(lua_State* L) {
    lua_pushinteger(L, dse_cutscene_create());
    return 1;
}

int L_CSDestroyPlayer(lua_State* L) {
    int id = CheckInt(L, 1);
    dse_cutscene_destroy(id);
    ReleaseCallbacks(id);
    return 0;
}

// ============================================================
// Sequence management
// ============================================================

int L_CSAddSequence(lua_State* L) {
    dse_cutscene_add_sequence(CheckInt(L, 1), luaL_checkstring(L, 2), CheckFloat(L, 3));
    return 0;
}

int L_CSRemoveSequence(lua_State* L) {
    dse_cutscene_remove_sequence(CheckInt(L, 1), luaL_checkstring(L, 2));
    return 0;
}

// ============================================================
// Track addition
// ============================================================

int L_CSAddCameraKeyframe(lua_State* L) {
    dse_cutscene_add_camera_keyframe(CheckInt(L, 1), luaL_checkstring(L, 2), CheckFloat(L, 3),
                                     CheckFloat(L, 4), CheckFloat(L, 5), CheckFloat(L, 6),
                                     CheckFloat(L, 7), CheckFloat(L, 8), CheckFloat(L, 9),
                                     OptFloat(L, 10, 60.0f));
    return 0;
}

int L_CSAddPropertyKeyframe(lua_State* L) {
    const char* interp_str = luaL_optstring(L, 6, "linear");
    int interp = 0;
    if (interp_str[0] == 's') interp = 1;
    else if (interp_str[0] == 'c') interp = 2;
    dse_cutscene_add_property_keyframe(CheckInt(L, 1), luaL_checkstring(L, 2),
                                       luaL_checkstring(L, 3), CheckFloat(L, 4),
                                       CheckFloat(L, 5), interp);
    return 0;
}

int L_CSAddEvent(lua_State* L) {
    dse_cutscene_add_event(CheckInt(L, 1), luaL_checkstring(L, 2), CheckFloat(L, 3),
                           luaL_checkstring(L, 4), luaL_optstring(L, 5, ""));
    return 0;
}

int L_CSAddAudioCue(lua_State* L) {
    dse_cutscene_add_audio_cue(CheckInt(L, 1), luaL_checkstring(L, 2), CheckFloat(L, 3),
                               luaL_checkstring(L, 4), OptFloat(L, 5, 1.0f),
                               OptBool(L, 6, false) ? 1 : 0);
    return 0;
}

// ============================================================
// Playback
// ============================================================

int L_CSPlay(lua_State* L) {
    dse_cutscene_play(CheckInt(L, 1), luaL_checkstring(L, 2));
    return 0;
}

int L_CSPause(lua_State* L) {
    dse_cutscene_pause(CheckInt(L, 1));
    return 0;
}

int L_CSResume(lua_State* L) {
    dse_cutscene_resume(CheckInt(L, 1));
    return 0;
}

int L_CSStop(lua_State* L) {
    dse_cutscene_stop(CheckInt(L, 1));
    return 0;
}

int L_CSSeek(lua_State* L) {
    dse_cutscene_seek(CheckInt(L, 1), CheckFloat(L, 2));
    return 0;
}

int L_CSGetTime(lua_State* L) {
    lua_pushnumber(L, dse_cutscene_get_time(CheckInt(L, 1)));
    return 1;
}

int L_CSGetState(lua_State* L) {
    switch (dse_cutscene_get_state(CheckInt(L, 1))) {
        case 1: lua_pushstring(L, "playing"); break;
        case 2: lua_pushstring(L, "paused"); break;
        default: lua_pushstring(L, "stopped"); break;
    }
    return 1;
}

int L_CSSetPlayRate(lua_State* L) {
    dse_cutscene_set_play_rate(CheckInt(L, 1), CheckFloat(L, 2));
    return 0;
}

int L_CSUpdate(lua_State* L) {
    dse_cutscene_update(CheckInt(L, 1), CheckFloat(L, 2));
    return 0;
}

// ============================================================
// Callbacks
// ============================================================

int L_CSSetCameraCallback(lua_State* L) {
    int id = CheckInt(L, 1);
    const char* seq_name = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    dse_cutscene_set_camera_callback(id, seq_name, CSCameraTrampoline,
                                     MakeCallbackCtx(L, id, 3));
    return 0;
}

int L_CSSetEventCallback(lua_State* L) {
    int id = CheckInt(L, 1);
    const char* seq_name = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    dse_cutscene_set_event_callback(id, seq_name, CSEventTrampoline,
                                    MakeCallbackCtx(L, id, 3));
    return 0;
}

int L_CSSetFinishCallback(lua_State* L) {
    int id = CheckInt(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    dse_cutscene_set_finish_callback(id, CSFinishTrampoline, MakeCallbackCtx(L, id, 2));
    return 0;
}

} // namespace

void ShutdownCutsceneBindings() {
    dse_cutscene_shutdown();
    s_cs_callbacks.clear();
}

void RegisterCutsceneBindings(lua_State* L) {
    static bool registered = false;
    if (!registered) {
        BindingCleanupRegistry::Instance().Register(ShutdownCutsceneBindings);
        registered = true;
    }
    lua_newtable(L);

    // Player management
    RegisterFn(L, "create_player", L_CSCreatePlayer);
    RegisterFn(L, "destroy_player", L_CSDestroyPlayer);

    // Sequence
    RegisterFn(L, "add_sequence", L_CSAddSequence);
    RegisterFn(L, "remove_sequence", L_CSRemoveSequence);

    // Tracks
    RegisterFn(L, "add_camera_keyframe", L_CSAddCameraKeyframe);
    RegisterFn(L, "add_property_keyframe", L_CSAddPropertyKeyframe);
    RegisterFn(L, "add_event", L_CSAddEvent);
    RegisterFn(L, "add_audio_cue", L_CSAddAudioCue);

    // Playback
    RegisterFn(L, "play", L_CSPlay);
    RegisterFn(L, "pause", L_CSPause);
    RegisterFn(L, "resume", L_CSResume);
    RegisterFn(L, "stop", L_CSStop);
    RegisterFn(L, "seek", L_CSSeek);
    RegisterFn(L, "get_time", L_CSGetTime);
    RegisterFn(L, "get_state", L_CSGetState);
    RegisterFn(L, "set_play_rate", L_CSSetPlayRate);
    RegisterFn(L, "update", L_CSUpdate);

    // Callbacks
    RegisterFn(L, "set_camera_callback", L_CSSetCameraCallback);
    RegisterFn(L, "set_event_callback", L_CSSetEventCallback);
    RegisterFn(L, "set_finish_callback", L_CSSetFinishCallback);

    lua_setglobal(L, "cutscene");
}

} // namespace dse::runtime::lua_binding
