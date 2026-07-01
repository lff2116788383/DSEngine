/**
 * @file virtual_geometry_renderer.h
 * @brief Main virtual geometry renderer — dual pipeline dispatcher
 *
 * Owns all VG subsystems and orchestrates the per-frame pipeline:
 *
 *   1. Collect VGInstances from ECS (NaniteStaticFlag component)
 *   2. Run DAG LOD selection (GPU compute or CPU fallback)
 *   3. Split selected clusters into HW-raster and SW-raster buckets
 *   4. HW path:  emit MultiDrawElementsIndirect via Cluster Indirect
 *   5. SW path:  dispatch software rasterizer compute → Visibility Buffer
 *   6. Resolve:  full-screen pass reads VisBuffer, evaluates materials → GBuffer
 *
 * Non-NaniteStatic meshes bypass this renderer entirely and use the
 * traditional per-object or per-meshlet Cluster Indirect path.
 */

#ifndef DSE_VIRTUAL_GEOMETRY_RENDERER_H
#define DSE_VIRTUAL_GEOMETRY_RENDERER_H

#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY

#include "engine/render/virtual_geometry/virtual_geometry_config.h"
#include "engine/render/virtual_geometry/virtual_geometry_types.h"
#include "engine/render/virtual_geometry/gpu_cluster_culling.h"
#include "engine/render/virtual_geometry/software_rasterizer.h"
#include "engine/render/virtual_geometry/visibility_buffer.h"
#include "engine/render/virtual_geometry/cluster_streaming.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace dse {
namespace render {
namespace vg {

struct VGFrameStats {
    uint32_t total_instances = 0;
    uint32_t nanite_static_instances = 0;
    uint32_t traditional_instances = 0;
    uint32_t selected_clusters = 0;
    uint32_t hw_raster_clusters = 0;
    uint32_t sw_raster_clusters = 0;
    uint32_t total_triangles = 0;
    uint32_t sw_pixels_written = 0;
    uint32_t resident_clusters = 0;
    uint32_t streaming_loads = 0;
    uint32_t streaming_evictions = 0;
};

class VirtualGeometryRenderer {
public:
    VirtualGeometryRenderer() = default;
    ~VirtualGeometryRenderer() = default;

    void Init(const VirtualGeometryConfig& config, uint32_t screen_width, uint32_t screen_height);
    void Shutdown();
    void OnResize(uint32_t width, uint32_t height);

    /// Register a virtual geometry mesh asset.  Returns a mesh_id.
    uint32_t RegisterMesh(const std::string& name, const VirtualGeometryMesh& mesh);

    /// Remove a registered mesh
    void UnregisterMesh(uint32_t mesh_id);

    /// Per-frame interface
    void BeginFrame(uint64_t frame_number);

    /// Submit instances for rendering this frame
    void SubmitInstance(const VGInstance& instance);

    /// Execute the full VG pipeline.
    /// Populates hw_draw_commands for the caller to issue via the RHI.
    void Execute(const glm::mat4& view, const glm::mat4& proj,
                 const glm::vec3& camera_pos, float fov_y);

    /// Get hardware-raster draw commands (for MultiDrawElementsIndirect)
    const std::vector<MeshletDrawCommand>& GetHWDrawCommands() const { return hw_draw_commands_; }
    uint32_t GetHWDrawCommandCount() const { return static_cast<uint32_t>(hw_draw_commands_.size()); }

    /// Access sub-systems for GPU data upload
    const GPUClusterCulling& GetCulling() const { return culling_; }
    const SoftwareRasterizer& GetSWRasterizer() const { return sw_raster_; }
    const VisibilityBuffer& GetVisBuffer() const { return vis_buffer_; }
    const ClusterStreaming& GetStreaming() const { return streaming_; }

    /// Runtime config (mutable for editor UI)
    VirtualGeometryConfig& GetConfig() { return config_; }
    const VirtualGeometryConfig& GetConfig() const { return config_; }

    const VGFrameStats& GetFrameStats() const { return frame_stats_; }

    /// Check if a mesh_id is registered
    bool HasMesh(uint32_t mesh_id) const { return mesh_registry_.count(mesh_id) > 0; }

private:
    void CollectInstances();
    void RunLODSelection(const glm::mat4& view, const glm::mat4& proj,
                         const glm::vec3& camera_pos, float fov_y);
    void RunSoftwareRasterizer(const glm::mat4& view_proj);
    void BuildHWDrawCommands();
    void RunVisBufferResolve(const glm::mat4& view_proj, const glm::vec3& camera_pos);

    VirtualGeometryConfig config_;
    GPUClusterCulling culling_;
    SoftwareRasterizer sw_raster_;
    VisibilityBuffer vis_buffer_;
    ClusterStreaming streaming_;

    uint32_t screen_width_ = 0;
    uint32_t screen_height_ = 0;
    uint64_t frame_number_ = 0;

    struct MeshEntry {
        VirtualGeometryMesh mesh;
        std::string name;
    };
    uint32_t next_mesh_id_ = 1;
    std::unordered_map<uint32_t, MeshEntry> mesh_registry_;

    std::vector<VGInstance> frame_instances_;
    std::vector<VGInstance> nanite_instances_;
    std::vector<VGInstance> traditional_instances_;
    std::vector<MeshletDrawCommand> hw_draw_commands_;

    VGFrameStats frame_stats_;
    bool initialized_ = false;
};

}  // namespace vg
}  // namespace render
}  // namespace dse

#endif  // DSE_ENABLE_VIRTUAL_GEOMETRY
#endif  // DSE_VIRTUAL_GEOMETRY_RENDERER_H
