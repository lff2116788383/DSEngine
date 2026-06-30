/**
 * @file video_decoder_ffmpeg.h
 * @brief FFmpeg 动态加载解码器实现
 */

#ifndef DSE_VIDEO_DECODER_FFMPEG_H
#define DSE_VIDEO_DECODER_FFMPEG_H

#include "engine/video/video_decoder.h"
#include <vector>
#include <cstdint>

namespace dse {
namespace video {

/// 检查 FFmpeg 库是否可用（运行时动态加载）
bool IsFFmpegAvailable();

/// FFmpeg 解码器：支持 H.264/H.265/VP9/AV1
/// 通过动态加载 libavcodec/libavformat/libswscale 实现
/// 不可用时返回 false，调用方降级到 pl_mpeg
class FFmpegDecoder : public IVideoDecoder {
public:
    FFmpegDecoder();
    ~FFmpegDecoder() override;

    bool Open(const std::string& path, bool decode_audio = true) override;
    bool DecodeNextFrame(VideoFrame& out_frame) override;
    int DecodeAudio(float* buffer, int max_samples) override;
    bool Seek(double time_sec) override;
    VideoInfo GetInfo() const override;
    bool IsEOF() const override;
    void Close() override;
    DecoderBackend GetBackend() const override { return DecoderBackend::FFmpeg; }

    /// FFmpeg 是否已成功加载
    bool IsLoaded() const { return loaded_; }

private:
    bool LoadLibraries();
    bool InitDecoder(const std::string& path, bool decode_audio);
    void FreeResources();

    bool loaded_ = false;
    bool eof_ = false;
    VideoInfo info_{};

    // Opaque handles to avoid exposing FFmpeg headers
    struct Impl;
    Impl* impl_ = nullptr;

    // Decoded frame buffer (YUV420P or converted RGBA)
    std::vector<uint8_t> frame_buffer_;
    std::vector<uint8_t> audio_buffer_;
    int audio_buffer_pos_ = 0;
    int audio_buffer_size_ = 0;
};

} // namespace video
} // namespace dse

#endif // DSE_VIDEO_DECODER_FFMPEG_H
