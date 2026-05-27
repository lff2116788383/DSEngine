#ifndef DSE_RENDER_PASSES_ATMOSPHERE_SKY_PASS_H
#define DSE_RENDER_PASSES_ATMOSPHERE_SKY_PASS_H

#include "engine/render/passes/render_pass_interface.h"
#include "engine/render/passes/render_pass_context.h"
#include "engine/render/render_graph.h"

namespace dse {
namespace render {

/// 大气散射天空渲染 Pass
/// - 管理 Transmittance LUT 生命周期（参数变化时重算）
/// - 程序化天空渲染（替代 cubemap skybox）
/// - 写入 depth=far 的天空像素
class AtmosphereSkyPass : public IRenderPass {
public:
    explicit AtmosphereSkyPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "atmosphere_sky_pass"; }

    /// 获取 transmittance LUT 纹理句柄（供其他 Pass 使用，如 aerial perspective）
    unsigned int GetTransmittanceLUT() const { return transmittance_lut_; }

    /// 是否有有效的大气天空实体
    bool IsActive() const { return active_; }

private:
    RenderPassContext& ctx_;

    // Transmittance LUT 资源
    unsigned int transmittance_lut_ = 0;         ///< 2D R16F texture
    unsigned int transmittance_rt_ = 0;          ///< RT 用于渲染 LUT
    int lut_width_ = 0;
    int lut_height_ = 0;
    bool lut_valid_ = false;

    // 上一帧参数哈希（检测变化）
    size_t prev_params_hash_ = 0;

    bool active_ = false;

    void EnsureTransmittanceLUT(int width, int height);
    void RenderTransmittanceLUT(CommandBuffer& cmd_buffer);
    size_t ComputeParamsHash() const;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_PASSES_ATMOSPHERE_SKY_PASS_H
