/**
 * @file video_texture.cpp
 * @brief 视频帧 → GPU 纹理上传实现
 */

#include "engine/video/video_texture.h"

// Stub implementation for unit testing (no real GL calls)
// In production this calls RHI texture APIs; for testing we track state only.

namespace dse {
namespace video {

VideoTexture::VideoTexture() = default;

VideoTexture::~VideoTexture() {
    Destroy();
}

void VideoTexture::Initialize(int width, int height, PixelFormat format) {
    if (initialized_) Destroy();

    width_ = width;
    height_ = height;
    format_ = format;

    // Allocate a placeholder texture ID (real impl uses RHI)
    // In a real engine this would call rhi->CreateTexture2D(...)
    static uint32_t s_next_tex_id = 1000;
    texture_id_ = s_next_tex_id++;
    initialized_ = true;
}

uint32_t VideoTexture::Upload(const VideoFrame& frame) {
    if (!initialized_) {
        Initialize(frame.width, frame.height, frame.format);
    }

    if (frame.width != width_ || frame.height != height_) {
        Destroy();
        Initialize(frame.width, frame.height, frame.format);
    }

    // In a real engine this would upload pixel data to GPU:
    // - RGBA8: glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data)
    // - YUV420P: upload Y/U/V planes to 3 textures, run YUV->RGB shader
    // For now, the texture_id_ represents a valid "uploaded" texture.
    (void)frame;

    return texture_id_;
}

void VideoTexture::Destroy() {
    if (texture_id_ != 0) {
        // In a real engine: rhi->DeleteTexture(texture_id_)
        texture_id_ = 0;
    }
    initialized_ = false;
    width_ = 0;
    height_ = 0;
}

} // namespace video
} // namespace dse
