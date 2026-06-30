/**
 * @file video_texture.h
 * @brief 视频帧 → GPU 纹理上传
 */

#ifndef DSE_VIDEO_TEXTURE_H
#define DSE_VIDEO_TEXTURE_H

#include "engine/video/video_types.h"
#include <cstdint>
#include <vector>

namespace dse {
namespace video {

class VideoTexture {
public:
    VideoTexture();
    ~VideoTexture();

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
    int width_ = 0;
    int height_ = 0;
    PixelFormat format_ = PixelFormat::RGBA8;
    bool initialized_ = false;
};

} // namespace video
} // namespace dse

#endif // DSE_VIDEO_TEXTURE_H
