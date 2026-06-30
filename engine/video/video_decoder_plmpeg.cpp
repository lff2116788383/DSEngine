/**
 * @file video_decoder_plmpeg.cpp
 * @brief pl_mpeg MPEG-1 解码器实现
 */

#define PL_MPEG_IMPLEMENTATION
#include "depends/pl_mpeg/pl_mpeg.h"

#include "engine/video/video_decoder_plmpeg.h"
#include <cstring>
#include <algorithm>

namespace dse {
namespace video {

PlmpegDecoder::PlmpegDecoder() = default;

PlmpegDecoder::~PlmpegDecoder() {
    Close();
}

bool PlmpegDecoder::Open(const std::string& path, bool decode_audio) {
    Close();

    plm_ = plm_create_with_filename(path.c_str());
    if (!plm_) return false;

    audio_enabled_ = decode_audio && plm_get_num_audio_streams(plm_) > 0;
    plm_set_audio_enabled(plm_, audio_enabled_ ? 1 : 0);
    plm_set_video_enabled(plm_, 1);

    if (audio_enabled_) {
        plm_set_audio_lead_time(plm_, 0.04); // 40ms lead
    }

    info_.width = plm_get_width(plm_);
    info_.height = plm_get_height(plm_);
    info_.fps = plm_get_framerate(plm_);
    info_.duration = plm_get_duration(plm_);
    info_.total_frames = static_cast<int64_t>(info_.fps * info_.duration);
    info_.has_audio = audio_enabled_;
    info_.audio_sample_rate = audio_enabled_ ? 44100 : 0;
    info_.audio_channels = audio_enabled_ ? 2 : 0;
    info_.codec_name = "mpeg1";

    rgba_buffer_.resize(static_cast<size_t>(info_.width) * info_.height * 4);
    eof_ = false;

    return true;
}

bool PlmpegDecoder::DecodeNextFrame(VideoFrame& out_frame) {
    if (!plm_ || eof_) return false;

    plm_frame_t* frame = plm_decode_video(plm_);
    if (!frame) {
        eof_ = true;
        return false;
    }

    // Convert YCrCb to RGBA
    plm_frame_to_rgba(frame, rgba_buffer_.data(), info_.width * 4);

    out_frame.format = PixelFormat::RGBA8;
    out_frame.width = info_.width;
    out_frame.height = info_.height;
    out_frame.pts = frame->time;
    out_frame.planes[0] = rgba_buffer_.data();
    out_frame.planes[1] = nullptr;
    out_frame.planes[2] = nullptr;
    out_frame.strides[0] = info_.width * 4;
    out_frame.strides[1] = 0;
    out_frame.strides[2] = 0;

    return true;
}

int PlmpegDecoder::DecodeAudio(float* buffer, int max_samples) {
    if (!plm_ || !audio_enabled_) return 0;

    int written = 0;

    // First drain any leftover from previous decode
    if (audio_buffer_pos_ < audio_buffer_size_) {
        int available = audio_buffer_size_ - audio_buffer_pos_;
        int to_copy = std::min(available, max_samples);
        std::memcpy(buffer, audio_buffer_.data() + audio_buffer_pos_,
                    static_cast<size_t>(to_copy) * sizeof(float));
        audio_buffer_pos_ += to_copy;
        written += to_copy;
    }

    // Decode more audio if needed
    while (written < max_samples) {
        plm_samples_t* samples = plm_decode_audio(plm_);
        if (!samples) break;

        // plm_samples_t has interleaved stereo, PLM_AUDIO_SAMPLES_PER_FRAME * 2 floats
        int sample_count = PLM_AUDIO_SAMPLES_PER_FRAME * 2;
        int to_copy = std::min(sample_count, max_samples - written);

        std::memcpy(buffer + written, samples->interleaved,
                    static_cast<size_t>(to_copy) * sizeof(float));
        written += to_copy;

        // Store remainder
        if (to_copy < sample_count) {
            int remainder = sample_count - to_copy;
            audio_buffer_.resize(static_cast<size_t>(remainder));
            std::memcpy(audio_buffer_.data(), samples->interleaved + to_copy,
                        static_cast<size_t>(remainder) * sizeof(float));
            audio_buffer_pos_ = 0;
            audio_buffer_size_ = remainder;
            break;
        }
    }

    return written;
}

bool PlmpegDecoder::Seek(double time_sec) {
    if (!plm_) return false;

    time_sec = std::max(0.0, std::min(time_sec, info_.duration));
    plm_seek(plm_, time_sec, FALSE);
    eof_ = false;
    audio_buffer_pos_ = 0;
    audio_buffer_size_ = 0;

    return true;
}

VideoInfo PlmpegDecoder::GetInfo() const {
    return info_;
}

bool PlmpegDecoder::IsEOF() const {
    return eof_ || (plm_ && plm_has_ended(plm_));
}

void PlmpegDecoder::Close() {
    if (plm_) {
        plm_destroy(plm_);
        plm_ = nullptr;
    }
    eof_ = false;
    info_ = {};
    rgba_buffer_.clear();
    audio_buffer_.clear();
    audio_buffer_pos_ = 0;
    audio_buffer_size_ = 0;
}

} // namespace video
} // namespace dse
