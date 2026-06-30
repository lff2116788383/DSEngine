/**
 * @file video_decoder.h
 * @brief 视频解码器抽象接口
 */

#ifndef DSE_VIDEO_DECODER_H
#define DSE_VIDEO_DECODER_H

#include "engine/video/video_types.h"
#include <memory>

namespace dse {
namespace video {

class IVideoDecoder {
public:
    virtual ~IVideoDecoder() = default;

    virtual bool Open(const std::string& path, bool decode_audio = true) = 0;
    virtual bool DecodeNextFrame(VideoFrame& out_frame) = 0;
    virtual int DecodeAudio(float* buffer, int max_samples) = 0;
    virtual bool Seek(double time_sec) = 0;
    virtual VideoInfo GetInfo() const = 0;
    virtual bool IsEOF() const = 0;
    virtual void Close() = 0;
    virtual DecoderBackend GetBackend() const = 0;
};

/// 根据 backend 类型创建解码器实例
std::unique_ptr<IVideoDecoder> CreateDecoder(DecoderBackend backend);

} // namespace video
} // namespace dse

#endif // DSE_VIDEO_DECODER_H
