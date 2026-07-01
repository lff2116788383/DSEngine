/**
 * @file virtual_geometry_pass.h
 * @brief RenderGraph passes for virtual geometry pipeline
 *
 * Three passes inserted into the render graph when VG is enabled:
 *
 *   VGCullPass     — LOD selection + cluster culling (compute)
 *   VGRasterPass   — Software rasterization of small triangles (compute)
 *   VGResolvePass  — VisBuffer → GBuffer resolve (full-screen fragment)
 *
 * Hardware-raster clusters are drawn via the existing MeshletDrawRenderPass
 * using the draw commands produced by VGCullPass.
 */

#ifndef DSE_VIRTUAL_GEOMETRY_PASS_H
#define DSE_VIRTUAL_GEOMETRY_PASS_H

#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY

#include "engine/render/passes/render_pass_interface.h"
#include "engine/render/passes/render_pass_context.h"
#include <memory>

namespace dse {
namespace render {

class RenderGraph;
class CommandBuffer;
class RhiDevice;

namespace vg {

class VirtualGeometryRenderer;

/// Compute pass: LOD selection + cluster frustum/occlusion culling
class VGCullPass : public IRenderPass {
public:
    explicit VGCullPass(RenderPassContext& ctx)
        : ctx_(ctx) {}

    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "vg_cull"; }
    void InitShaders(RhiDevice* rhi);

private:
    VirtualGeometryRenderer* GetRenderer() const;
    RenderPassContext& ctx_;
    bool shader_compiled_ = false;
};

/// Compute pass: software rasterization → VisBuffer
class VGRasterPass : public IRenderPass {
public:
    explicit VGRasterPass(RenderPassContext& ctx)
        : ctx_(ctx) {}

    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "vg_sw_raster"; }
    void InitShaders(RhiDevice* rhi);

private:
    VirtualGeometryRenderer* GetRenderer() const;
    RenderPassContext& ctx_;
    bool shader_compiled_ = false;
};

/// Full-screen pass: VisBuffer resolve → GBuffer output
class VGResolvePass : public IRenderPass {
public:
    explicit VGResolvePass(RenderPassContext& ctx)
        : ctx_(ctx) {}

    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "vg_resolve"; }
    void InitShaders(RhiDevice* rhi);

private:
    VirtualGeometryRenderer* GetRenderer() const;
    RenderPassContext& ctx_;
    bool shader_compiled_ = false;
};

}  // namespace vg
}  // namespace render
}  // namespace dse

#endif  // DSE_ENABLE_VIRTUAL_GEOMETRY
#endif  // DSE_VIRTUAL_GEOMETRY_PASS_H
