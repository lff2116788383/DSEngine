/**
 * @file virtual_geometry_renderer.cpp
 * @brief Main virtual geometry renderer implementation
 */

#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY

#include "engine/render/virtual_geometry/virtual_geometry_renderer.h"
#include <algorithm>
#include <cassert>

namespace dse {
namespace render {
namespace vg {

void VirtualGeometryRenderer::Init(const VirtualGeometryConfig& config,
                                    uint32_t screen_width, uint32_t screen_height) {
    config_ = config;
    screen_width_ = screen_width;
    screen_height_ = screen_height;

    culling_.Init(config);
    sw_raster_.Init(screen_width, screen_height);
    vis_buffer_.Init(screen_width, screen_height);
    streaming_.Init(config);

    initialized_ = true;
}

void VirtualGeometryRenderer::Shutdown() {
    culling_.Shutdown();
    sw_raster_.Shutdown();
    vis_buffer_.Shutdown();
    streaming_.Shutdown();
    mesh_registry_.clear();
    frame_instances_.clear();
    nanite_instances_.clear();
    traditional_instances_.clear();
    hw_draw_commands_.clear();
    initialized_ = false;
}

void VirtualGeometryRenderer::OnResize(uint32_t width, uint32_t height) {
    screen_width_ = width;
    screen_height_ = height;
    sw_raster_.Resize(width, height);
    vis_buffer_.Resize(width, height);
}

uint32_t VirtualGeometryRenderer::RegisterMesh(const std::string& name,
                                                const VirtualGeometryMesh& mesh) {
    uint32_t id = next_mesh_id_++;
    mesh_registry_[id] = {mesh, name};
    return id;
}

void VirtualGeometryRenderer::UnregisterMesh(uint32_t mesh_id) {
    mesh_registry_.erase(mesh_id);
}

void VirtualGeometryRenderer::BeginFrame(uint64_t frame_number) {
    frame_number_ = frame_number;
    frame_instances_.clear();
    nanite_instances_.clear();
    traditional_instances_.clear();
    hw_draw_commands_.clear();
    frame_stats_ = {};

    culling_.BeginFrame();
    sw_raster_.ClearVisBuffer();
    vis_buffer_.Clear();
    streaming_.BeginFrame(frame_number);
}

void VirtualGeometryRenderer::SubmitInstance(const VGInstance& instance) {
    frame_instances_.push_back(instance);
}

void VirtualGeometryRenderer::Execute(const glm::mat4& view, const glm::mat4& proj,
                                       const glm::vec3& camera_pos, float fov_y) {
    if (!initialized_ || !config_.enabled) return;

    CollectInstances();
    RunLODSelection(view, proj, camera_pos, fov_y);

    glm::mat4 view_proj = proj * view;

    if (config_.enable_software_rasterizer && config_.enable_visibility_buffer) {
        RunSoftwareRasterizer(view_proj);
        RunVisBufferResolve(view_proj, camera_pos);
    }

    BuildHWDrawCommands();

    // Update streaming
    if (config_.enable_cluster_streaming) {
        std::unordered_map<uint32_t, const VirtualGeometryMesh*> mesh_ptrs;
        for (auto& [id, entry] : mesh_registry_) {
            mesh_ptrs[id] = &entry.mesh;
        }
        streaming_.ProcessLoads(mesh_ptrs);
        frame_stats_.resident_clusters = streaming_.GetStats().resident_clusters;
        frame_stats_.streaming_loads = streaming_.GetStats().loads_this_frame;
        frame_stats_.streaming_evictions = streaming_.GetStats().evictions_this_frame;
    }
}

void VirtualGeometryRenderer::CollectInstances() {
    for (auto& inst : frame_instances_) {
        ++frame_stats_.total_instances;
        if (inst.nanite_static && config_.enable_visibility_buffer) {
            nanite_instances_.push_back(inst);
            ++frame_stats_.nanite_static_instances;
        } else {
            traditional_instances_.push_back(inst);
            ++frame_stats_.traditional_instances;
        }
    }

    // Feed nanite instances to culling system
    std::unordered_map<uint32_t, std::vector<VGInstance>> by_mesh;
    for (auto& inst : nanite_instances_) {
        by_mesh[inst.mesh_id].push_back(inst);
    }
    // Also feed traditional instances (they go through cluster indirect, not VisBuffer)
    for (auto& inst : traditional_instances_) {
        by_mesh[inst.mesh_id].push_back(inst);
    }

    for (auto& [mesh_id, instances] : by_mesh) {
        auto it = mesh_registry_.find(mesh_id);
        if (it == mesh_registry_.end()) continue;
        culling_.AddInstances(mesh_id, it->second.mesh, instances);
    }
}

void VirtualGeometryRenderer::RunLODSelection(const glm::mat4& view, const glm::mat4& proj,
                                                const glm::vec3& camera_pos, float fov_y) {
    auto result = culling_.SelectLOD_CPU(view, proj, camera_pos,
                                          static_cast<float>(screen_height_), config_);

    frame_stats_.selected_clusters = static_cast<uint32_t>(result.selected_clusters.size());
    frame_stats_.hw_raster_clusters = static_cast<uint32_t>(result.hw_raster_clusters.size());
    frame_stats_.sw_raster_clusters = static_cast<uint32_t>(result.sw_raster_clusters.size());
    frame_stats_.total_triangles = result.total_triangles;

    // Request streaming for selected clusters
    if (config_.enable_cluster_streaming) {
        for (auto& rm : culling_.GetResult().selected_clusters) {
            // rm is a cluster index; we need to map back to mesh_id
            // For now, use mesh_id=0 as placeholder (streaming keyed by mesh_id + cluster_index)
            streaming_.RequestCluster(0, rm);
        }
    }
}

void VirtualGeometryRenderer::RunSoftwareRasterizer(const glm::mat4& view_proj) {
    const auto& result = culling_.GetResult();
    if (result.sw_raster_clusters.empty()) return;

    // For each nanite instance, prepare SW raster triangles
    for (auto& inst : nanite_instances_) {
        auto it = mesh_registry_.find(inst.mesh_id);
        if (it == mesh_registry_.end()) continue;

        sw_raster_.PrepareTriangles(result.sw_raster_clusters,
                                    it->second.mesh,
                                    inst.model, view_proj,
                                    inst.material_id, 0);
    }

    sw_raster_.RasterizeCPU();
    frame_stats_.sw_pixels_written = sw_raster_.GetStats().pixels_written;

    // Merge into visibility buffer
    vis_buffer_.MergeSoftwareResults(sw_raster_.GetVisBuffer(),
                                     sw_raster_.GetWidth(),
                                     sw_raster_.GetHeight());
}

void VirtualGeometryRenderer::BuildHWDrawCommands() {
    const auto& result = culling_.GetResult();
    hw_draw_commands_.clear();

    for (uint32_t ci : result.hw_raster_clusters) {
        // Look up the cluster across all registered meshes
        for (auto& [mesh_id, entry] : mesh_registry_) {
            if (ci >= entry.mesh.clusters.size()) continue;
            const auto& cluster = entry.mesh.clusters[ci];

            MeshletDrawCommand cmd{};
            cmd.count = cluster.triangle_count * 3;
            cmd.instance_count = 1;
            if (ci < entry.mesh.draw_ranges.size()) {
                cmd.first_index = entry.mesh.draw_ranges[ci].index_offset;
            }
            cmd.base_vertex = 0;
            cmd.base_instance = 0;
            hw_draw_commands_.push_back(cmd);
            break;
        }
    }
}

void VirtualGeometryRenderer::RunVisBufferResolve(const glm::mat4& view_proj,
                                                    const glm::vec3& camera_pos) {
    // Prepare vertex data for attribute recovery
    for (auto& inst : nanite_instances_) {
        auto it = mesh_registry_.find(inst.mesh_id);
        if (it == mesh_registry_.end()) continue;
        vis_buffer_.PrepareVertexData(it->second.mesh, inst.model);
    }

    // CPU resolve is available for debugging; GPU resolve done via shader
    // (The actual GPU resolve dispatch happens in the render pass)
}

}  // namespace vg
}  // namespace render
}  // namespace dse

#endif  // DSE_ENABLE_VIRTUAL_GEOMETRY
