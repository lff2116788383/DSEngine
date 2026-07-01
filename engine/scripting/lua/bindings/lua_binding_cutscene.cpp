/**
 * @file lua_binding_cutscene.cpp
 * @brief 过场/导演系统 Lua 绑定
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
#include "engine/cutscene/cutscene_player.h"
#include "engine/cutscene/cutscene_track.h"

#include <unordered_map>
#include <memory>

extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

using namespace dse::cutscene;
using namespace helper;

struct LuaCutsceneInstance {
    CutscenePlayer player;
    // 存储每个 seq 的 camera track（方便查找）
    std::unordered_map<std::string, std::shared_ptr<CameraTrack>> camera_tracks;
    std::unordered_map<std::string, std::shared_ptr<EventTrack>> event_tracks;
    std::unordered_map<std::string, std::shared_ptr<AudioTrack>> audio_tracks;
    // property tracks 按 "seq_name:track_name" 索引
    std::unordered_map<std::string, std::shared_ptr<PropertyTrack>> property_tracks;
    // Lua 回调引用
    int finish_callback_ref = LUA_NOREF;
    lua_State* L = nullptr;
};

static std::unordered_map<int, std::unique_ptr<LuaCutsceneInstance>> s_cs_instances;
static int s_next_cs_id = 1;

// ============================================================
// Player management
// ============================================================

int L_CSCreatePlayer(lua_State* L) {
    int id = s_next_cs_id++;
    auto inst = std::make_unique<LuaCutsceneInstance>();
    inst->L = L;
    s_cs_instances[id] = std::move(inst);
    lua_pushinteger(L, id);
    return 1;
}

int L_CSDestroyPlayer(lua_State* L) {
    int id = CheckInt(L, 1);
    auto it = s_cs_instances.find(id);
    if (it != s_cs_instances.end()) {
        if (it->second->finish_callback_ref != LUA_NOREF) {
            luaL_unref(L, LUA_REGISTRYINDEX, it->second->finish_callback_ref);
        }
        s_cs_instances.erase(it);
    }
    return 0;
}

#define GET_CS_INST(id) \
    auto cs_it = s_cs_instances.find(id); \
    if (cs_it == s_cs_instances.end()) return 0; \
    auto& inst = *cs_it->second

// ============================================================
// Sequence management
// ============================================================

int L_CSAddSequence(lua_State* L) {
    int id = CheckInt(L, 1); GET_CS_INST(id);
    const char* name = luaL_checkstring(L, 2);
    float duration = CheckFloat(L, 3);
    auto seq = std::make_shared<CutsceneSequence>(name, duration);
    inst.player.AddSequence(seq);
    return 0;
}

int L_CSRemoveSequence(lua_State* L) {
    int id = CheckInt(L, 1); GET_CS_INST(id);
    const char* name = luaL_checkstring(L, 2);
    inst.player.RemoveSequence(name);
    inst.camera_tracks.erase(name);
    inst.event_tracks.erase(name);
    inst.audio_tracks.erase(name);
    return 0;
}

// ============================================================
// Track addition
// ============================================================

int L_CSAddCameraKeyframe(lua_State* L) {
    int id = CheckInt(L, 1); GET_CS_INST(id);
    const char* seq_name = luaL_checkstring(L, 2);
    float time = CheckFloat(L, 3);
    float px = CheckFloat(L, 4), py = CheckFloat(L, 5), pz = CheckFloat(L, 6);
    float lx = CheckFloat(L, 7), ly = CheckFloat(L, 8), lz = CheckFloat(L, 9);
    float fov = OptFloat(L, 10, 60.0f);

    auto seq = inst.player.GetSequence(seq_name);
    if (!seq) return 0;

    // 获取或创建该 seq 的 camera track
    auto& cam_track = inst.camera_tracks[seq_name];
    if (!cam_track) {
        cam_track = std::make_shared<CameraTrack>("Camera");
        seq->AddTrack(cam_track);
    }

    CameraKeyframe kf;
    kf.time = time;
    kf.position = glm::vec3(px, py, pz);
    kf.look_at = glm::vec3(lx, ly, lz);
    kf.fov = fov;
    cam_track->AddKeyframe(kf);
    return 0;
}

int L_CSAddPropertyKeyframe(lua_State* L) {
    int id = CheckInt(L, 1); GET_CS_INST(id);
    const char* seq_name = luaL_checkstring(L, 2);
    const char* track_name = luaL_checkstring(L, 3);
    float time = CheckFloat(L, 4);
    float value = CheckFloat(L, 5);
    const char* interp_str = luaL_optstring(L, 6, "linear");

    auto seq = inst.player.GetSequence(seq_name);
    if (!seq) return 0;

    std::string key = std::string(seq_name) + ":" + track_name;
    auto& prop_track = inst.property_tracks[key];
    if (!prop_track) {
        prop_track = std::make_shared<PropertyTrack>(track_name);
        seq->AddTrack(prop_track);
    }

    InterpMode interp = InterpMode::Linear;
    if (interp_str[0] == 's') interp = InterpMode::Step;
    else if (interp_str[0] == 'c') interp = InterpMode::CubicBezier;

    prop_track->AddKeyframe(time, value, interp);
    return 0;
}

int L_CSAddEvent(lua_State* L) {
    int id = CheckInt(L, 1); GET_CS_INST(id);
    const char* seq_name = luaL_checkstring(L, 2);
    float time = CheckFloat(L, 3);
    const char* event_name = luaL_checkstring(L, 4);
    const char* payload = luaL_optstring(L, 5, "");

    auto seq = inst.player.GetSequence(seq_name);
    if (!seq) return 0;

    auto& evt_track = inst.event_tracks[seq_name];
    if (!evt_track) {
        evt_track = std::make_shared<EventTrack>("Events");
        seq->AddTrack(evt_track);
    }

    evt_track->AddEvent(time, event_name, payload);
    return 0;
}

int L_CSAddAudioCue(lua_State* L) {
    int id = CheckInt(L, 1); GET_CS_INST(id);
    const char* seq_name = luaL_checkstring(L, 2);
    float time = CheckFloat(L, 3);
    const char* path = luaL_checkstring(L, 4);
    float volume = OptFloat(L, 5, 1.0f);
    bool loop = OptBool(L, 6, false);

    auto seq = inst.player.GetSequence(seq_name);
    if (!seq) return 0;

    auto& aud_track = inst.audio_tracks[seq_name];
    if (!aud_track) {
        aud_track = std::make_shared<AudioTrack>("Audio");
        seq->AddTrack(aud_track);
    }

    aud_track->AddCue(time, path, volume, loop);
    return 0;
}

// ============================================================
// Playback
// ============================================================

int L_CSPlay(lua_State* L) {
    int id = CheckInt(L, 1); GET_CS_INST(id);
    const char* name = luaL_checkstring(L, 2);
    inst.player.Play(name);
    return 0;
}

int L_CSPause(lua_State* L) {
    int id = CheckInt(L, 1); GET_CS_INST(id);
    inst.player.Pause();
    return 0;
}

int L_CSResume(lua_State* L) {
    int id = CheckInt(L, 1); GET_CS_INST(id);
    inst.player.Resume();
    return 0;
}

int L_CSStop(lua_State* L) {
    int id = CheckInt(L, 1); GET_CS_INST(id);
    inst.player.Stop();
    return 0;
}

int L_CSSeek(lua_State* L) {
    int id = CheckInt(L, 1); GET_CS_INST(id);
    float time = CheckFloat(L, 2);
    inst.player.Seek(time);
    return 0;
}

int L_CSGetTime(lua_State* L) {
    int id = CheckInt(L, 1); GET_CS_INST(id);
    lua_pushnumber(L, inst.player.GetCurrentTime());
    return 1;
}

int L_CSGetState(lua_State* L) {
    int id = CheckInt(L, 1); GET_CS_INST(id);
    switch (inst.player.GetState()) {
        case PlayState::Playing: lua_pushstring(L, "playing"); break;
        case PlayState::Paused:  lua_pushstring(L, "paused"); break;
        default:                 lua_pushstring(L, "stopped"); break;
    }
    return 1;
}

int L_CSSetPlayRate(lua_State* L) {
    int id = CheckInt(L, 1); GET_CS_INST(id);
    inst.player.SetPlayRate(CheckFloat(L, 2));
    return 0;
}

int L_CSUpdate(lua_State* L) {
    int id = CheckInt(L, 1); GET_CS_INST(id);
    float dt = CheckFloat(L, 2);
    inst.player.Update(dt);
    return 0;
}

// ============================================================
// Callbacks
// ============================================================

int L_CSSetCameraCallback(lua_State* L) {
    int id = CheckInt(L, 1); GET_CS_INST(id);
    const char* seq_name = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    auto cam_it = inst.camera_tracks.find(seq_name);
    if (cam_it != inst.camera_tracks.end() && cam_it->second) {
        lua_State* LL = L;
        cam_it->second->SetApplyCallback([LL, ref](const glm::vec3& pos, const glm::vec3& look, float fov) {
            lua_rawgeti(LL, LUA_REGISTRYINDEX, ref);
            lua_pushnumber(LL, pos.x); lua_pushnumber(LL, pos.y); lua_pushnumber(LL, pos.z);
            lua_pushnumber(LL, look.x); lua_pushnumber(LL, look.y); lua_pushnumber(LL, look.z);
            lua_pushnumber(LL, fov);
            lua_pcall(LL, 7, 0, 0);
        });
    }
    return 0;
}

int L_CSSetEventCallback(lua_State* L) {
    int id = CheckInt(L, 1); GET_CS_INST(id);
    const char* seq_name = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    lua_pushvalue(L, 3);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    auto evt_it = inst.event_tracks.find(seq_name);
    if (evt_it != inst.event_tracks.end() && evt_it->second) {
        lua_State* LL = L;
        evt_it->second->SetFireCallback([LL, ref](const std::string& name, const std::string& payload) {
            lua_rawgeti(LL, LUA_REGISTRYINDEX, ref);
            lua_pushstring(LL, name.c_str());
            lua_pushstring(LL, payload.c_str());
            lua_pcall(LL, 2, 0, 0);
        });
    }
    return 0;
}

int L_CSSetFinishCallback(lua_State* L) {
    int id = CheckInt(L, 1); GET_CS_INST(id);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_pushvalue(L, 2);

    if (inst.finish_callback_ref != LUA_NOREF) {
        luaL_unref(L, LUA_REGISTRYINDEX, inst.finish_callback_ref);
    }
    inst.finish_callback_ref = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_State* LL = L;
    int ref = inst.finish_callback_ref;
    inst.player.SetFinishCallback([LL, ref](const std::string& seq_name) {
        lua_rawgeti(LL, LUA_REGISTRYINDEX, ref);
        lua_pushstring(LL, seq_name.c_str());
        lua_pcall(LL, 1, 0, 0);
    });
    return 0;
}

} // namespace

void ShutdownCutsceneBindings() {
    s_cs_instances.clear();
    s_next_cs_id = 1;
}

void RegisterCutsceneBindings(lua_State* L) {
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
