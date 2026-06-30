/**
 * @file cutscene_player.cpp
 * @brief 过场播放器实现
 */

#include "engine/cutscene/cutscene_player.h"
#include <algorithm>

namespace dse {
namespace cutscene {

// ============================================================
// CutsceneSequence
// ============================================================

void CutsceneSequence::AddTrack(std::shared_ptr<CutsceneTrack> track) {
    if (track) tracks_.push_back(std::move(track));
}

void CutsceneSequence::Evaluate(float time) {
    for (auto& track : tracks_) {
        track->Evaluate(time);
    }
}

void CutsceneSequence::Reset() {
    for (auto& track : tracks_) {
        track->Reset();
    }
}

// ============================================================
// CutscenePlayer
// ============================================================

void CutscenePlayer::AddSequence(std::shared_ptr<CutsceneSequence> seq) {
    if (seq) sequences_[seq->GetName()] = std::move(seq);
}

void CutscenePlayer::RemoveSequence(const std::string& name) {
    sequences_.erase(name);
}

std::shared_ptr<CutsceneSequence> CutscenePlayer::GetSequence(const std::string& name) const {
    auto it = sequences_.find(name);
    return (it != sequences_.end()) ? it->second : nullptr;
}

void CutscenePlayer::Play(const std::string& name) {
    auto seq = GetSequence(name);
    if (!seq) return;

    current_seq_name_ = name;
    current_time_ = 0.0f;
    state_ = PlayState::Playing;
    seq->Reset();
}

void CutscenePlayer::Pause() {
    if (state_ == PlayState::Playing) state_ = PlayState::Paused;
}

void CutscenePlayer::Resume() {
    if (state_ == PlayState::Paused) state_ = PlayState::Playing;
}

void CutscenePlayer::Stop() {
    state_ = PlayState::Stopped;
    current_time_ = 0.0f;
    current_seq_name_.clear();
}

void CutscenePlayer::Seek(float time) {
    current_time_ = time;
    auto seq = GetSequence(current_seq_name_);
    if (seq) {
        seq->Reset();
        seq->Evaluate(current_time_);
    }
}

void CutscenePlayer::AddTrigger(const CutsceneTrigger& trigger) {
    triggers_.push_back(trigger);
}

void CutscenePlayer::ClearTriggers() {
    triggers_.clear();
}

void CutscenePlayer::CheckTriggers() {
    for (auto& trigger : triggers_) {
        if (trigger.fired) continue;
        if (trigger.condition && trigger.condition()) {
            trigger.fired = true;
            Play(trigger.sequence_name);
        }
    }
    // 移除已触发且 auto_remove 的触发器
    triggers_.erase(
        std::remove_if(triggers_.begin(), triggers_.end(),
                       [](const CutsceneTrigger& t) { return t.fired && t.auto_remove; }),
        triggers_.end());
}

void CutscenePlayer::Update(float dt) {
    // 检查触发器
    if (state_ != PlayState::Playing) {
        CheckTriggers();
    }

    if (state_ != PlayState::Playing) return;

    current_time_ += dt * play_rate_;

    auto seq = GetSequence(current_seq_name_);
    if (!seq) {
        Stop();
        return;
    }

    // 评估轨道
    seq->Evaluate(current_time_);

    // 检查是否播完
    if (current_time_ >= seq->GetDuration()) {
        std::string finished_name = current_seq_name_;
        Stop();
        if (finish_callback_) {
            finish_callback_(finished_name);
        }
        // 播完后继续检查触发器
        CheckTriggers();
    }
}

} // namespace cutscene
} // namespace dse
