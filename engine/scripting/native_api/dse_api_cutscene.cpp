/**
 * @file dse_api_cutscene.cpp
 * @brief DSEngine Native C ABI — 过场/导演系统（手写）
 *
 * 播放器实例由本层管理（int 句柄）。相机/事件/完成回调通过
 * C 函数指针 + user_data 传出，脚本层自行持有闭包上下文。
 */

#include "engine/scripting/native_api/dse_api.h"

#include "engine/cutscene/cutscene_player.h"
#include "engine/cutscene/cutscene_track.h"

#include <memory>
#include <string>
#include <unordered_map>

using namespace dse::cutscene;

namespace {

struct CSInstance {
    CutscenePlayer player;
    std::unordered_map<std::string, std::shared_ptr<CameraTrack>> camera_tracks;
    std::unordered_map<std::string, std::shared_ptr<EventTrack>> event_tracks;
    std::unordered_map<std::string, std::shared_ptr<AudioTrack>> audio_tracks;
    // property tracks 按 "seq_name:track_name" 索引
    std::unordered_map<std::string, std::shared_ptr<PropertyTrack>> property_tracks;
};

std::unordered_map<int, std::unique_ptr<CSInstance>> s_cs_instances;
int s_next_cs_id = 1;

CSInstance* GetCS(int id) {
    auto it = s_cs_instances.find(id);
    return it != s_cs_instances.end() ? it->second.get() : nullptr;
}

}  // namespace

extern "C" int dse_cutscene_create(void) {
    int id = s_next_cs_id++;
    s_cs_instances[id] = std::make_unique<CSInstance>();
    return id;
}

extern "C" void dse_cutscene_destroy(int player_id) {
    s_cs_instances.erase(player_id);
}

extern "C" void dse_cutscene_shutdown(void) {
    s_cs_instances.clear();
    s_next_cs_id = 1;
}

extern "C" void dse_cutscene_add_sequence(int player_id, const char* name, float duration) {
    auto* inst = GetCS(player_id);
    if (!inst || !name) return;
    inst->player.AddSequence(std::make_shared<CutsceneSequence>(name, duration));
}

extern "C" void dse_cutscene_remove_sequence(int player_id, const char* name) {
    auto* inst = GetCS(player_id);
    if (!inst || !name) return;
    inst->player.RemoveSequence(name);
    inst->camera_tracks.erase(name);
    inst->event_tracks.erase(name);
    inst->audio_tracks.erase(name);
}

extern "C" void dse_cutscene_add_camera_keyframe(int player_id, const char* seq_name, float time,
                                                 float px, float py, float pz,
                                                 float lx, float ly, float lz, float fov) {
    auto* inst = GetCS(player_id);
    if (!inst || !seq_name) return;
    auto seq = inst->player.GetSequence(seq_name);
    if (!seq) return;

    auto& cam_track = inst->camera_tracks[seq_name];
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
}

extern "C" void dse_cutscene_add_property_keyframe(int player_id, const char* seq_name,
                                                   const char* track_name, float time,
                                                   float value, int interp) {
    auto* inst = GetCS(player_id);
    if (!inst || !seq_name || !track_name) return;
    auto seq = inst->player.GetSequence(seq_name);
    if (!seq) return;

    const std::string key = std::string(seq_name) + ":" + track_name;
    auto& prop_track = inst->property_tracks[key];
    if (!prop_track) {
        prop_track = std::make_shared<PropertyTrack>(track_name);
        seq->AddTrack(prop_track);
    }

    InterpMode mode = InterpMode::Linear;
    if (interp == 1) mode = InterpMode::Step;
    else if (interp == 2) mode = InterpMode::CubicBezier;
    prop_track->AddKeyframe(time, value, mode);
}

extern "C" void dse_cutscene_add_event(int player_id, const char* seq_name, float time,
                                       const char* event_name, const char* payload) {
    auto* inst = GetCS(player_id);
    if (!inst || !seq_name || !event_name) return;
    auto seq = inst->player.GetSequence(seq_name);
    if (!seq) return;

    auto& evt_track = inst->event_tracks[seq_name];
    if (!evt_track) {
        evt_track = std::make_shared<EventTrack>("Events");
        seq->AddTrack(evt_track);
    }
    evt_track->AddEvent(time, event_name, payload ? payload : "");
}

extern "C" void dse_cutscene_add_audio_cue(int player_id, const char* seq_name, float time,
                                           const char* path, float volume, int loop) {
    auto* inst = GetCS(player_id);
    if (!inst || !seq_name || !path) return;
    auto seq = inst->player.GetSequence(seq_name);
    if (!seq) return;

    auto& aud_track = inst->audio_tracks[seq_name];
    if (!aud_track) {
        aud_track = std::make_shared<AudioTrack>("Audio");
        seq->AddTrack(aud_track);
    }
    aud_track->AddCue(time, path, volume, loop != 0);
}

extern "C" void dse_cutscene_play(int player_id, const char* seq_name) {
    auto* inst = GetCS(player_id);
    if (inst && seq_name) inst->player.Play(seq_name);
}

extern "C" void dse_cutscene_pause(int player_id) {
    if (auto* inst = GetCS(player_id)) inst->player.Pause();
}

extern "C" void dse_cutscene_resume(int player_id) {
    if (auto* inst = GetCS(player_id)) inst->player.Resume();
}

extern "C" void dse_cutscene_stop(int player_id) {
    if (auto* inst = GetCS(player_id)) inst->player.Stop();
}

extern "C" void dse_cutscene_seek(int player_id, float time) {
    if (auto* inst = GetCS(player_id)) inst->player.Seek(time);
}

extern "C" float dse_cutscene_get_time(int player_id) {
    auto* inst = GetCS(player_id);
    return inst ? inst->player.GetCurrentTime() : 0.0f;
}

extern "C" int dse_cutscene_get_state(int player_id) {
    auto* inst = GetCS(player_id);
    if (!inst) return 0;
    switch (inst->player.GetState()) {
        case PlayState::Playing: return 1;
        case PlayState::Paused: return 2;
        default: return 0;
    }
}

extern "C" void dse_cutscene_set_play_rate(int player_id, float rate) {
    if (auto* inst = GetCS(player_id)) inst->player.SetPlayRate(rate);
}

extern "C" void dse_cutscene_update(int player_id, float dt) {
    if (auto* inst = GetCS(player_id)) inst->player.Update(dt);
}

extern "C" void dse_cutscene_set_camera_callback(int player_id, const char* seq_name,
                                                 dse_cutscene_camera_fn fn, void* user_data) {
    auto* inst = GetCS(player_id);
    if (!inst || !seq_name) return;
    auto it = inst->camera_tracks.find(seq_name);
    if (it == inst->camera_tracks.end() || !it->second) return;
    it->second->SetApplyCallback([fn, user_data](const glm::vec3& pos, const glm::vec3& look,
                                                 float fov) {
        if (fn) fn(pos.x, pos.y, pos.z, look.x, look.y, look.z, fov, user_data);
    });
}

extern "C" void dse_cutscene_set_event_callback(int player_id, const char* seq_name,
                                                dse_cutscene_event_fn fn, void* user_data) {
    auto* inst = GetCS(player_id);
    if (!inst || !seq_name) return;
    auto it = inst->event_tracks.find(seq_name);
    if (it == inst->event_tracks.end() || !it->second) return;
    it->second->SetFireCallback([fn, user_data](const std::string& name,
                                                const std::string& payload) {
        if (fn) fn(name.c_str(), payload.c_str(), user_data);
    });
}

extern "C" void dse_cutscene_set_finish_callback(int player_id,
                                                 dse_cutscene_finish_fn fn, void* user_data) {
    auto* inst = GetCS(player_id);
    if (!inst) return;
    inst->player.SetFinishCallback([fn, user_data](const std::string& seq_name) {
        if (fn) fn(seq_name.c_str(), user_data);
    });
}
