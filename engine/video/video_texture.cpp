/**
 * @file video_texture.cpp
 * @brief 视频帧 → GPU 纹理上传实现
 *
 * 已注入 RhiDevice 时走真实 GPU 路径（CreateTexture2D/DeleteTexture）；
 * 未注入时保持轻量 stub（仅状态跟踪），供无 GPU 环境与单元测试使用。
 * RHI 暂未提供"原地更新纹理数据"接口，故逐帧上传采用重建纹理实现，
 * 后续若接入 UpdateTextureSubRegion 类接口可改为原地更新以降低开销。
 */

#include "engine/video/video_texture.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_handle.h"

namespace dse {
namespace video {

VideoTexture::VideoTexture() = default;

VideoTexture::~VideoTexture() {
    Destroy();
}

void VideoTexture::SetRhiDevice(render::RhiDevice* rhi) {
    // 切换设备时旧纹理已失效，先释放再换绑。
    Destroy();
    rhi_ = rhi;
}

void VideoTexture::Initialize(int width, int height, PixelFormat format) {
    if (initialized_) Destroy();

    width_ = width;
    height_ = height;
    format_ = format;

    if (rhi_) {
        render::TextureHandle handle = rhi_->CreateTexture2D(
            width, height, nullptr, true);
        texture_handle_ = handle.raw();
        texture_id_ = texture_handle_;
    } else {
        // Stub implementation for unit testing (no real GL calls)
        // In production this calls RHI texture APIs; for testing we track state only.
        static uint32_t s_next_tex_id = 1000;
        texture_id_ = s_next_tex_id++;
    }
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

    if (rhi_) {
        if (frame.format == PixelFormat::RGBA8 && frame.planes[0] != nullptr) {
            // 逐帧重建纹理并上传像素（无原地更新接口时的等效实现）。
            if (texture_handle_ != 0) {
                rhi_->DeleteTexture(render::TextureHandle::from_raw(texture_handle_));
                texture_handle_ = 0;
            }
            render::TextureHandle handle = rhi_->CreateTexture2D(
                frame.width, frame.height, frame.planes[0], true);
            texture_handle_ = handle.raw();
            texture_id_ = texture_handle_;
        } else {
            // YUV420P 等格式暂未做三平面 + 转换 shader 上传，保持已有纹理不变。
            return texture_id_;
        }
    } else {
        // Stub: 无 RHI 设备时仅记录"已上传"状态。
        (void)frame;
    }

    return texture_id_;
}

void VideoTexture::Destroy() {
    if (rhi_ && texture_handle_ != 0) {
        rhi_->DeleteTexture(render::TextureHandle::from_raw(texture_handle_));
    }
    texture_handle_ = 0;
    texture_id_ = 0;
    initialized_ = false;
    width_ = 0;
    height_ = 0;
}

} // namespace video
} // namespace dse
