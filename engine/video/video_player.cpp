/**
 * @file video_player.cpp
 * @brief 视频播放器高层控制实现
 */

#include "engine/video/video_player.h"
#include <algorithm>

namespace dse {
namespace video {

VideoPlayer::VideoPlayer()
    : texture_(std::make_unique<VideoTexture>()) {}

VideoPlayer::~VideoPlayer() {
    Stop();
}

void VideoPlayer::SetRhiDevice(render::RhiDevice* rhi) {
    if (texture_) texture_->SetRhiDevice(rhi);
}

void VideoPlayer::Play(const std::string& path, const VideoPlayConfig& config) {
    Stop();

    config_ = config;
    loop_ = config.loop;
    playback_rate_ = config.playback_rate;
    current_path_ = path;

    decoder_ = CreateDecoder(config.backend);
    if (!decoder_) {
        state_ = VideoState::Error;
        if (on_error_) on_error_("Failed to create decoder");
        return;
    }

    if (!decoder_->Open(path, config.decode_audio)) {
        state_ = VideoState::Error;
        if (on_error_) on_error_("Failed to open: " + path);
        decoder_.reset();
        return;
    }

    info_ = decoder_->GetInfo();
    current_time_ = 0.0;
    next_frame_time_ = 0.0;

    // 后台预解码 + PTS 时钟：视频帧由解码线程填充环形缓冲，
    // 主线程按目标时间取帧，天然支持音画同步与预取。
    sync_ = std::make_unique<VideoAudioSync>();
    sync_->Start(decoder_.get(), config.prefetch_frames);

    state_ = VideoState::Playing;
}

void VideoPlayer::Pause() {
    if (state_ == VideoState::Playing) {
        state_ = VideoState::Paused;
    }
}

void VideoPlayer::Resume() {
    if (state_ == VideoState::Paused) {
        state_ = VideoState::Playing;
    }
}

void VideoPlayer::Stop() {
    if (sync_) {
        sync_->Stop();
        sync_.reset();
    }
    if (decoder_) {
        decoder_->Close();
        decoder_.reset();
    }
    if (texture_) {
        texture_->Destroy();
    }
    state_ = VideoState::Stopped;
    current_time_ = 0.0;
    next_frame_time_ = 0.0;
    info_ = {};
}

void VideoPlayer::Seek(double time_sec) {
    if (!decoder_) return;

    time_sec = std::max(0.0, std::min(time_sec, info_.duration));
    if (decoder_->Seek(time_sec)) {
        current_time_ = time_sec;
        next_frame_time_ = time_sec;
        // 解码线程需要从新位置重新填充环形缓冲。
        if (sync_) {
            sync_->Stop();
            sync_->Start(decoder_.get(), config_.prefetch_frames);
        }
    }
}

void VideoPlayer::SetLoop(bool loop) {
    loop_ = loop;
}

void VideoPlayer::SetPlaybackRate(float rate) {
    playback_rate_ = std::max(0.1f, std::min(rate, 10.0f));
}

uint32_t VideoPlayer::Update(float delta_time) {
    if (state_ != VideoState::Playing || !decoder_) {
        return GetCurrentTexture();
    }

    current_time_ += static_cast<double>(delta_time) * static_cast<double>(playback_rate_);

    // 后台预解码路径：解码线程填充环形缓冲，主线程按目标时间取帧（PTS 驱动）。
    if (sync_ && sync_->IsRunning()) {
        VideoFrame frame{};
        if (sync_->GetFrameAtTime(current_time_, frame)) {
            return texture_->Upload(frame);
        }

        // 无可用帧：解码线程已因 EOF 退出且缓冲已清空 → 播完。
        if (sync_->IsEof()) {
            if (loop_) {
                decoder_->Seek(0.0);
                current_time_ = 0.0;
                next_frame_time_ = 0.0;
                sync_->Stop();
                sync_->Start(decoder_.get(), config_.prefetch_frames);
                if (on_looped_) on_looped_();
            } else {
                state_ = VideoState::Finished;
                if (on_finished_) on_finished_();
            }
        }
        return GetCurrentTexture();
    }

    // 同步解码回退路径（无后台线程时，保持原有逐帧解码行为）
    double frame_duration = (info_.fps > 0.0) ? (1.0 / info_.fps) : (1.0 / 30.0);

    if (current_time_ >= next_frame_time_) {
        VideoFrame frame{};
        bool got_frame = false;

        // Decode frames until we catch up (skip if behind)
        while (current_time_ >= next_frame_time_) {
            if (!decoder_->DecodeNextFrame(frame)) {
                // EOF
                if (loop_) {
                    decoder_->Seek(0.0);
                    current_time_ = 0.0;
                    next_frame_time_ = 0.0;
                    if (on_looped_) on_looped_();
                    // Try decoding first frame of new loop
                    if (decoder_->DecodeNextFrame(frame)) {
                        got_frame = true;
                        next_frame_time_ = frame_duration;
                    }
                    break;
                } else {
                    state_ = VideoState::Finished;
                    if (on_finished_) on_finished_();
                    return GetCurrentTexture();
                }
            }
            got_frame = true;
            next_frame_time_ += frame_duration;
        }

        if (got_frame) {
            return texture_->Upload(frame);
        }
    }

    return GetCurrentTexture();
}

uint32_t VideoPlayer::GetCurrentTexture() const {
    return texture_ ? texture_->GetTextureId() : 0;
}

} // namespace video
} // namespace dse
