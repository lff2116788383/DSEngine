/**
 * @file sprite_renderer.h
 * @brief 后端无关的最小 2D sprite 渲染器（B0：通用绘制原语「活体验证」）
 *
 * 用 B0 新增的通用原语（BindPipeline / BindUniformBuffer / BindTexture(2D) /
 * BindVertexBuffer / BindIndexBuffer / DrawIndexed）组合出「带纹理 quad」绘制，
 * 端到端验证 mesh/sprite 类消费者要用的每个新原语在三后端上都正确。
 *
 * 注意：这是契约验证脚手架，不替代生产 DrawSpriteBatch（含批处理 / SDF / 排序），
 * 后者的迁移属于 B2。复用既有 sprite2d 着色器（pos\@0/color\@1/uv\@2 + PerFrame UBO +
 * u_texture sampler2D），着色器与几何由 RhiDevice 内建资源访问器/CreateGpuBuffer 提供。
 */

#ifndef DSE_RENDER_SPRITE_RENDERER_H
#define DSE_RENDER_SPRITE_RENDERER_H

#include <glm/glm.hpp>

#include "engine/render/rhi/rhi_handle.h"

namespace dse {
namespace render {

class CommandBuffer;
class RhiDevice;

/**
 * @class SpriteRenderer
 * @brief 用通用原语绘制单个带纹理 quad。资源（PSO / 程序 / VB / IB / UBO）首帧懒创建。
 */
class SpriteRenderer {
public:
    /// 记录一次带纹理 quad 绘制。
    /// @param texture_handle 2D 纹理句柄（采样到 quad 上）
    /// @param vp             视图投影矩阵；quad 顶点位于裁剪空间 [-half,+half]，vp=单位即直接落在屏幕中央
    /// @param half_extent    quad 半边长（裁剪空间），如 0.5 表示占屏幕中央一半
    /// @param tint           顶点色（与纹理相乘），默认白
    void Draw(CommandBuffer& cmd, RhiDevice& device, unsigned int texture_handle,
              const glm::mat4& vp, float half_extent = 0.5f,
              const glm::vec4& tint = glm::vec4(1.0f));

    /// 释放内建资源（可选；设备析构时缓冲随之回收）
    void Shutdown(RhiDevice& device);

private:
    void EnsureResources(RhiDevice& device);

    unsigned int pso_ = 0;
    BufferHandle vbo_;
    BufferHandle ibo_;
    BufferHandle ubo_;
    bool init_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_SPRITE_RENDERER_H
