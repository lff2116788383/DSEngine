/**
 * @file video_types.h
 * @brief 视频播放系统核心类型定义
 */

#ifndef DSE_VIDEO_TYPES_H
#define DSE_VIDEO_TYPES_H

#include <cstdint>
#include <string>
#include <functional>

namespace dse {
namespace video {

enum class VideoState : uint8_t {
    Stopped,
    Playing,
    Paused,
    Finished,
    Error
};

enum class PixelFormat : uint8_t {
    YUV420P,    ///< 3 平面 Y/U/V
    RGBA8,      ///< 已转换 RGBA
    RGB8        ///< 已转换 RGB
};

enum class DecoderBackend : uint8_t {
    Auto,       ///< 优先 FFmpeg，回退 pl_mpeg
    FFmpeg,
    PlMpeg
};

struct VideoInfo {
    int width = 0;
    int height = 0;
    double fps = 0.0;
    double duration = 0.0;
    int64_t total_frames = 0;
    bool has_audio = false;
    int audio_sample_rate = 0;
    int audio_channels = 0;
    std::string codec_name;
};

struct VideoFrame {
    PixelFormat format = PixelFormat::RGBA8;
    int width = 0;
    int height = 0;
    double pts = 0.0;
    const uint8_t* planes[3] = {};
    int strides[3] = {};
};

struct VideoPlayConfig {
    DecoderBackend backend = DecoderBackend::Auto;
    bool loop = false;
    float playback_rate = 1.0f;
    bool decode_audio = true;
    bool hw_accel = false;
    int prefetch_frames = 4;
    bool yuv_gpu_convert = true;
};

inline const char* VideoStateToString(VideoState s) {
    switch (s) {
        case VideoState::Stopped:  return "stopped";
        case VideoState::Playing:  return "playing";
        case VideoState::Paused:   return "paused";
        case VideoState::Finished: return "finished";
        case VideoState::Error:    return "error";
    }
    return "unknown";
}

} // namespace video
} // namespace dse

#endif // DSE_VIDEO_TYPES_H
