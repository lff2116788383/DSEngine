/**
 * @file gpu_cluster_culling.h
 * @brief GPU-driven per-cluster culling with DAG LOD selection
 *
 * Two-pass compute:
 *   Pass 1 — LOD selection: walk DAG top-down, pick clusters whose screen-space
 *            error is below threshold; output selected cluster list.
 *   Pass 2 — Cluster culling: frustum + cone + Hi-Z on selected clusters;
 *            output indirect draw commands.
 */

#ifndef DSE_GPU_CLUSTER_CULLING_H
#define DSE_GPU_CLUSTER_CULLING_H

#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY

#include "engine/render/virtual_geometry/virtual_geometry_types.h"
#include "engine/render/virtual_geometry/virtual_geometry_config.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace dse {
namespace render {
namespace vg {

struct LODSelectionResult {
    std::vector<uint32_t> selected_clusters;
    std::vector<uint32_t> hw_raster_clusters;
    std::vector<uint32_t> sw_raster_clusters;
    uint32_t total_triangles = 0;
};

struct GPUClusterCullUniforms {
    glm::mat4 view_projection;
    glm::vec4 camera_pos;
    glm::vec4 screen_params;       // x=width, y=height, z=fov_y, w=unused
    glm::vec4 frustum_planes[6];
    float     lod_error_threshold;
    float     sw_raster_threshold; // screen-space pixel threshold
    uint32_t  dag_node_count;
    uint32_t  flags;               // bit0=frustum, bit1=occlusion, bit2=cone, bit3=lod_select
};

class GPUClusterCulling {
public:
    GPUClusterCulling() = default;
    ~GPUClusterCulling() = default;

    void Init(const VirtualGeometryConfig& config);
    void Shutdown();

    /// Register a VG mesh and its instances for this frame
    void BeginFrame();
    void AddInstances(uint32_t mesh_id, const VirtualGeometryMesh& mesh,
                      const std::vector<VGInstance>& instances);

    /// Run LOD selection on CPU (fallback when compute not available)
    LODSelectionResult SelectLOD_CPU(const glm::mat4& view, const glm::mat4& proj,
                                     const glm::vec3& camera_pos,
                                     float screen_height,
                                     const VirtualGeometryConfig& config);

    /// Prepare GPU data for compute dispatch
    void PrepareGPUData(const glm::mat4& view_proj, const glm::vec3& camera_pos,
                        float screen_width, float screen_height, float fov_y);

    /// Get the uniforms for the compute shader
    const GPUClusterCullUniforms& GetUniforms() const { return uniforms_; }

    /// Get selected clusters after LOD selection
    const LODSelectionResult& GetResult() const { return result_; }

    /// Get all DAG nodes (for GPU upload)
    const std::vector<DAGNodeGPU>& GetDAGNodesGPU() const { return dag_nodes_gpu_; }

    /// Get all cluster GPU data
    const std::vector<ClusterGPU>& GetClustersGPU() const { return clusters_gpu_; }

    /// Get indirect draw commands
    const std::vector<MeshletDrawCommand>& GetDrawCommands() const { return draw_commands_; }

    uint32_t GetTotalClusterCount() const { return total_cluster_count_; }

private:
    float ComputeScreenSpaceError(const DAGNode& node, const glm::mat4& view_proj,
                                   const glm::vec3& camera_pos, float screen_height,
                                   float fov_y);
    float ComputeScreenSpaceSize(const glm::vec3& center, float radius,
                                  const glm::mat4& view_proj, float screen_height);
    void ExtractFrustumPlanes(const glm::mat4& view_proj, glm::vec4 planes[6]);

    struct RegisteredMesh {
        const VirtualGeometryMesh* mesh;
        std::vector<VGInstance> instances;
    };

    std::vector<RegisteredMesh> frame_meshes_;
    std::vector<DAGNodeGPU> dag_nodes_gpu_;
    std::vector<ClusterGPU> clusters_gpu_;
    std::vector<MeshletDrawCommand> draw_commands_;
    GPUClusterCullUniforms uniforms_{};
    LODSelectionResult result_;
    uint32_t total_cluster_count_ = 0;
    bool initialized_ = false;
};

}  // namespace vg
}  // namespace render
}  // namespace dse

#endif  // DSE_ENABLE_VIRTUAL_GEOMETRY
#endif  // DSE_GPU_CLUSTER_CULLING_H
