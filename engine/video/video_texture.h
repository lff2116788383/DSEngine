/**
 * @file video_texture.h
 * @brief 视频帧 → GPU 纹理上传
 */

#ifndef DSE_VIDEO_TEXTURE_H
#define DSE_VIDEO_TEXTURE_H

#include "engine/video/video_types.h"
#include <cstdint>
#include <vector>

namespace dse::render {
class RhiDevice;
}

namespace dse {
namespace video {

class VideoTexture {
public:
    VideoTexture();
    ~VideoTexture();

    /// 注入 RHI 设备：注入后纹理走真实 GPU 创建/上传路径；
    /// 未注入时保持轻量 stub（仅状态跟踪，供无 GPU 环境/单测使用）。
    void SetRhiDevice(render::RhiDevice* rhi);

    /// 初始化纹理资源
    void Initialize(int width, int height, PixelFormat format);

    /// 上传帧数据到 GPU 纹理
    /// @return 纹理 ID（OpenGL texture name）
    uint32_t Upload(const VideoFrame& frame);

    /// 获取当前纹理 ID
    uint32_t GetTextureId() const { return texture_id_; }

    /// 是否已初始化
    bool IsInitialized() const { return initialized_; }

    /// 释放 GPU 资源
    void Destroy();

    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }

private:
    uint32_t texture_id_ = 0;
    uint32_t texture_handle_ = 0;   ///< RHI 句柄底层 id（== GL texture name）
    render::RhiDevice* rhi_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    PixelFormat format_ = PixelFormat::RGBA8;
    bool initialized_ = false;
};

} // namespace video
} // namespace dse

#endif // DSE_VIDEO_TEXTURE_H
