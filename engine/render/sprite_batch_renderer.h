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

#include "engine/render/rhi/per_in_flight_buffer.h"
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
    void EnsureFxUbos(RhiDevice& device, size_t needed);
    unsigned int PsoForBlend(RhiDevice& device, unsigned int blend_mode);

    unsigned int pso_alpha_ = 0;
    unsigned int pso_additive_ = 0;
    unsigned int pso_multiply_ = 0;
    unsigned int white_tex_ = 0;

    /// 动态顶点缓冲（按需扩容）。每帧覆写 → 每在飞帧缓冲（规避 2 帧在飞下的覆写竞争，D9）。
    PerInFlightBuffer vbo_;
    BufferHandle ibo_;   ///< 静态 quad 索引（0,1,2,0,2,3 重复）；非每帧写，单缓冲即可
    /// PerFrame（std140，176B；默认 sprite2d 路径仅用 vp）。每帧覆写 → 每在飞帧缓冲。
    PerInFlightBuffer ubo_;
    /// SDF/VFX 批的 push-block 参数 UBO 池（SpriteFx 布局，128B/个）。每 fx 批一个独立逻辑
    /// 缓冲（参数互异），每帧覆写 → 各自每在飞帧缓冲（满足 Vulkan「提交前不可别名/覆写」约束）。
    std::vector<PerInFlightBuffer> fx_ubos_;

    size_t ibo_cap_quads_ = 0;
    bool init_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_SPRITE_BATCH_RENDERER_H
