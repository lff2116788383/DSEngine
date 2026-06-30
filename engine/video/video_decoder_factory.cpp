/**
 * @file video_decoder_factory.cpp
 * @brief 解码器工厂：根据 backend 创建合适的解码器实例
 */

#include "engine/video/video_decoder.h"
#include "engine/video/video_decoder_plmpeg.h"
#include "engine/video/video_decoder_ffmpeg.h"

namespace dse {
namespace video {

std::unique_ptr<IVideoDecoder> CreateDecoder(DecoderBackend backend) {
    switch (backend) {
        case DecoderBackend::FFmpeg:
            if (IsFFmpegAvailable()) {
                return std::make_unique<FFmpegDecoder>();
            }
            // FFmpeg not available, fall through to pl_mpeg
            return std::make_unique<PlmpegDecoder>();

        case DecoderBackend::PlMpeg:
            return std::make_unique<PlmpegDecoder>();

        case DecoderBackend::Auto:
        default:
            // Try FFmpeg first, fallback to pl_mpeg
            if (IsFFmpegAvailable()) {
                return std::make_unique<FFmpegDecoder>();
            }
            return std::make_unique<PlmpegDecoder>();
    }
}

} // namespace video
} // namespace dse
