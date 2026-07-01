/**
 * @file meshlet_render_pass.cpp
 * @brief Meshlet Cluster 渲染管线 Pass 实现
 */

#include "engine/render/meshlet/meshlet_render_pass.h"
#include "engine/render/render_graph.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_gpu_driven.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"
#include "engine/render/render_scene_view.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_render.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/world.h"
#include "engine/platform/screen.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cstring>

namespace dse {
namespace render {

// ============================================================
// MeshletCullRenderPass
// ============================================================

void MeshletCullRenderPass::Setup(RenderGraph& graph) {
    auto hiz_mip = graph.DeclareResource("hiz_mip_chain");
    auto meshlet_visibility = graph.DeclareResource("meshlet_visibility");
    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, hiz_mip);
    graph.PassWriteWithState(pass, meshlet_visibility, ResourceState::UnorderedAccess);
    graph.MarkOutput(meshlet_visibility);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void MeshletCullRenderPass::Execute(CommandBuffer& /*cmd_buffer*/) {
    ctx_.meshlet_active_this_frame = false;
    ctx_.meshlet_draw_count = 0;
    ctx_.meshlet_total_clusters = 0;
    ctx_.meshlet_visible_clusters = 0;

    if (!ctx_.meshlet_enabled) return;
    if (cull_pass_.GetRegisteredMeshCount() == 0) return;

    // Collect instances from ECS or external registration
    CollectInstances();

    if (cull_pass_.GetInstanceCount() == 0) return;

    // Prepare per-frame GPU data (transform meshlets to world space, build draw commands)
    const auto& snap = *ctx_.snapshot;
    glm::mat4 view(1.0f), proj(1.0f);
    glm::vec3 camera_pos(0.0f);
    if (snap.camera_3d.valid) {
        view = snap.camera_3d.view;
        proj = glm::perspective(
            glm::radians(snap.camera_3d.fov),
            static_cast<float>(Screen::width()) / static_cast<float>(std::max(1, Screen::height())),
            snap.camera_3d.near_clip, snap.camera_3d.far_clip);
        camera_pos = snap.camera_3d.position;
    }

    uint32_t total = cull_pass_.PrepareGPUData(view, proj, camera_pos);
    if (total == 0) return;

    ctx_.meshlet_total_clusters = static_cast<int>(total);

    // Ensure GPU buffers are large enough
    EnsureBuffers(total);

    // Upload GPU data to SSBO
    UploadGPUData();

    // Try GPU cull first, fall back to CPU
    auto* rhi = ctx_.rhi_device;
    if (rhi && rhi->SupportsCompute() && ctx_.meshlet_cull_shader != 0 &&
        ctx_.render_targets.hiz_texture != 0) {
        DispatchGPUCull();
    } else {
        ExecuteCPUFallback();
    }

    // Upload culled draw commands to SSBO
    const auto& cmds = cull_pass_.GetDrawCommands();
    if (!cmds.empty() && ctx_.meshlet_draw_cmd_ssbo.id != 0 && rhi) {
        rhi->UpdateGpuBuffer(ctx_.meshlet_draw_cmd_ssbo, 0,
                             cmds.size() * sizeof(MeshletDrawCommand), cmds.data());
    }

    ctx_.meshlet_draw_count = static_cast<int>(cmds.size());
    ctx_.meshlet_visible_clusters = static_cast<int>(cull_pass_.GetVisibleMeshletCount());
    ctx_.meshlet_active_this_frame = ctx_.meshlet_visible_clusters > 0;
}

void MeshletCullRenderPass::EnsureShader() {
    if (shader_compiled_) return;
    shader_compiled_ = true;
    // Shader handle is injected via ctx_.meshlet_cull_shader by FramePipeline
}

void MeshletCullRenderPass::EnsureBuffers(uint32_t meshlet_count) {
    auto* rhi = ctx_.rhi_device;
    if (!rhi) return;

    size_t needed = static_cast<size_t>(meshlet_count);

    // MeshletGPUData SSBO
    if (needed > gpu_data_capacity_) {
        size_t new_cap = std::max(needed, gpu_data_capacity_ * 2);
        new_cap = std::max(new_cap, size_t(256));
        if (ctx_.meshlet_gpu_data_ssbo.id != 0)
            rhi->DeleteGpuBuffer(ctx_.meshlet_gpu_data_ssbo);
        GpuBufferDesc desc{};
        desc.size = new_cap * sizeof(MeshletGPUData);
        desc.usage = GpuBufferUsage::kStorage;
        desc.is_dynamic = true;
        desc.debug_name = "meshlet_gpu_data";
        ctx_.meshlet_gpu_data_ssbo = rhi->CreateGpuBuffer(desc, nullptr);
        gpu_data_capacity_ = new_cap;
    }

    // Draw command SSBO (also used as indirect draw buffer)
    if (needed > draw_cmd_capacity_) {
        size_t new_cap = std::max(needed, draw_cmd_capacity_ * 2);
        new_cap = std::max(new_cap, size_t(256));
        if (ctx_.meshlet_draw_cmd_ssbo.id != 0)
            rhi->DeleteGpuBuffer(ctx_.meshlet_draw_cmd_ssbo);
        GpuBufferDesc desc{};
        desc.size = new_cap * sizeof(MeshletDrawCommand);
        desc.usage = GpuBufferUsage::kStorage | GpuBufferUsage::kIndirect;
        desc.is_dynamic = true;
        desc.debug_name = "meshlet_draw_cmds";
        ctx_.meshlet_draw_cmd_ssbo = rhi->CreateGpuBuffer(desc, nullptr);
        draw_cmd_capacity_ = new_cap;
    }

    // Material entry SSBO
    if (needed > material_capacity_) {
        size_t new_cap = std::max(needed, material_capacity_ * 2);
        new_cap = std::max(new_cap, size_t(256));
        if (ctx_.meshlet_material_ssbo.id != 0)
            rhi->DeleteGpuBuffer(ctx_.meshlet_material_ssbo);
        GpuBufferDesc desc{};
        desc.size = new_cap * sizeof(MeshletMaterialEntry);
        desc.usage = GpuBufferUsage::kStorage;
        desc.is_dynamic = true;
        desc.debug_name = "meshlet_materials";
        ctx_.meshlet_material_ssbo = rhi->CreateGpuBuffer(desc, nullptr);
        material_capacity_ = new_cap;
    }
}

void MeshletCullRenderPass::CollectInstances() {
    cull_pass_.BeginFrame();
    material_entries_.clear();

    // Collect from ECS: entities with MeshletMeshComponent
    if (!ctx_.world) return;
    auto& reg = ctx_.world->registry();

    // Look for entities that have registered meshlet mesh IDs
    // In production this would use a dedicated MeshletMeshComponent;
    // for now we use the existing MeshRendererComponent with meshlet data attached
    auto view = reg.view<TransformComponent, MeshRendererComponent>();
    for (auto entity : view) {
        auto& mesh_comp = view.get<MeshRendererComponent>(entity);
        if (mesh_comp.meshlet_mesh_id == 0) continue; // No meshlet data

        auto& tf = view.get<TransformComponent>(entity);
        cull_pass_.AddInstance(mesh_comp.meshlet_mesh_id, tf.local_to_world);

        // Material mapping (one entry per meshlet of this instance)
        auto it = cull_pass_.GetMeshRegistry().find(mesh_comp.meshlet_mesh_id);
        if (it != cull_pass_.GetMeshRegistry().end()) {
            uint32_t count = static_cast<uint32_t>(it->second.mesh_data.meshlets.size());
            for (uint32_t i = 0; i < count; ++i) {
                MeshletMaterialEntry entry{};
                entry.material_index = mesh_comp.material_index;
                material_entries_.push_back(entry);
            }
        }
    }
}

void MeshletCullRenderPass::UploadGPUData() {
    auto* rhi = ctx_.rhi_device;
    if (!rhi) return;

    const auto& gpu_data = cull_pass_.GetMeshletGPUData();
    if (!gpu_data.empty() && ctx_.meshlet_gpu_data_ssbo.id != 0) {
        rhi->UpdateGpuBuffer(ctx_.meshlet_gpu_data_ssbo, 0,
                             gpu_data.size() * sizeof(MeshletGPUData), gpu_data.data());
    }

    // Upload initial draw commands (pre-cull, all visible)
    const auto& cmds = cull_pass_.GetDrawCommands();
    if (!cmds.empty() && ctx_.meshlet_draw_cmd_ssbo.id != 0) {
        rhi->UpdateGpuBuffer(ctx_.meshlet_draw_cmd_ssbo, 0,
                             cmds.size() * sizeof(MeshletDrawCommand), cmds.data());
    }

    // Upload material entries
    if (!material_entries_.empty() && ctx_.meshlet_material_ssbo.id != 0) {
        rhi->UpdateGpuBuffer(ctx_.meshlet_material_ssbo, 0,
                             material_entries_.size() * sizeof(MeshletMaterialEntry),
                             material_entries_.data());
    }
}

void MeshletCullRenderPass::DispatchGPUCull() {
    auto* rhi = ctx_.rhi_device;
    if (!rhi) return;

    const unsigned int shader = ctx_.meshlet_cull_shader;
    if (shader == 0) return;

    // Bind SSBOs
    rhi->BindGpuBuffer(ctx_.meshlet_gpu_data_ssbo, 0, false);   // read-only meshlet data
    rhi->BindGpuBuffer(ctx_.meshlet_draw_cmd_ssbo, 1, true);    // read-write draw commands

    // Bind Hi-Z texture
    const unsigned int hiz_gpu_tex = rhi->GetHiZGpuTexture(ctx_.render_targets.hiz_texture);
    if (hiz_gpu_tex != 0) {
        rhi->SetComputeTextureSampler(0, hiz_gpu_tex);
    }

    // Camera matrices
    const auto& snap = *ctx_.snapshot;
    glm::mat4 view_projection(1.0f);
    if (snap.camera_3d.valid) {
        const glm::mat4 clip_correction = rhi->GetProjectionCorrection();
        glm::mat4 projection = clip_correction * glm::perspective(
            glm::radians(snap.camera_3d.fov),
            static_cast<float>(Screen::width()) / static_cast<float>(std::max(1, Screen::height())),
            snap.camera_3d.near_clip, snap.camera_3d.far_clip);
        view_projection = projection * snap.camera_3d.view;

        // Normalize Z to [0,1] for GL backends (same logic as HiZCullPass)
        if (clip_correction[2][2] == 1.0f && clip_correction[3][2] == 0.0f) {
            glm::mat4 z_remap(1.0f);
            z_remap[2][2] = 0.5f;
            z_remap[3][2] = 0.5f;
            view_projection = z_remap * view_projection;
        }
    }

    const int mip_count = rhi->GetHiZMipCount(ctx_.render_targets.hiz_texture);
    const int meshlet_count = ctx_.meshlet_total_clusters;

    rhi->SetComputeUniformMat4(shader, "u_view_projection", &view_projection[0][0]);
    rhi->SetComputeUniformVec2f(shader, "u_screen_size",
                                static_cast<float>(Screen::width()),
                                static_cast<float>(Screen::height()));
    rhi->SetComputeUniformInt(shader, "u_mip_count", mip_count);
    rhi->SetComputeUniformInt(shader, "u_meshlet_count", meshlet_count);

    // Camera position for cone culling
    glm::vec3 cam_pos = snap.camera_3d.valid ? snap.camera_3d.position : glm::vec3(0.0f);
    rhi->SetComputeUniformVec3(shader, "u_camera_pos", cam_pos.x, cam_pos.y, cam_pos.z);

    unsigned int groups_x = (static_cast<unsigned int>(meshlet_count) + 63) / 64;
    rhi->DispatchCompute(shader, groups_x, 1, 1);
    rhi->ComputeMemoryBarrier();
}

void MeshletCullRenderPass::ExecuteCPUFallback() {
    const auto& snap = *ctx_.snapshot;
    glm::mat4 view_proj(1.0f);
    glm::vec3 camera_pos(0.0f);
    if (snap.camera_3d.valid) {
        glm::mat4 proj = glm::perspective(
            glm::radians(snap.camera_3d.fov),
            static_cast<float>(Screen::width()) / static_cast<float>(std::max(1, Screen::height())),
            snap.camera_3d.near_clip, snap.camera_3d.far_clip);
        view_proj = proj * snap.camera_3d.view;
        camera_pos = snap.camera_3d.position;
    }
    cull_pass_.CullCPU(view_proj, camera_pos, cull_config_);
}

// ============================================================
// MeshletDrawRenderPass
// ============================================================

void MeshletDrawRenderPass::Setup(RenderGraph& graph) {
    auto meshlet_visibility = graph.DeclareResource("meshlet_visibility");
    auto scene_color = graph.DeclareResource("scene_color");
    auto pass = graph.AddPass(GetName());
    graph.PassRead(pass, meshlet_visibility);
    graph.PassWrite(pass, scene_color);
    graph.PassSetExecute(pass, [this](CommandBuffer& cmd) { Execute(cmd); });
}

void MeshletDrawRenderPass::Execute(CommandBuffer& /*cmd_buffer*/) {
    if (!ctx_.meshlet_active_this_frame) return;
    if (ctx_.meshlet_draw_count <= 0) return;

    auto* rhi = ctx_.rhi_device;
    if (!rhi) return;

    EnsureMegaBuffer();

    // Bind Meshlet Mega VAO
    if (ctx_.meshlet_mega_vao.id != 0) {
        rhi->BindMegaVAO(ctx_.meshlet_mega_vao);
    }

    // Bind material SSBO for per-meshlet material lookup in shader
    if (ctx_.meshlet_material_ssbo.id != 0) {
        rhi->BindGpuBuffer(ctx_.meshlet_material_ssbo, 2, false);
    }

    // Setup GPU-Driven PBR shader (reuse existing infrastructure)
    if (ctx_.snapshot && ctx_.snapshot->camera_3d.valid) {
        const auto& cam = ctx_.snapshot->camera_3d;
        glm::mat4 proj = glm::perspective(
            glm::radians(cam.fov),
            static_cast<float>(Screen::width()) / static_cast<float>(std::max(1, Screen::height())),
            cam.near_clip, cam.far_clip);

        // Find directional light
        glm::vec3 light_dir(0.0f, -1.0f, 0.0f);
        glm::vec3 light_color(1.0f);
        float light_intensity = 1.0f;
        float ambient = 0.3f;

        if (ctx_.scene_view && !ctx_.scene_view->directional_lights.empty()) {
            const auto& dl = ctx_.scene_view->directional_lights[0];
            light_dir = dl.direction;
            light_color = dl.color;
            light_intensity = dl.intensity;
        }

        if (rhi->HasGPUDrivenPBRShader()) {
            rhi->SetupGPUDrivenPBRShader(cam.view, proj, cam.position,
                                          light_dir, light_color, light_intensity, ambient);
        }
    }

    // Issue multi-draw indirect
    if (ctx_.meshlet_draw_cmd_ssbo.id != 0) {
        rhi->MultiDrawIndexedIndirect(ctx_.meshlet_draw_cmd_ssbo.id,
                                       ctx_.meshlet_draw_count,
                                       sizeof(MeshletDrawCommand), 0);
    }

    rhi->UnbindVAO();
}

void MeshletDrawRenderPass::EnsureMegaBuffer() {
    if (mega_buffer_initialized_) return;

    auto* rhi = ctx_.rhi_device;
    if (!rhi) return;

    // Initial Mega Buffer allocation (will grow as meshes register)
    constexpr size_t kInitialVBOSize = 16 * 1024 * 1024;  // 16 MB
    constexpr size_t kInitialIBOSize = 8 * 1024 * 1024;   // 8 MB

    ctx_.meshlet_mega_vao = rhi->CreateMegaVAO(kInitialVBOSize, kInitialIBOSize,
                                                ctx_.meshlet_mega_vbo, ctx_.meshlet_mega_ibo);
    mega_vbo_capacity_ = kInitialVBOSize;
    mega_ibo_capacity_ = kInitialIBOSize;
    mega_buffer_initialized_ = true;
}

} // namespace render
} // namespace dse
