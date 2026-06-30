/**
 * @file video_system.h
 * @brief ECS 视频系统：驱动 VideoScreenComponent 播放
 */

#ifndef DSE_VIDEO_SYSTEM_H
#define DSE_VIDEO_SYSTEM_H

#include "engine/video/video_player.h"
#include "engine/video/video_screen_component.h"
#include <unordered_map>
#include <memory>
#include <cstdint>

namespace dse {
namespace video {

class VideoSystem {
public:
    VideoSystem();
    ~VideoSystem();

    /// 每帧更新：遍历所有 VideoScreenComponent，驱动播放器
    void Update(float delta_time);

    /// 为指定实体创建播放器
    uint32_t CreatePlayer(const VideoScreenComponent& config);

    /// 销毁指定播放器
    void DestroyPlayer(uint32_t handle);

    /// 获取播放器输出纹理
    uint32_t GetPlayerTexture(uint32_t handle) const;

    /// 获取播放器状态
    VideoState GetPlayerState(uint32_t handle) const;

    /// 播放控制
    void PlayerPlay(uint32_t handle, const std::string& path, const VideoPlayConfig& config);
    void PlayerPause(uint32_t handle);
    void PlayerResume(uint32_t handle);
    void PlayerStop(uint32_t handle);
    void PlayerSeek(uint32_t handle, double time_sec);

    /// 获取活跃播放器数量
    size_t GetActivePlayerCount() const { return players_.size(); }

    /// 清理所有播放器
    void Shutdown();

private:
    VideoPlayer* GetPlayer(uint32_t handle);
    const VideoPlayer* GetPlayer(uint32_t handle) const;

    uint32_t next_handle_ = 1;
    std::unordered_map<uint32_t, std::unique_ptr<VideoPlayer>> players_;
};

} // namespace video
} // namespace dse

#endif // DSE_VIDEO_SYSTEM_H
