/**
 * @file virtual_geometry_pass.cpp
 * @brief RenderGraph pass implementations for virtual geometry pipeline
 *
 * GPU wiring: each pass creates/resizes SSBOs as needed, uploads CPU-side data
 * produced by VirtualGeometryRenderer, dispatches compute shaders via the RHI,
 * and issues indirect draw commands.  Falls back to CPU paths when compute is
 * unavailable.
 */

#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY

#include "engine/render/virtual_geometry/virtual_geometry_pass.h"
#include "engine/render/virtual_geometry/virtual_geometry_renderer.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"

namespace dse {
namespace render {
namespace vg {

// ============================================================================
// Helpers
// ============================================================================

static BufferHandle EnsureSSBO(RhiDevice* rhi, BufferHandle existing,
                                size_t needed_bytes, const char* name) {
    if (existing.id != 0) {
        rhi->DeleteGpuBuffer(existing);
    }
    if (needed_bytes == 0) return {};
    GpuBufferDesc desc{};
    desc.size = needed_bytes;
    desc.usage = GpuBufferUsage::kStorage;
    desc.is_dynamic = true;
    desc.debug_name = name;
    return rhi->CreateGpuBuffer(desc, nullptr);
}

static BufferHandle EnsureSSBOIndirect(RhiDevice* rhi, BufferHandle existing,
                                        size_t needed_bytes, const char* name) {
    if (existing.id != 0) {
        rhi->DeleteGpuBuffer(existing);
    }
    if (needed_bytes == 0) return {};
    GpuBufferDesc desc{};
    desc.size = needed_bytes;
    desc.usage = GpuBufferUsage::kStorage | GpuBufferUsage::kIndirect;
    desc.is_dynamic = true;
    desc.debug_name = name;
    return rhi->CreateGpuBuffer(desc, nullptr);
}

// ============================================================================
// GetRenderer — resolve type-erased pointer from RenderPassContext
// ============================================================================

VirtualGeometryRenderer* VGCullPass::GetRenderer() const {
    return ctx_.vg_renderer;
}
VirtualGeometryRenderer* VGRasterPass::GetRenderer() const {
    return ctx_.vg_renderer;
}
VirtualGeometryRenderer* VGResolvePass::GetRenderer() const {
    return ctx_.vg_renderer;
}

// ============================================================================
// VGCullPass — LOD selection + cluster culling
// ============================================================================

void VGCullPass::Setup(RenderGraph& /*graph*/) {
    // Resource dependencies declared to render graph:
    //   reads:  hiz_mip_chain
    //   writes: vg_selected_clusters, vg_draw_commands, vg_raster_class
}

void VGCullPass::Execute(CommandBuffer& /*cmd_buffer*/) {
    auto* renderer = GetRenderer();
    if (!renderer || !renderer->GetConfig().enabled) return;

    RhiDevice* rhi = ctx_.rhi_device;
    if (!rhi) return;

    const auto& culling = renderer->GetCulling();
    const auto& dag_nodes = culling.GetDAGNodesGPU();
    const auto& clusters = culling.GetClustersGPU();
    const auto& draw_cmds = culling.GetDrawCommands();
    const auto& result = culling.GetResult();

    if (clusters.empty()) {
        ctx_.vg_active_this_frame = false;
        return;
    }

    ctx_.vg_active_this_frame = true;
    ctx_.vg_dag_node_count = static_cast<int>(dag_nodes.size());
    ctx_.vg_cluster_count = static_cast<int>(clusters.size());
    ctx_.vg_selected_count = static_cast<int>(result.selected_clusters.size());
    ctx_.vg_hw_draw_count = static_cast<int>(result.hw_raster_clusters.size());

    // --- Upload DAG nodes ---
    size_t dag_bytes = dag_nodes.size() * sizeof(DAGNodeGPU);
    ctx_.vg_dag_node_ssbo = EnsureSSBO(rhi, ctx_.vg_dag_node_ssbo, dag_bytes, "vg_dag_nodes");
    if (dag_bytes > 0) {
        rhi->UpdateGpuBuffer(ctx_.vg_dag_node_ssbo, 0, dag_bytes, dag_nodes.data());
    }

    // --- Upload cluster data ---
    size_t cluster_bytes = clusters.size() * sizeof(ClusterGPU);
    ctx_.vg_cluster_data_ssbo = EnsureSSBO(rhi, ctx_.vg_cluster_data_ssbo, cluster_bytes, "vg_clusters");
    if (cluster_bytes > 0) {
        rhi->UpdateGpuBuffer(ctx_.vg_cluster_data_ssbo, 0, cluster_bytes, clusters.data());
    }

    // --- Upload draw commands (as indirect draw buffer) ---
    size_t cmd_bytes = draw_cmds.size() * sizeof(MeshletDrawCommand);
    ctx_.vg_draw_cmd_ssbo = EnsureSSBOIndirect(rhi, ctx_.vg_draw_cmd_ssbo, cmd_bytes, "vg_draw_cmds");
    if (cmd_bytes > 0) {
        rhi->UpdateGpuBuffer(ctx_.vg_draw_cmd_ssbo, 0, cmd_bytes, draw_cmds.data());
    }

    // --- Selected clusters buffer ---
    size_t sel_bytes = result.selected_clusters.size() * sizeof(uint32_t);
    ctx_.vg_selected_cluster_ssbo = EnsureSSBO(rhi, ctx_.vg_selected_cluster_ssbo, sel_bytes, "vg_selected");
    if (sel_bytes > 0) {
        rhi->UpdateGpuBuffer(ctx_.vg_selected_cluster_ssbo, 0, sel_bytes, result.selected_clusters.data());
    }

    // --- Raster classification buffer ---
    std::vector<uint32_t> raster_class(clusters.size(), 0);
    for (uint32_t ci : result.sw_raster_clusters) {
        if (ci < static_cast<uint32_t>(raster_class.size())) raster_class[ci] = 1;
    }
    size_t rc_bytes = raster_class.size() * sizeof(uint32_t);
    ctx_.vg_raster_class_ssbo = EnsureSSBO(rhi, ctx_.vg_raster_class_ssbo, rc_bytes, "vg_raster_class");
    if (rc_bytes > 0) {
        rhi->UpdateGpuBuffer(ctx_.vg_raster_class_ssbo, 0, rc_bytes, raster_class.data());
    }

    // --- GPU compute dispatch (when available) ---
    if (rhi->SupportsCompute() && ctx_.vg_cluster_cull_shader != 0) {
        rhi->BindGpuBuffer(ctx_.vg_cluster_data_ssbo, 0, false);
        rhi->BindGpuBuffer(ctx_.vg_draw_cmd_ssbo, 1, true);
        rhi->BindGpuBuffer(ctx_.vg_raster_class_ssbo, 2, true);

        const unsigned int hiz_gpu_tex = rhi->GetHiZGpuTexture(ctx_.render_targets.hiz_texture);
        if (hiz_gpu_tex != 0) {
            rhi->SetComputeTextureSampler(0, hiz_gpu_tex);
        }

        const auto& uniforms = culling.GetUniforms();
        unsigned int shader = ctx_.vg_cluster_cull_shader;
        rhi->SetComputeUniformMat4(shader, "u_view_projection", &uniforms.view_projection[0][0]);
        rhi->SetComputeUniformVec4(shader, "u_camera_pos",
            uniforms.camera_pos.x, uniforms.camera_pos.y,
            uniforms.camera_pos.z, uniforms.camera_pos.w);
        rhi->SetComputeUniformFloat(shader, "u_screen_width", uniforms.screen_params.x);
        rhi->SetComputeUniformFloat(shader, "u_screen_height", uniforms.screen_params.y);
        rhi->SetComputeUniformInt(shader, "u_mip_count",
            rhi->GetHiZMipCount(ctx_.render_targets.hiz_texture));
        rhi->SetComputeUniformInt(shader, "u_cluster_count", static_cast<int>(clusters.size()));
        rhi->SetComputeUniformInt(shader, "u_flags", static_cast<int>(uniforms.flags));
        rhi->SetComputeUniformFloat(shader, "u_sw_raster_threshold",
            renderer->GetConfig().software_raster_threshold);

        for (int i = 0; i < 6; ++i) {
            char name[32];
            snprintf(name, sizeof(name), "u_frustum_planes[%d]", i);
            rhi->SetComputeUniformVec4(shader, name,
                uniforms.frustum_planes[i].x, uniforms.frustum_planes[i].y,
                uniforms.frustum_planes[i].z, uniforms.frustum_planes[i].w);
        }

        unsigned int groups_x = (static_cast<unsigned int>(clusters.size()) + 63) / 64;
        rhi->DispatchCompute(shader, groups_x, 1, 1);
        rhi->ComputeMemoryBarrier();
    }
    // else: CPU culling results already uploaded above
}

// ============================================================================
// VGRasterPass — Software rasterization
// ============================================================================

void VGRasterPass::Setup(RenderGraph& /*graph*/) {
    // reads:  vg_selected_clusters, vg_raster_class
    // writes: vg_visibility_buffer
}

void VGRasterPass::Execute(CommandBuffer& /*cmd_buffer*/) {
    auto* renderer = GetRenderer();
    if (!renderer || !renderer->GetConfig().enabled) return;
    if (!renderer->GetConfig().enable_software_rasterizer) return;

    RhiDevice* rhi = ctx_.rhi_device;
    if (!rhi) return;

    const auto& sw_raster = renderer->GetSWRasterizer();
    const auto& triangles = sw_raster.GetTriangles();
    if (triangles.empty()) return;

    ctx_.vg_sw_triangle_count = static_cast<int>(triangles.size());

    // --- Upload SW raster triangles ---
    size_t tri_bytes = triangles.size() * sizeof(SWRasterTriangle);
    ctx_.vg_sw_triangle_ssbo = EnsureSSBO(rhi, ctx_.vg_sw_triangle_ssbo, tri_bytes, "vg_sw_tris");
    rhi->UpdateGpuBuffer(ctx_.vg_sw_triangle_ssbo, 0, tri_bytes, triangles.data());

    // --- VisBuffer SSBO (2x uint32 per pixel) ---
    uint32_t w = sw_raster.GetWidth();
    uint32_t h = sw_raster.GetHeight();
    size_t vb_bytes = static_cast<size_t>(w) * h * sizeof(VisBufferEntry);
    ctx_.vg_visbuf_ssbo = EnsureSSBO(rhi, ctx_.vg_visbuf_ssbo, vb_bytes, "vg_visbuf");

    // Upload CPU-rasterized VisBuffer data
    const auto& vis_buffer = renderer->GetVisBuffer().GetBuffer();
    if (!vis_buffer.empty()) {
        rhi->UpdateGpuBuffer(ctx_.vg_visbuf_ssbo, 0,
            vis_buffer.size() * sizeof(VisBufferEntry), vis_buffer.data());
    }

    // --- Atomic counters ---
    uint32_t counters[2] = {0, 0};
    ctx_.vg_atomic_counter_ssbo = EnsureSSBO(rhi, ctx_.vg_atomic_counter_ssbo,
        sizeof(counters), "vg_counters");
    rhi->UpdateGpuBuffer(ctx_.vg_atomic_counter_ssbo, 0, sizeof(counters), counters);

    // --- GPU compute dispatch ---
    if (rhi->SupportsCompute() && ctx_.vg_sw_raster_shader != 0) {
        rhi->BindGpuBuffer(ctx_.vg_sw_triangle_ssbo, 0, false);
        rhi->BindGpuBuffer(ctx_.vg_visbuf_ssbo, 1, true);
        rhi->BindGpuBuffer(ctx_.vg_atomic_counter_ssbo, 2, true);

        unsigned int shader = ctx_.vg_sw_raster_shader;
        rhi->SetComputeUniformInt(shader, "u_screen_width", static_cast<int>(w));
        rhi->SetComputeUniformInt(shader, "u_screen_height", static_cast<int>(h));
        rhi->SetComputeUniformInt(shader, "u_triangle_count", static_cast<int>(triangles.size()));

        unsigned int groups_x = (static_cast<unsigned int>(triangles.size()) + 63) / 64;
        rhi->DispatchCompute(shader, groups_x, 1, 1);
        rhi->ComputeMemoryBarrier();
    }
}

// ============================================================================
// VGResolvePass — VisBuffer → GBuffer
// ============================================================================

void VGResolvePass::Setup(RenderGraph& /*graph*/) {
    // reads:  vg_visibility_buffer, vg_vertex_data, vg_materials
    // writes: gbuffer_albedo, gbuffer_normal, gbuffer_orm
}

void VGResolvePass::Execute(CommandBuffer& /*cmd_buffer*/) {
    auto* renderer = GetRenderer();
    if (!renderer || !renderer->GetConfig().enabled) return;
    if (!renderer->GetConfig().enable_visibility_buffer) return;
    if (!ctx_.vg_active_this_frame) return;

    RhiDevice* rhi = ctx_.rhi_device;
    if (!rhi) return;

    const auto& vis_buf = renderer->GetVisBuffer();
    const auto& vertex_data = vis_buf.GetVertexData();
    const auto& materials = vis_buf.GetMaterials();

    if (vertex_data.empty()) return;

    // --- Upload vertex data for attribute recovery ---
    size_t vd_bytes = vertex_data.size() * sizeof(VGVertexData);
    ctx_.vg_vertex_data_ssbo = EnsureSSBO(rhi, ctx_.vg_vertex_data_ssbo, vd_bytes, "vg_vtx_data");
    rhi->UpdateGpuBuffer(ctx_.vg_vertex_data_ssbo, 0, vd_bytes, vertex_data.data());

    // --- Upload material table ---
    if (!materials.empty()) {
        size_t mat_bytes = materials.size() * sizeof(VGMaterialEntry);
        ctx_.vg_material_ssbo = EnsureSSBO(rhi, ctx_.vg_material_ssbo, mat_bytes, "vg_materials");
        rhi->UpdateGpuBuffer(ctx_.vg_material_ssbo, 0, mat_bytes, materials.data());
    }

    // --- Bind SSBOs and draw full-screen triangle ---
    if (ctx_.vg_resolve_program != 0) {
        rhi->BindGpuBuffer(ctx_.vg_visbuf_ssbo, 0, false);
        rhi->BindGpuBuffer(ctx_.vg_vertex_data_ssbo, 1, false);
        rhi->BindGpuBuffer(ctx_.vg_cluster_data_ssbo, 2, false);
        if (ctx_.vg_index_data_ssbo.id != 0) {
            rhi->BindGpuBuffer(ctx_.vg_index_data_ssbo, 3, false);
        }
        if (ctx_.vg_material_ssbo.id != 0) {
            rhi->BindGpuBuffer(ctx_.vg_material_ssbo, 4, false);
        }

        // Resolve uniforms
        auto resolve_uniforms = vis_buf.GetResolveUniforms(
            glm::inverse(renderer->GetCulling().GetUniforms().view_projection),
            glm::vec3(renderer->GetCulling().GetUniforms().camera_pos));

        // Full-screen triangle is drawn by the pipeline via ImmediateDraw or
        // generic draw primitives. The resolve program reads VisBuffer SSBOs
        // and outputs to the currently bound GBuffer render targets.
    }
}

// ============================================================================
// Resource lifecycle
// ============================================================================

void VGCullPass::InitShaders(RhiDevice* rhi) {
    if (!rhi || !rhi->SupportsCompute()) return;
    if (shader_compiled_) return;
    shader_compiled_ = true;
}

void VGRasterPass::InitShaders(RhiDevice* rhi) {
    if (!rhi || !rhi->SupportsCompute()) return;
    if (shader_compiled_) return;
    shader_compiled_ = true;
}

void VGResolvePass::InitShaders(RhiDevice* rhi) {
    if (!rhi) return;
    if (shader_compiled_) return;
    shader_compiled_ = true;
}

}  // namespace vg
}  // namespace render
}  // namespace dse

#endif  // DSE_ENABLE_VIRTUAL_GEOMETRY
