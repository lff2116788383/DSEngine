/**
 * @file skybox_renderer.h
 * @brief 后端无关的天空盒渲染器（A1：用通用绘制原语取代 CommandBuffer::DrawSkybox）
 *
 * 通过 CommandBuffer 的通用原语（BindPipeline /
 * PushConstants / BindTexture(TexCube) / BindVertexBuffer / Draw）组合出天空盒绘制。
 * 着色器与立方体几何为各后端预编译资源，经 RhiDevice 的内建资源访问器获取。
 */

#ifndef DSE_RENDER_SKYBOX_RENDERER_H
#define DSE_RENDER_SKYBOX_RENDERER_H

#include <glm/glm.hpp>

namespace dse {
namespace render {

class CommandBuffer;
class RhiDevice;

/**
 * @class SkyboxRenderer
 * @brief 用通用原语绘制天空盒。缓存天空盒专用 PSO（首帧懒创建）。
 */
class SkyboxRenderer {
public:
    /// 记录一次天空盒绘制。view/projection 为该相机的视图/投影矩阵；
    /// 内部移除 view 的平移分量（仅保留旋转），与原 DrawSkybox 行为一致。
    void Draw(CommandBuffer& cmd, RhiDevice& device, unsigned int cubemap_handle,
              const glm::mat4& view, const glm::mat4& projection);

private:
    unsigned int pso_ = 0;
    bool pso_init_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_SKYBOX_RENDERER_H
