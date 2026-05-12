/**
 * @file builtin_passes.h
 * @brief 引擎内置渲染 Pass 声明
 */

#ifndef DSE_RENDER_PASSES_BUILTIN_PASSES_H
#define DSE_RENDER_PASSES_BUILTIN_PASSES_H

#include "engine/render/passes/render_pass_interface.h"
#include "engine/render/passes/render_pass_context.h"
#include "engine/render/render_graph.h"

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

} // namespace render
} // namespace dse

#endif // DSE_RENDER_PASSES_BUILTIN_PASSES_H
