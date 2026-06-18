/**
 * @file sprite_batch_renderer.h
 * @brief 生产级 2D sprite 批渲染器（后端无关，仅用通用绘制原语）。
 *
 * 替代旧 ABI CommandBuffer::DrawSpriteBatch（B2a）。复刻其语义：
 *   - 世界空间 quad 顶点（item.model * 单位 quad + UV 子矩形 + 顶点色）；
 *   - 按 texture/material/shader_variant/blend/vfx/sdf 游程批处理（仅合并相邻同态项，保序）；
 *   - 每批选程序（sprite2d 默认 / TEXT_SDF 文本 / ui_effects 特效）+ 选混合 PSO
 *     + 设 push-block UBO 参数 + 绑纹理 + DrawIndexed。
 *
 * 资源（PSO / 程序 / 动态 VB / 静态 IB / PerFrame UBO / 参数 UBO / 白纹理）首帧懒建。
 * 调用方负责排序与 view/proj（旧 ABI 经 SetCamera 取，迁移后直接传入）。
 */

#ifndef DSE_RENDER_SPRITE_BATCH_RENDERER_H
#define DSE_RENDER_SPRITE_BATCH_RENDERER_H

#include <glm/glm.hpp>
#include <vector>

#include "engine/render/rhi/rhi_handle.h"
#include "engine/render/rhi/rhi_types.h"

namespace dse {
namespace render {

class CommandBuffer;
class RhiDevice;

/**
 * @class SpriteBatchRenderer
 * @brief 用通用原语绘制一批 2D sprite。资源帧间懒建并复用。
 */
class SpriteBatchRenderer {
public:
    /// 绘制一批已排好序的 sprite。items 为空则直接返回。
    void Draw(CommandBuffer& cmd, RhiDevice& device,
              const std::vector<SpriteDrawItem>& items,
              const glm::mat4& view, const glm::mat4& projection);

    /// 释放内部 GPU 资源（析构/重置时调用）。
    void Shutdown(RhiDevice& device);

private:
    void EnsureResources(RhiDevice& device, size_t needed_quads);
    unsigned int PsoForBlend(RhiDevice& device, unsigned int blend_mode);

    unsigned int pso_alpha_ = 0;
    unsigned int pso_additive_ = 0;
    unsigned int pso_multiply_ = 0;
    unsigned int white_tex_ = 0;

    BufferHandle vbo_;   ///< 动态顶点缓冲（按需扩容）
    BufferHandle ibo_;   ///< 静态 quad 索引（0,1,2,0,2,3 重复）
    BufferHandle ubo_;   ///< PerFrame（std140，176B；着色器仅用 vp）
    BufferHandle sdf_ubo_;  ///< TEXT_SDF 参数 push-block
    BufferHandle vfx_ubo_;  ///< ui_effects 参数 push-block

    size_t vbo_cap_quads_ = 0;
    size_t ibo_cap_quads_ = 0;
    bool init_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_SPRITE_BATCH_RENDERER_H
