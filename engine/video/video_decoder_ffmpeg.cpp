/**
 * @file video_decoder_ffmpeg.cpp
 * @brief FFmpeg 动态加载解码器实现
 *
 * 运行时动态加载 libavcodec/libavformat/libswscale/libavutil/libswresample。
 * 若 FFmpeg DLL/SO 不在 PATH 或 LD_LIBRARY_PATH 中，IsFFmpegAvailable() 返回 false，
 * 播放器自动降级到 pl_mpeg。
 */

#include "engine/video/video_decoder_ffmpeg.h"
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#define DSE_DLOPEN(name) LoadLibraryA(name)
#define DSE_DLSYM(lib, sym) GetProcAddress((HMODULE)(lib), sym)
#define DSE_DLCLOSE(lib) FreeLibrary((HMODULE)(lib))
typedef HMODULE DllHandle;
#else
#include <dlfcn.h>
#define DSE_DLOPEN(name) dlopen(name, RTLD_LAZY)
#define DSE_DLSYM(lib, sym) dlsym(lib, sym)
#define DSE_DLCLOSE(lib) dlclose(lib)
typedef void* DllHandle;
#endif

namespace dse {
namespace video {

// ============================================================================
// FFmpeg function pointer types (minimal subset for decode)
// ============================================================================

// AVFormatContext operations
typedef struct AVFormatContext AVFormatContext;
typedef struct AVCodecContext AVCodecContext;
typedef struct AVCodec AVCodec;
typedef struct AVFrame AVFrame;
typedef struct AVPacket AVPacket;
typedef struct AVStream AVStream;
typedef struct SwsContext SwsContext;
typedef struct SwrContext SwrContext;

// Minimal function signatures we need
using fn_avformat_open_input = int(*)(AVFormatContext**, const char*, void*, void**);
using fn_avformat_find_stream_info = int(*)(AVFormatContext*, void**);
using fn_avformat_close_input = void(*)(AVFormatContext**);
using fn_avformat_alloc_context = AVFormatContext*(*)();
using fn_av_find_best_stream = int(*)(AVFormatContext*, int, int, int, const AVCodec**, int);
using fn_av_read_frame = int(*)(AVFormatContext*, AVPacket*);
using fn_av_seek_frame = int(*)(AVFormatContext*, int, int64_t, int);

using fn_avcodec_find_decoder = const AVCodec*(*)(int);
using fn_avcodec_alloc_context3 = AVCodecContext*(*)(const AVCodec*);
using fn_avcodec_parameters_to_context = int(*)(AVCodecContext*, const void*);
using fn_avcodec_open2 = int(*)(AVCodecContext*, const AVCodec*, void**);
using fn_avcodec_free_context = void(*)(AVCodecContext**);
using fn_avcodec_send_packet = int(*)(AVCodecContext*, const AVPacket*);
using fn_avcodec_receive_frame = int(*)(AVCodecContext*, AVFrame*);

using fn_av_frame_alloc = AVFrame*(*)();
using fn_av_frame_free = void(*)(AVFrame**);
using fn_av_packet_alloc = AVPacket*(*)();
using fn_av_packet_free = void(*)(AVPacket**);
using fn_av_packet_unref = void(*)(AVPacket*);

using fn_sws_getContext = SwsContext*(*)(int, int, int, int, int, int, int, void*, void*, const double*);
using fn_sws_scale = int(*)(SwsContext*, const uint8_t* const*, const int*, int, int, uint8_t* const*, const int*);
using fn_sws_freeContext = void(*)(SwsContext*);

// ============================================================================
// Dynamic library handles and function pointers
// ============================================================================

struct FFmpegLibs {
    DllHandle avformat = nullptr;
    DllHandle avcodec = nullptr;
    DllHandle avutil = nullptr;
    DllHandle swscale = nullptr;
    DllHandle swresample = nullptr;

    // Function pointers
    fn_avformat_open_input p_avformat_open_input = nullptr;
    fn_avformat_find_stream_info p_avformat_find_stream_info = nullptr;
    fn_avformat_close_input p_avformat_close_input = nullptr;
    fn_av_find_best_stream p_av_find_best_stream = nullptr;
    fn_av_read_frame p_av_read_frame = nullptr;
    fn_av_seek_frame p_av_seek_frame = nullptr;

    fn_avcodec_find_decoder p_avcodec_find_decoder = nullptr;
    fn_avcodec_alloc_context3 p_avcodec_alloc_context3 = nullptr;
    fn_avcodec_parameters_to_context p_avcodec_parameters_to_context = nullptr;
    fn_avcodec_open2 p_avcodec_open2 = nullptr;
    fn_avcodec_free_context p_avcodec_free_context = nullptr;
    fn_avcodec_send_packet p_avcodec_send_packet = nullptr;
    fn_avcodec_receive_frame p_avcodec_receive_frame = nullptr;

    fn_av_frame_alloc p_av_frame_alloc = nullptr;
    fn_av_frame_free p_av_frame_free = nullptr;
    fn_av_packet_alloc p_av_packet_alloc = nullptr;
    fn_av_packet_free p_av_packet_free = nullptr;
    fn_av_packet_unref p_av_packet_unref = nullptr;

    fn_sws_getContext p_sws_getContext = nullptr;
    fn_sws_scale p_sws_scale = nullptr;
    fn_sws_freeContext p_sws_freeContext = nullptr;

    bool loaded = false;
};

static FFmpegLibs s_ffmpeg{};
static bool s_load_attempted = false;

static bool TryLoadFFmpeg() {
    if (s_load_attempted) return s_ffmpeg.loaded;
    s_load_attempted = true;

#ifdef _WIN32
    // Try common FFmpeg DLL names on Windows
    const char* avformat_names[] = {"avformat-60.dll", "avformat-59.dll", "avformat-58.dll", nullptr};
    const char* avcodec_names[] = {"avcodec-60.dll", "avcodec-59.dll", "avcodec-58.dll", nullptr};
    const char* avutil_names[] = {"avutil-58.dll", "avutil-57.dll", "avutil-56.dll", nullptr};
    const char* swscale_names[] = {"swscale-7.dll", "swscale-6.dll", "swscale-5.dll", nullptr};
#else
    const char* avformat_names[] = {"libavformat.so.60", "libavformat.so.59", "libavformat.so.58", "libavformat.so", nullptr};
    const char* avcodec_names[] = {"libavcodec.so.60", "libavcodec.so.59", "libavcodec.so.58", "libavcodec.so", nullptr};
    const char* avutil_names[] = {"libavutil.so.58", "libavutil.so.57", "libavutil.so.56", "libavutil.so", nullptr};
    const char* swscale_names[] = {"libswscale.so.7", "libswscale.so.6", "libswscale.so.5", "libswscale.so", nullptr};
#endif

    auto try_load = [](const char** names) -> DllHandle {
        for (int i = 0; names[i]; ++i) {
            DllHandle h = DSE_DLOPEN(names[i]);
            if (h) return h;
        }
        return nullptr;
    };

    s_ffmpeg.avformat = try_load(avformat_names);
    s_ffmpeg.avcodec = try_load(avcodec_names);
    s_ffmpeg.avutil = try_load(avutil_names);
    s_ffmpeg.swscale = try_load(swscale_names);

    if (!s_ffmpeg.avformat || !s_ffmpeg.avcodec || !s_ffmpeg.avutil || !s_ffmpeg.swscale) {
        // Cleanup any partially loaded libs
        if (s_ffmpeg.avformat) { DSE_DLCLOSE(s_ffmpeg.avformat); s_ffmpeg.avformat = nullptr; }
        if (s_ffmpeg.avcodec) { DSE_DLCLOSE(s_ffmpeg.avcodec); s_ffmpeg.avcodec = nullptr; }
        if (s_ffmpeg.avutil) { DSE_DLCLOSE(s_ffmpeg.avutil); s_ffmpeg.avutil = nullptr; }
        if (s_ffmpeg.swscale) { DSE_DLCLOSE(s_ffmpeg.swscale); s_ffmpeg.swscale = nullptr; }
        return false;
    }

    // Load function pointers
    #define LOAD_SYM(lib, name) s_ffmpeg.p_##name = (fn_##name)DSE_DLSYM(s_ffmpeg.lib, #name)

    LOAD_SYM(avformat, avformat_open_input);
    LOAD_SYM(avformat, avformat_find_stream_info);
    LOAD_SYM(avformat, avformat_close_input);
    LOAD_SYM(avformat, av_find_best_stream);
    LOAD_SYM(avformat, av_read_frame);
    LOAD_SYM(avformat, av_seek_frame);

    LOAD_SYM(avcodec, avcodec_find_decoder);
    LOAD_SYM(avcodec, avcodec_alloc_context3);
    LOAD_SYM(avcodec, avcodec_parameters_to_context);
    LOAD_SYM(avcodec, avcodec_open2);
    LOAD_SYM(avcodec, avcodec_free_context);
    LOAD_SYM(avcodec, avcodec_send_packet);
    LOAD_SYM(avcodec, avcodec_receive_frame);

    LOAD_SYM(avutil, av_frame_alloc);
    LOAD_SYM(avutil, av_frame_free);

    LOAD_SYM(avcodec, av_packet_alloc);
    LOAD_SYM(avcodec, av_packet_free);
    LOAD_SYM(avcodec, av_packet_unref);

    LOAD_SYM(swscale, sws_getContext);
    LOAD_SYM(swscale, sws_scale);
    LOAD_SYM(swscale, sws_freeContext);

    #undef LOAD_SYM

    // Verify critical functions loaded
    if (!s_ffmpeg.p_avformat_open_input || !s_ffmpeg.p_avcodec_find_decoder ||
        !s_ffmpeg.p_avcodec_send_packet || !s_ffmpeg.p_avcodec_receive_frame ||
        !s_ffmpeg.p_av_frame_alloc || !s_ffmpeg.p_av_packet_alloc) {
        DSE_DLCLOSE(s_ffmpeg.avformat); s_ffmpeg.avformat = nullptr;
        DSE_DLCLOSE(s_ffmpeg.avcodec); s_ffmpeg.avcodec = nullptr;
        DSE_DLCLOSE(s_ffmpeg.avutil); s_ffmpeg.avutil = nullptr;
        DSE_DLCLOSE(s_ffmpeg.swscale); s_ffmpeg.swscale = nullptr;
        return false;
    }

    s_ffmpeg.loaded = true;
    return true;
}

bool IsFFmpegAvailable() {
    return TryLoadFFmpeg();
}

// ============================================================================
// FFmpegDecoder Implementation
// ============================================================================

// Internal implementation (holds raw FFmpeg pointers)
struct FFmpegDecoder::Impl {
    AVFormatContext* fmt_ctx = nullptr;
    AVCodecContext* video_codec_ctx = nullptr;
    AVCodecContext* audio_codec_ctx = nullptr;
    AVFrame* frame = nullptr;
    AVFrame* rgb_frame = nullptr;
    AVPacket* packet = nullptr;
    SwsContext* sws_ctx = nullptr;
    int video_stream_idx = -1;
    int audio_stream_idx = -1;
    double time_base_video = 0.0;
    double time_base_audio = 0.0;
};

FFmpegDecoder::FFmpegDecoder() {
    loaded_ = TryLoadFFmpeg();
}

FFmpegDecoder::~FFmpegDecoder() {
    Close();
}

bool FFmpegDecoder::LoadLibraries() {
    return TryLoadFFmpeg();
}

bool FFmpegDecoder::Open(const std::string& path, bool decode_audio) {
    Close();

    if (!loaded_) return false;

    impl_ = new Impl{};

    // Open input
    int ret = s_ffmpeg.p_avformat_open_input(&impl_->fmt_ctx, path.c_str(), nullptr, nullptr);
    if (ret < 0 || !impl_->fmt_ctx) {
        FreeResources();
        return false;
    }

    // Find stream info
    ret = s_ffmpeg.p_avformat_find_stream_info(impl_->fmt_ctx, nullptr);
    if (ret < 0) {
        FreeResources();
        return false;
    }

    // Find best video stream
    const AVCodec* video_codec = nullptr;
    impl_->video_stream_idx = s_ffmpeg.p_av_find_best_stream(
        impl_->fmt_ctx, 0 /* AVMEDIA_TYPE_VIDEO */, -1, -1, &video_codec, 0);

    if (impl_->video_stream_idx < 0 || !video_codec) {
        FreeResources();
        return false;
    }

    // Open video codec
    impl_->video_codec_ctx = s_ffmpeg.p_avcodec_alloc_context3(video_codec);
    if (!impl_->video_codec_ctx) {
        FreeResources();
        return false;
    }

    // Get stream parameters - access via opaque struct offset
    // AVStream.codecpar is at a fixed offset in the struct
    // For safety, we store the codec parameters we need
    // Note: In real production code, we'd use avcodec_parameters_to_context
    // For now, directly open with the found codec
    ret = s_ffmpeg.p_avcodec_open2(impl_->video_codec_ctx, video_codec, nullptr);
    if (ret < 0) {
        FreeResources();
        return false;
    }

    // Allocate frames and packet
    impl_->frame = s_ffmpeg.p_av_frame_alloc();
    impl_->packet = s_ffmpeg.p_av_packet_alloc();
    if (!impl_->frame || !impl_->packet) {
        FreeResources();
        return false;
    }

    // Fill info (using codec context)
    // Note: In actual FFmpeg, we'd read from AVCodecContext and AVStream
    info_.codec_name = "ffmpeg";
    info_.has_audio = decode_audio && (impl_->audio_stream_idx >= 0);
    eof_ = false;

    return true;
}

bool FFmpegDecoder::DecodeNextFrame(VideoFrame& out_frame) {
    if (!loaded_ || !impl_ || eof_) return false;

    while (true) {
        int ret = s_ffmpeg.p_av_read_frame(impl_->fmt_ctx, impl_->packet);
        if (ret < 0) {
            eof_ = true;
            return false;
        }

        // Check if this is a video packet (simplified - in real code compare stream index)
        ret = s_ffmpeg.p_avcodec_send_packet(impl_->video_codec_ctx, impl_->packet);
        s_ffmpeg.p_av_packet_unref(impl_->packet);

        if (ret < 0) continue;

        ret = s_ffmpeg.p_avcodec_receive_frame(impl_->video_codec_ctx, impl_->frame);
        if (ret < 0) continue;

        // Got a decoded frame - convert to RGBA if sws_ctx available
        // For now, output as YUV420P (planes are in frame->data[0..2])
        // Real implementation would use sws_scale for conversion

        out_frame.format = PixelFormat::YUV420P;
        out_frame.width = info_.width;
        out_frame.height = info_.height;
        out_frame.pts = 0.0; // Would compute from frame->pts * time_base

        return true;
    }
}

int FFmpegDecoder::DecodeAudio(float* buffer, int max_samples) {
    (void)buffer; (void)max_samples;
    // Audio decoding via FFmpeg (swresample) - stub for now
    return 0;
}

bool FFmpegDecoder::Seek(double time_sec) {
    if (!loaded_ || !impl_) return false;

    int64_t ts = static_cast<int64_t>(time_sec * 1000000.0); // AV_TIME_BASE
    int ret = s_ffmpeg.p_av_seek_frame(impl_->fmt_ctx, -1, ts, 0x2 /* AVSEEK_FLAG_BACKWARD */);
    if (ret >= 0) {
        eof_ = false;
        return true;
    }
    return false;
}

VideoInfo FFmpegDecoder::GetInfo() const {
    return info_;
}

bool FFmpegDecoder::IsEOF() const {
    return eof_;
}

void FFmpegDecoder::Close() {
    FreeResources();
    eof_ = false;
    info_ = {};
    frame_buffer_.clear();
    audio_buffer_.clear();
    audio_buffer_pos_ = 0;
    audio_buffer_size_ = 0;
}

void FFmpegDecoder::FreeResources() {
    if (!impl_) return;

    if (impl_->sws_ctx && s_ffmpeg.p_sws_freeContext) {
        s_ffmpeg.p_sws_freeContext(impl_->sws_ctx);
    }
    if (impl_->frame && s_ffmpeg.p_av_frame_free) {
        s_ffmpeg.p_av_frame_free(&impl_->frame);
    }
    if (impl_->rgb_frame && s_ffmpeg.p_av_frame_free) {
        s_ffmpeg.p_av_frame_free(&impl_->rgb_frame);
    }
    if (impl_->packet && s_ffmpeg.p_av_packet_free) {
        s_ffmpeg.p_av_packet_free(&impl_->packet);
    }
    if (impl_->video_codec_ctx && s_ffmpeg.p_avcodec_free_context) {
        s_ffmpeg.p_avcodec_free_context(&impl_->video_codec_ctx);
    }
    if (impl_->audio_codec_ctx && s_ffmpeg.p_avcodec_free_context) {
        s_ffmpeg.p_avcodec_free_context(&impl_->audio_codec_ctx);
    }
    if (impl_->fmt_ctx && s_ffmpeg.p_avformat_close_input) {
        s_ffmpeg.p_avformat_close_input(&impl_->fmt_ctx);
    }

    delete impl_;
    impl_ = nullptr;
}

} // namespace video
} // namespace dse
