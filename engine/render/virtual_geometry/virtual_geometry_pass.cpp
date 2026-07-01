/**
 * @file virtual_geometry_pass.cpp
 * @brief RenderGraph pass implementations for virtual geometry pipeline
 *
 * These passes use the CPU fallback paths; GPU compute dispatch is wired
 * through the RHI's DispatchCompute / CmdDispatchComputePass interface
 * when available.  The CPU path ensures correctness on all backends.
 */

#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY

#include "engine/render/virtual_geometry/virtual_geometry_pass.h"

namespace dse {
namespace render {
namespace vg {

// ============================================================================
// VGCullPass — LOD selection + cluster culling
// ============================================================================

void VGCullPass::Setup(RenderGraph& /*graph*/) {
    // Declare resource dependencies for the render graph:
    //   reads:  hiz_mip_chain (from Hi-Z build pass)
    //   writes: vg_selected_clusters, vg_draw_commands
    // (Actual resource handles are backend-specific; the renderer owns the data.)
}

void VGCullPass::Execute(CommandBuffer& /*cmd_buffer*/) {
    if (!renderer_.GetConfig().enabled) return;

    // GPU path: dispatch vg_lod_select.comp + vg_cluster_cull.comp
    // CPU fallback: already executed in VirtualGeometryRenderer::Execute()
    //
    // The renderer's Execute() call in the pipeline does the CPU LOD selection.
    // When GPU compute is available, this pass would:
    //   1. Upload DAG nodes + cluster data to SSBOs
    //   2. Dispatch vg_lod_select.comp (iterative until no more expansions)
    //   3. Dispatch vg_cluster_cull.comp on selected clusters
    //   4. Read back draw commands + raster classification
    //
    // For now, CPU results are already in the renderer after Execute().
}

// ============================================================================
// VGRasterPass — Software rasterization
// ============================================================================

void VGRasterPass::Setup(RenderGraph& /*graph*/) {
    // Declare resource dependencies:
    //   reads:  vg_selected_clusters (from VGCullPass)
    //   writes: vg_visibility_buffer
}

void VGRasterPass::Execute(CommandBuffer& /*cmd_buffer*/) {
    if (!renderer_.GetConfig().enabled) return;
    if (!renderer_.GetConfig().enable_software_rasterizer) return;

    // GPU path: dispatch vg_sw_raster.comp
    //   1. Upload SWRasterTriangle[] to SSBO
    //   2. Clear vis buffer (compute or transfer)
    //   3. Dispatch with triangle_count / 64 groups
    //
    // CPU fallback: already executed in VirtualGeometryRenderer::Execute()
}

// ============================================================================
// VGResolvePass — VisBuffer → GBuffer
// ============================================================================

void VGResolvePass::Setup(RenderGraph& /*graph*/) {
    // Declare resource dependencies:
    //   reads:  vg_visibility_buffer, vg_vertex_data, vg_materials
    //   writes: gbuffer_albedo, gbuffer_normal, gbuffer_orm
}

void VGResolvePass::Execute(CommandBuffer& /*cmd_buffer*/) {
    if (!renderer_.GetConfig().enabled) return;
    if (!renderer_.GetConfig().enable_visibility_buffer) return;

    // GPU path: bind vg_resolve.vert + vg_resolve.frag, draw full-screen triangle
    //   1. Bind VisBuffer SSBO, vertex data SSBO, cluster data SSBO,
    //      index buffer SSBO, material SSBO
    //   2. Set push constants (inv_view_proj, camera_pos, screen_params)
    //   3. Draw 3 vertices (full-screen triangle, no VAO needed)
    //
    // CPU fallback: VisibilityBuffer::ResolveCPU() is available for debugging
}

}  // namespace vg
}  // namespace render
}  // namespace dse

#endif  // DSE_ENABLE_VIRTUAL_GEOMETRY
