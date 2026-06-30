/**
 * @file video_player.h
 * @brief 视频播放器高层控制
 */

#ifndef DSE_VIDEO_PLAYER_H
#define DSE_VIDEO_PLAYER_H

#include "engine/video/video_types.h"
#include "engine/video/video_decoder.h"
#include "engine/video/video_texture.h"
#include <memory>
#include <functional>
#include <string>

namespace dse {
namespace video {

class VideoPlayer {
public:
    VideoPlayer();
    ~VideoPlayer();

    /// 播放控制
    void Play(const std::string& path, const VideoPlayConfig& config = {});
    void Pause();
    void Resume();
    void Stop();
    void Seek(double time_sec);
    void SetLoop(bool loop);
    void SetPlaybackRate(float rate);

    /// 每帧更新：推进时钟，解码帧，上传纹理
    /// @return 当前帧纹理 ID（0=无有效帧）
    uint32_t Update(float delta_time);

    /// 状态查询
    VideoState GetState() const { return state_; }
    double GetCurrentTime() const { return current_time_; }
    double GetDuration() const { return info_.duration; }
    const VideoInfo& GetInfo() const { return info_; }
    uint32_t GetCurrentTexture() const;
    float GetPlaybackRate() const { return playback_rate_; }
    bool IsLooping() const { return loop_; }

    /// 回调
    void SetOnFinished(std::function<void()> cb) { on_finished_ = std::move(cb); }
    void SetOnLooped(std::function<void()> cb) { on_looped_ = std::move(cb); }
    void SetOnError(std::function<void(const std::string&)> cb) { on_error_ = std::move(cb); }

private:
    std::unique_ptr<IVideoDecoder> decoder_;
    std::unique_ptr<VideoTexture> texture_;
    VideoInfo info_{};
    VideoState state_ = VideoState::Stopped;
    VideoPlayConfig config_{};

    double current_time_ = 0.0;
    double next_frame_time_ = 0.0;
    float playback_rate_ = 1.0f;
    bool loop_ = false;
    std::string current_path_;

    std::function<void()> on_finished_;
    std::function<void()> on_looped_;
    std::function<void(const std::string&)> on_error_;
};

} // namespace video
} // namespace dse

#endif // DSE_VIDEO_PLAYER_H
