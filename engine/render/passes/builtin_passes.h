/**
 * @file builtin_passes.h
 * @brief 引擎内置渲染 Pass 声明
 */

#ifndef DSE_RENDER_PASSES_BUILTIN_PASSES_H
#define DSE_RENDER_PASSES_BUILTIN_PASSES_H

#include "engine/render/passes/render_pass_interface.h"
#include "engine/render/passes/render_pass_context.h"
#include "engine/render/render_graph.h"
#include <glm/glm.hpp>

namespace dse {
namespace render {

// ---- PreZ Pass ----
class PreZPass : public IRenderPass {
public:
    explicit PreZPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "prez_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- CSM Shadow Pass ----
class CSMShadowPass : public IRenderPass {
public:
    explicit CSMShadowPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "shadow_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- Spot Shadow Pass ----
class SpotShadowPass : public IRenderPass {
public:
    explicit SpotShadowPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "spot_shadow_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- Point Shadow Pass ----
class PointShadowPass : public IRenderPass {
public:
    explicit PointShadowPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "point_shadow_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- GBuffer Pass (deferred geometry) ----
class GBufferPass : public IRenderPass {
public:
    explicit GBufferPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "gbuffer_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- Deferred Lighting Pass ----
class DeferredLightingPass : public IRenderPass {
public:
    explicit DeferredLightingPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "deferred_lighting_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- Forward Scene Pass ----
class ForwardScenePass : public IRenderPass {
public:
    explicit ForwardScenePass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "scene_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- Post Process (Bloom) Pass ----
class BloomPass : public IRenderPass {
public:
    explicit BloomPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "post_process_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- SSAO Pass ----
class SSAOPass : public IRenderPass {
public:
    explicit SSAOPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "ssao_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- Contact Shadow Pass ----
class ContactShadowPass : public IRenderPass {
public:
    explicit ContactShadowPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "contact_shadow_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- FXAA Pass ----
class FXAAPass : public IRenderPass {
public:
    explicit FXAAPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "fxaa_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- Auto Exposure Pass ----
class AutoExposurePass : public IRenderPass {
public:
    explicit AutoExposurePass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "auto_exposure_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- UI Pass ----
class UIPass : public IRenderPass {
public:
    explicit UIPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "ui_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- Composite Pass ----
class CompositePass : public IRenderPass {
public:
    explicit CompositePass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "composite_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- TAA Pass ----
class TAAPass : public IRenderPass {
public:
    explicit TAAPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "taa_pass"; }

    /// 每帧由 frame_pipeline 调用：更新 jitter 偏移
    void UpdateJitter(int frame_index);

    /// 获取当前帧 jitter 值（供 FramePipeline 修改投影矩阵）
    glm::vec2 GetCurrentJitter() const { return current_jitter_; }

private:
    RenderPassContext& ctx_;
    unsigned int history_rt_[2] = {0, 0}; ///< 双缓冲历史 RT (ping-pong)
    int history_index_ = 0;              ///< 当前写入索引
    int history_width_ = 0;              ///< 历史 RT 宽度
    int history_height_ = 0;             ///< 历史 RT 高度
    bool has_valid_history_ = false;     ///< 历史帧是否可用
    glm::vec2 current_jitter_ = {};
    int frame_index_ = 0;

    void EnsureHistoryRT(int width, int height);
    static float Halton(int index, int base);
};

// ---- DOF Pass ----
class DOFPass : public IRenderPass {
public:
    explicit DOFPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "dof_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- Motion Vector Pass ----
class MotionVectorPass : public IRenderPass {
public:
    explicit MotionVectorPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "motion_vector_pass"; }
private:
    RenderPassContext& ctx_;
    glm::mat4 prev_vp_ = glm::mat4(1.0f);
    bool has_prev_vp_ = false;
};

// ---- Motion Blur Pass ----
class MotionBlurPass : public IRenderPass {
public:
    explicit MotionBlurPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "motion_blur_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- SSR Pass ----
class SSRPass : public IRenderPass {
public:
    explicit SSRPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "ssr_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- Outline / Edge Detection Pass ----
class OutlinePass : public IRenderPass {
public:
    explicit OutlinePass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "outline_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- Light Shaft / God Ray Pass ----
class LightShaftPass : public IRenderPass {
public:
    explicit LightShaftPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "light_shaft_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- Volumetric Fog Pass ----
class VolumetricFogPass : public IRenderPass {
public:
    explicit VolumetricFogPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "volumetric_fog_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- WBOIT (Weighted Blended OIT) Pass ----
class WBOITPass : public IRenderPass {
public:
    explicit WBOITPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "wboit_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- Water / Ocean Pass ----
class WaterPass : public IRenderPass {
public:
    explicit WaterPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "water_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- Screen-Space Decal Pass ----
class DecalPass : public IRenderPass {
public:
    explicit DecalPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "decal_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- Present Pass (runtime only) ----
class PresentPass : public IRenderPass {
public:
    explicit PresentPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "present_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- Hi-Z Build Pass (Compute) ----
class HiZBuildPass : public IRenderPass {
public:
    explicit HiZBuildPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "hiz_build_pass"; }
private:
    RenderPassContext& ctx_;
    unsigned int hiz_copy_shader_ = 0;
    unsigned int hiz_downsample_shader_ = 0;
    bool shaders_compiled_ = false;
    void EnsureShaders();
};

// ---- Hi-Z Occlusion Cull Pass (Compute) ----
class HiZCullPass : public IRenderPass {
public:
    explicit HiZCullPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "hiz_cull_pass"; }
private:
    RenderPassContext& ctx_;
    unsigned int hiz_cull_shader_ = 0;
    bool shader_compiled_ = false;
    void EnsureShader();
};

// ---- GPU Driven Cull Pass (Compute) ----
class GPUCullPass : public IRenderPass {
public:
    explicit GPUCullPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "gpu_cull_pass"; }
private:
    RenderPassContext& ctx_;
};

// ---- DDGI Update Pass (Compute) ----
class DDGIUpdatePass : public IRenderPass {
public:
    explicit DDGIUpdatePass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "ddgi_update_pass"; }
private:
    RenderPassContext& ctx_;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_PASSES_BUILTIN_PASSES_H
