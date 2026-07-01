/**
 * @file gpu_cluster_culling.cpp
 * @brief CPU-side logic for GPU cluster culling + LOD selection
 *
 * The CPU fallback path walks the DAG top-down:
 *   For each root node, compute screen-space error.
 *   If error < threshold → use this (coarser) node's cluster.
 *   Else → recurse into children (finer detail).
 *   Leaf nodes are always selected if reached.
 *
 * Selected clusters are then frustum + cone culled, and split into
 * hardware vs software rasterization buckets by screen-space triangle size.
 */

#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY

#include "engine/render/virtual_geometry/gpu_cluster_culling.h"
#include <algorithm>
#include <cassert>
#include <cmath>

namespace dse {
namespace render {
namespace vg {

void GPUClusterCulling::Init(const VirtualGeometryConfig& config) {
    initialized_ = true;
}

void GPUClusterCulling::Shutdown() {
    frame_meshes_.clear();
    dag_nodes_gpu_.clear();
    clusters_gpu_.clear();
    draw_commands_.clear();
    initialized_ = false;
}

void GPUClusterCulling::BeginFrame() {
    frame_meshes_.clear();
    dag_nodes_gpu_.clear();
    clusters_gpu_.clear();
    draw_commands_.clear();
    result_ = {};
    total_cluster_count_ = 0;
}

void GPUClusterCulling::AddInstances(uint32_t mesh_id,
                                      const VirtualGeometryMesh& mesh,
                                      const std::vector<VGInstance>& instances) {
    frame_meshes_.push_back({&mesh, instances});
}

void GPUClusterCulling::ExtractFrustumPlanes(const glm::mat4& vp, glm::vec4 planes[6]) {
    // Left
    planes[0] = glm::vec4(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0],
                           vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
    // Right
    planes[1] = glm::vec4(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0],
                           vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
    // Bottom
    planes[2] = glm::vec4(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1],
                           vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
    // Top
    planes[3] = glm::vec4(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1],
                           vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
    // Near
    planes[4] = glm::vec4(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2],
                           vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]);
    // Far
    planes[5] = glm::vec4(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2],
                           vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);

    for (int i = 0; i < 6; ++i) {
        float len = glm::length(glm::vec3(planes[i]));
        if (len > 0.0f) planes[i] /= len;
    }
}

float GPUClusterCulling::ComputeScreenSpaceError(const DAGNode& node,
                                                   const glm::mat4& view_proj,
                                                   const glm::vec3& camera_pos,
                                                   float screen_height,
                                                   float fov_y) {
    float dist = glm::length(node.bound_center - camera_pos);
    if (dist < 0.001f) dist = 0.001f;

    // Project object-space error to screen pixels
    // screen_error = (object_error / distance) * (screen_height / (2 * tan(fov/2)))
    float proj_factor = screen_height / (2.0f * std::tan(fov_y * 0.5f));
    return (node.lod_error / dist) * proj_factor;
}

float GPUClusterCulling::ComputeScreenSpaceSize(const glm::vec3& center, float radius,
                                                  const glm::mat4& view_proj,
                                                  float screen_height) {
    glm::vec4 clip = view_proj * glm::vec4(center, 1.0f);
    if (clip.w <= 0.0f) return 0.0f;
    float proj_radius = radius / clip.w;
    return proj_radius * screen_height * 0.5f;
}

LODSelectionResult GPUClusterCulling::SelectLOD_CPU(
        const glm::mat4& view, const glm::mat4& proj,
        const glm::vec3& camera_pos,
        float screen_height,
        const VirtualGeometryConfig& config) {
    LODSelectionResult result;
    glm::mat4 view_proj = proj * view;

    glm::vec4 frustum_planes[6];
    ExtractFrustumPlanes(view_proj, frustum_planes);

    float fov_y = 2.0f * std::atan(1.0f / proj[1][1]);

    for (auto& rm : frame_meshes_) {
        const auto& mesh = *rm.mesh;

        for (auto& inst : rm.instances) {
            // Walk DAG top-down for this instance
            // Find root nodes (nodes with no parent)
            std::vector<uint32_t> roots;
            for (uint32_t i = 0; i < mesh.dag_nodes.size(); ++i) {
                if (mesh.dag_nodes[i].parent == UINT32_MAX)
                    roots.push_back(i);
            }

            // DFS from each root
            std::vector<uint32_t> stack;
            for (uint32_t r : roots) stack.push_back(r);

            while (!stack.empty()) {
                uint32_t ni = stack.back(); stack.pop_back();
                const auto& node = mesh.dag_nodes[ni];

                // Transform bounding sphere to world space
                glm::vec3 world_center = glm::vec3(inst.model * glm::vec4(node.bound_center, 1.0f));
                float world_radius = node.bound_radius *
                    glm::length(glm::vec3(inst.model[0]));  // Approximate uniform scale

                // Frustum test
                bool visible = true;
                for (int p = 0; p < 6; ++p) {
                    float d = glm::dot(glm::vec3(frustum_planes[p]), world_center)
                              + frustum_planes[p].w;
                    if (d < -world_radius) { visible = false; break; }
                }
                if (!visible) continue;

                // Leaf node: always select
                if (node.first_child == UINT32_MAX || node.child_count == 0) {
                    uint32_t ci = node.cluster_index;
                    result.selected_clusters.push_back(ci);
                    result.total_triangles += mesh.clusters[ci].triangle_count;

                    // Determine rasterization mode
                    float screen_size = ComputeScreenSpaceSize(
                        world_center, world_radius, view_proj, screen_height);
                    float avg_tri_pixels = (screen_size * screen_size * 3.14159f) /
                        std::max(1u, mesh.clusters[ci].triangle_count);

                    if (config.enable_software_rasterizer &&
                        avg_tri_pixels < config.software_raster_threshold) {
                        result.sw_raster_clusters.push_back(ci);
                    } else {
                        result.hw_raster_clusters.push_back(ci);
                    }
                    continue;
                }

                // Interior node: check if screen-space error is acceptable
                DAGNode world_node = node;
                world_node.bound_center = world_center;
                world_node.bound_radius = world_radius;

                float screen_error = ComputeScreenSpaceError(
                    world_node, view_proj, camera_pos, screen_height, fov_y);

                if (screen_error <= config.lod_error_threshold) {
                    // Error small enough — use this coarser level
                    uint32_t ci = node.cluster_index;
                    result.selected_clusters.push_back(ci);
                    result.total_triangles += mesh.clusters[ci].triangle_count;

                    float screen_size = ComputeScreenSpaceSize(
                        world_center, world_radius, view_proj, screen_height);
                    float avg_tri_pixels = (screen_size * screen_size * 3.14159f) /
                        std::max(1u, mesh.clusters[ci].triangle_count);

                    if (config.enable_software_rasterizer &&
                        avg_tri_pixels < config.software_raster_threshold) {
                        result.sw_raster_clusters.push_back(ci);
                    } else {
                        result.hw_raster_clusters.push_back(ci);
                    }
                } else {
                    // Error too large — need finer detail, recurse into children
                    for (uint32_t c = 0; c < node.child_count; ++c) {
                        uint32_t child_idx = node.first_child + c;
                        if (child_idx < mesh.dag_nodes.size())
                            stack.push_back(child_idx);
                    }
                }
            }
        }
    }

    result_ = result;
    return result;
}

void GPUClusterCulling::PrepareGPUData(const glm::mat4& view_proj,
                                        const glm::vec3& camera_pos,
                                        float screen_width, float screen_height,
                                        float fov_y) {
    dag_nodes_gpu_.clear();
    clusters_gpu_.clear();
    draw_commands_.clear();
    total_cluster_count_ = 0;

    uint32_t global_cluster_offset = 0;
    uint32_t global_dag_offset = 0;

    for (uint32_t mi = 0; mi < frame_meshes_.size(); ++mi) {
        const auto& rm = frame_meshes_[mi];
        const auto& mesh = *rm.mesh;

        for (uint32_t ii = 0; ii < rm.instances.size(); ++ii) {
            const auto& inst = rm.instances[ii];

            // Upload DAG nodes (transformed to world space)
            for (const auto& node : mesh.dag_nodes) {
                DAGNodeGPU gpu{};
                glm::vec3 wc = glm::vec3(inst.model * glm::vec4(node.bound_center, 1.0f));
                float ws = node.bound_radius * glm::length(glm::vec3(inst.model[0]));
                gpu.sphere = glm::vec4(wc, ws);
                gpu.lod_error = node.lod_error;
                gpu.parent = (node.parent != UINT32_MAX) ? node.parent + global_dag_offset : UINT32_MAX;
                gpu.first_child = (node.first_child != UINT32_MAX) ? node.first_child + global_dag_offset : UINT32_MAX;
                gpu.child_count = node.child_count;
                dag_nodes_gpu_.push_back(gpu);
            }

            // Upload cluster GPU data
            for (uint32_t ci = 0; ci < mesh.clusters.size(); ++ci) {
                const auto& cluster = mesh.clusters[ci];
                ClusterGPU gpu{};
                glm::vec3 wc = glm::vec3(inst.model * glm::vec4(cluster.center, 1.0f));
                float ws = cluster.radius * glm::length(glm::vec3(inst.model[0]));
                gpu.sphere = glm::vec4(wc, ws);

                glm::vec3 world_cone = glm::normalize(
                    glm::mat3(inst.model) * cluster.cone_axis);
                gpu.cone = glm::vec4(world_cone, cluster.cone_cutoff);

                gpu.vertex_offset = cluster.vertex_offset;
                gpu.vertex_count = cluster.vertex_count;
                gpu.index_offset = (ci < mesh.draw_ranges.size()) ? mesh.draw_ranges[ci].index_offset : 0;
                gpu.index_count = cluster.triangle_count * 3;
                gpu.material_id = inst.material_id;
                gpu.instance_id = ii;
                gpu.pad[0] = gpu.pad[1] = 0;
                clusters_gpu_.push_back(gpu);

                // Build draw command
                MeshletDrawCommand cmd{};
                cmd.count = cluster.triangle_count * 3;
                cmd.instance_count = 1;  // Will be set to 0 by cull shader if culled
                cmd.first_index = gpu.index_offset;
                cmd.base_vertex = 0;
                cmd.base_instance = 0;
                draw_commands_.push_back(cmd);
            }

            global_dag_offset += static_cast<uint32_t>(mesh.dag_nodes.size());
            global_cluster_offset += static_cast<uint32_t>(mesh.clusters.size());
        }
    }

    total_cluster_count_ = static_cast<uint32_t>(clusters_gpu_.size());

    // Fill uniforms
    uniforms_.view_projection = view_proj;
    uniforms_.camera_pos = glm::vec4(camera_pos, 0.0f);
    uniforms_.screen_params = glm::vec4(screen_width, screen_height, fov_y, 0.0f);
    ExtractFrustumPlanes(view_proj, uniforms_.frustum_planes);
    uniforms_.dag_node_count = static_cast<uint32_t>(dag_nodes_gpu_.size());
    uniforms_.flags = 0x0F;  // All flags enabled
}

}  // namespace vg
}  // namespace render
}  // namespace dse

#endif  // DSE_ENABLE_VIRTUAL_GEOMETRY
