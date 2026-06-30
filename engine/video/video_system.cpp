/**
 * @file video_system.cpp
 * @brief ECS 视频系统实现
 */

#include "engine/video/video_system.h"

namespace dse {
namespace video {

VideoSystem::VideoSystem() = default;

VideoSystem::~VideoSystem() {
    Shutdown();
}

void VideoSystem::Update(float delta_time) {
    for (auto& [handle, player] : players_) {
        if (player && player->GetState() == VideoState::Playing) {
            player->Update(delta_time);
        }
    }
}

uint32_t VideoSystem::CreatePlayer(const VideoScreenComponent& config) {
    uint32_t handle = next_handle_++;
    auto player = std::make_unique<VideoPlayer>();

    if (config.auto_play && !config.video_path.empty()) {
        VideoPlayConfig play_config{};
        play_config.loop = config.loop;
        play_config.playback_rate = config.playback_rate;
        play_config.backend = config.backend;
        player->Play(config.video_path, play_config);
    }

    players_[handle] = std::move(player);
    return handle;
}

void VideoSystem::DestroyPlayer(uint32_t handle) {
    players_.erase(handle);
}

uint32_t VideoSystem::GetPlayerTexture(uint32_t handle) const {
    auto* player = GetPlayer(handle);
    return player ? player->GetCurrentTexture() : 0;
}

VideoState VideoSystem::GetPlayerState(uint32_t handle) const {
    auto* player = GetPlayer(handle);
    return player ? player->GetState() : VideoState::Stopped;
}

void VideoSystem::PlayerPlay(uint32_t handle, const std::string& path, const VideoPlayConfig& config) {
    auto* player = GetPlayer(handle);
    if (player) player->Play(path, config);
}

void VideoSystem::PlayerPause(uint32_t handle) {
    auto* player = GetPlayer(handle);
    if (player) player->Pause();
}

void VideoSystem::PlayerResume(uint32_t handle) {
    auto* player = GetPlayer(handle);
    if (player) player->Resume();
}

void VideoSystem::PlayerStop(uint32_t handle) {
    auto* player = GetPlayer(handle);
    if (player) player->Stop();
}

void VideoSystem::PlayerSeek(uint32_t handle, double time_sec) {
    auto* player = GetPlayer(handle);
    if (player) player->Seek(time_sec);
}

void VideoSystem::Shutdown() {
    players_.clear();
    next_handle_ = 1;
}

VideoPlayer* VideoSystem::GetPlayer(uint32_t handle) {
    auto it = players_.find(handle);
    return (it != players_.end()) ? it->second.get() : nullptr;
}

const VideoPlayer* VideoSystem::GetPlayer(uint32_t handle) const {
    auto it = players_.find(handle);
    return (it != players_.end()) ? it->second.get() : nullptr;
}

} // namespace video
} // namespace dse
