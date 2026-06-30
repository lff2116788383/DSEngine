/**
 * @file video_decoder_plmpeg.h
 * @brief pl_mpeg MPEG-1 解码器实现
 */

#ifndef DSE_VIDEO_DECODER_PLMPEG_H
#define DSE_VIDEO_DECODER_PLMPEG_H

#include "engine/video/video_decoder.h"
#include <vector>

// Forward declare pl_mpeg types
typedef struct plm_t plm_t;

namespace dse {
namespace video {

class PlmpegDecoder : public IVideoDecoder {
public:
    PlmpegDecoder();
    ~PlmpegDecoder() override;

    bool Open(const std::string& path, bool decode_audio = true) override;
    bool DecodeNextFrame(VideoFrame& out_frame) override;
    int DecodeAudio(float* buffer, int max_samples) override;
    bool Seek(double time_sec) override;
    VideoInfo GetInfo() const override;
    bool IsEOF() const override;
    void Close() override;
    DecoderBackend GetBackend() const override { return DecoderBackend::PlMpeg; }

private:
    plm_t* plm_ = nullptr;
    VideoInfo info_{};
    bool eof_ = false;
    bool audio_enabled_ = true;
    std::vector<uint8_t> rgba_buffer_;
    std::vector<float> audio_buffer_;
    int audio_buffer_pos_ = 0;
    int audio_buffer_size_ = 0;
};

} // namespace video
} // namespace dse

#endif // DSE_VIDEO_DECODER_PLMPEG_H
