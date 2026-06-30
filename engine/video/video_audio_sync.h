/**
 * @file video_audio_sync.h
 * @brief 音视频同步控制
 */

#ifndef DSE_VIDEO_AUDIO_SYNC_H
#define DSE_VIDEO_AUDIO_SYNC_H

#include "engine/video/video_types.h"
#include "engine/video/video_decoder.h"
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

namespace dse {
namespace video {

struct FrameEntry {
    std::vector<uint8_t> data;
    VideoFrame frame;
    double pts = 0.0;
};

class VideoAudioSync {
public:
    VideoAudioSync();
    ~VideoAudioSync();

    void Start(IVideoDecoder* decoder, int ring_size = 4);
    void Stop();

    /// 获取最接近目标时间的帧（主线程调用）
    bool GetFrameAtTime(double target_time, VideoFrame& out_frame);

    /// 更新音频时钟（音频回调线程调用）
    void UpdateAudioClock(double audio_pts);

    /// 获取音频主时钟
    double GetAudioClock() const { return audio_clock_.load(); }

    /// 是否有帧可用
    bool HasFrames() const;

    /// 通知解码线程可继续（主线程消费帧后调用）
    void NotifyConsumed();

    bool IsRunning() const { return running_.load(); }

private:
    void DecodeThreadFunc();

    IVideoDecoder* decoder_ = nullptr;
    std::atomic<bool> running_{false};
    std::atomic<double> audio_clock_{0.0};
    std::thread decode_thread_;

    std::mutex ring_mutex_;
    std::condition_variable ring_not_full_;
    std::condition_variable ring_not_empty_;
    std::vector<FrameEntry> ring_buffer_;
    int ring_capacity_ = 4;
    int ring_head_ = 0;
    int ring_tail_ = 0;
    int ring_count_ = 0;
};

} // namespace video
} // namespace dse

#endif // DSE_VIDEO_AUDIO_SYNC_H
