/**
 * @file cutscene_track.cpp
 * @brief 过场轨道实现
 */

#include "engine/cutscene/cutscene_track.h"
#include <algorithm>

namespace dse {
namespace cutscene {

// ============================================================
// CameraTrack
// ============================================================

void CameraTrack::Evaluate(float time) {
    if (keyframes_.empty() || !apply_func_) return;

    if (keyframes_.size() == 1 || time <= keyframes_.front().time) {
        const auto& kf = keyframes_.front();
        apply_func_(kf.position, kf.look_at, kf.fov);
        return;
    }
    if (time >= keyframes_.back().time) {
        const auto& kf = keyframes_.back();
        apply_func_(kf.position, kf.look_at, kf.fov);
        return;
    }

    // 找到包含 time 的区间
    for (size_t i = 0; i + 1 < keyframes_.size(); ++i) {
        const auto& a = keyframes_[i];
        const auto& b = keyframes_[i + 1];
        if (time >= a.time && time <= b.time) {
            float t = (b.time - a.time) > 0.0f ? (time - a.time) / (b.time - a.time) : 0.0f;
            if (a.interp == InterpMode::Step) t = 0.0f;
            glm::vec3 pos = glm::mix(a.position, b.position, t);
            glm::vec3 look = glm::mix(a.look_at, b.look_at, t);
            float fov = a.fov + (b.fov - a.fov) * t;
            apply_func_(pos, look, fov);
            return;
        }
    }
}

// ============================================================
// PropertyTrack
// ============================================================

void PropertyTrack::AddKeyframe(float time, float value, InterpMode interp) {
    Keyframe<float> kf;
    kf.time = time;
    kf.value = value;
    kf.interp = interp;
    keyframes_.push_back(kf);
    // 保持按时间排序
    std::sort(keyframes_.begin(), keyframes_.end(),
              [](const Keyframe<float>& a, const Keyframe<float>& b) { return a.time < b.time; });
}

void PropertyTrack::Evaluate(float time) {
    if (keyframes_.empty() || !apply_func_) return;

    if (keyframes_.size() == 1 || time <= keyframes_.front().time) {
        apply_func_(keyframes_.front().value);
        return;
    }
    if (time >= keyframes_.back().time) {
        apply_func_(keyframes_.back().value);
        return;
    }

    for (size_t i = 0; i + 1 < keyframes_.size(); ++i) {
        const auto& a = keyframes_[i];
        const auto& b = keyframes_[i + 1];
        if (time >= a.time && time <= b.time) {
            float t = (b.time - a.time) > 0.0f ? (time - a.time) / (b.time - a.time) : 0.0f;
            if (a.interp == InterpMode::Step) t = 0.0f;
            float value = a.value + (b.value - a.value) * t;
            apply_func_(value);
            return;
        }
    }
}

// ============================================================
// EventTrack
// ============================================================

void EventTrack::AddEvent(float time, const std::string& event_name, const std::string& payload) {
    CutsceneEvent ev;
    ev.time = time;
    ev.event_name = event_name;
    ev.payload = payload;
    events_.push_back(ev);
    std::sort(events_.begin(), events_.end(),
              [](const CutsceneEvent& a, const CutsceneEvent& b) { return a.time < b.time; });
}

void EventTrack::Evaluate(float time) {
    if (events_.empty() || !fire_func_) return;

    for (const auto& ev : events_) {
        if (ev.time > last_time_ && ev.time <= time) {
            fire_func_(ev.event_name, ev.payload);
        }
    }
    last_time_ = time;
}

void EventTrack::Reset() {
    last_time_ = -1.0f;
}

// ============================================================
// AudioTrack
// ============================================================

void AudioTrack::AddCue(float time, const std::string& path, float volume, bool loop) {
    AudioCue cue;
    cue.time = time;
    cue.audio_path = path;
    cue.volume = volume;
    cue.loop = loop;
    cues_.push_back(cue);
    std::sort(cues_.begin(), cues_.end(),
              [](const AudioCue& a, const AudioCue& b) { return a.time < b.time; });
}

void AudioTrack::Evaluate(float time) {
    if (cues_.empty() || !play_func_) return;

    for (const auto& cue : cues_) {
        if (cue.time > last_time_ && cue.time <= time) {
            play_func_(cue.audio_path, cue.volume, cue.loop);
        }
    }
    last_time_ = time;
}

void AudioTrack::Reset() {
    last_time_ = -1.0f;
}

} // namespace cutscene
} // namespace dse
