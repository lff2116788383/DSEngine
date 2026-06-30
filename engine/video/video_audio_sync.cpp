/**
 * @file video_audio_sync.cpp
 * @brief 音视频同步控制实现
 */

#include "engine/video/video_audio_sync.h"
#include <cstring>
#include <algorithm>

namespace dse {
namespace video {

VideoAudioSync::VideoAudioSync() = default;

VideoAudioSync::~VideoAudioSync() {
    Stop();
}

void VideoAudioSync::Start(IVideoDecoder* decoder, int ring_size) {
    Stop();

    decoder_ = decoder;
    ring_capacity_ = std::max(2, ring_size);
    ring_buffer_.resize(static_cast<size_t>(ring_capacity_));
    ring_head_ = 0;
    ring_tail_ = 0;
    ring_count_ = 0;
    audio_clock_.store(0.0);
    running_.store(true);

    decode_thread_ = std::thread(&VideoAudioSync::DecodeThreadFunc, this);
}

void VideoAudioSync::Stop() {
    if (running_.load()) {
        running_.store(false);
        ring_not_full_.notify_all();
        ring_not_empty_.notify_all();
        if (decode_thread_.joinable()) {
            decode_thread_.join();
        }
    }
    ring_buffer_.clear();
    ring_head_ = 0;
    ring_tail_ = 0;
    ring_count_ = 0;
    decoder_ = nullptr;
}

void VideoAudioSync::DecodeThreadFunc() {
    while (running_.load()) {
        // Wait if buffer is full
        {
            std::unique_lock<std::mutex> lock(ring_mutex_);
            ring_not_full_.wait(lock, [this]() {
                return ring_count_ < ring_capacity_ || !running_.load();
            });
            if (!running_.load()) break;
        }

        // Decode a frame
        VideoFrame frame{};
        if (!decoder_->DecodeNextFrame(frame)) {
            // EOF - stop decode thread
            break;
        }

        // Copy frame data into ring buffer entry
        {
            std::unique_lock<std::mutex> lock(ring_mutex_);
            auto& entry = ring_buffer_[static_cast<size_t>(ring_tail_)];

            // Copy pixel data
            size_t data_size = 0;
            if (frame.format == PixelFormat::RGBA8) {
                data_size = static_cast<size_t>(frame.strides[0]) * frame.height;
            } else if (frame.format == PixelFormat::YUV420P) {
                data_size = static_cast<size_t>(frame.strides[0]) * frame.height +
                            static_cast<size_t>(frame.strides[1]) * (frame.height / 2) +
                            static_cast<size_t>(frame.strides[2]) * (frame.height / 2);
            }

            entry.data.resize(data_size);
            if (frame.format == PixelFormat::RGBA8 && frame.planes[0]) {
                std::memcpy(entry.data.data(), frame.planes[0], data_size);
            }

            entry.frame = frame;
            entry.frame.planes[0] = entry.data.data();
            entry.pts = frame.pts;

            ring_tail_ = (ring_tail_ + 1) % ring_capacity_;
            ring_count_++;

            ring_not_empty_.notify_one();
        }
    }
}

bool VideoAudioSync::GetFrameAtTime(double target_time, VideoFrame& out_frame) {
    std::unique_lock<std::mutex> lock(ring_mutex_);

    if (ring_count_ == 0) return false;

    // Find the frame closest to (but not exceeding) target_time
    auto& entry = ring_buffer_[static_cast<size_t>(ring_head_)];

    // If the front frame's PTS is still ahead of target, keep previous frame
    if (entry.pts > target_time + 0.04) {
        return false; // Wait for audio to catch up
    }

    // Consume frames that are behind target (skip if video is late)
    while (ring_count_ > 1) {
        int next = (ring_head_ + 1) % ring_capacity_;
        auto& next_entry = ring_buffer_[static_cast<size_t>(next)];
        if (next_entry.pts <= target_time) {
            ring_head_ = next;
            ring_count_--;
            ring_not_full_.notify_one();
        } else {
            break;
        }
    }

    // Return current front frame
    out_frame = ring_buffer_[static_cast<size_t>(ring_head_)].frame;
    ring_head_ = (ring_head_ + 1) % ring_capacity_;
    ring_count_--;
    ring_not_full_.notify_one();

    return true;
}

void VideoAudioSync::UpdateAudioClock(double audio_pts) {
    audio_clock_.store(audio_pts);
}

bool VideoAudioSync::HasFrames() const {
    // Simple check without lock (atomic would be better but ring_count_ changes are small)
    return ring_count_ > 0;
}

void VideoAudioSync::NotifyConsumed() {
    ring_not_full_.notify_one();
}

} // namespace video
} // namespace dse
