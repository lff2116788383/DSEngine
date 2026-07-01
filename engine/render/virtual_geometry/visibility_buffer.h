/**
 * @file visibility_buffer.h
 * @brief Visibility Buffer pass + material resolve for virtual geometry
 *
 * The VisBuffer stores per-pixel (depth, cluster_id, triangle_id, material_id).
 * After both HW and SW rasterization fill it, a full-screen resolve pass reads
 * each pixel, recovers the triangle's vertex attributes via barycentrics, and
 * evaluates the material shader to produce GBuffer output.
 *
 * Only meshes marked NaniteStatic go through this path; dynamic and NPR meshes
 * use the traditional Cluster Indirect (hardware-only) path.
 */

#ifndef DSE_VISIBILITY_BUFFER_H
#define DSE_VISIBILITY_BUFFER_H

#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY

#include "engine/render/virtual_geometry/virtual_geometry_types.h"
#include "engine/render/virtual_geometry/virtual_geometry_config.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace dse {
namespace render {
namespace vg {

/// GPU-side vertex data in the mega vertex buffer for attribute recovery
struct VGVertexData {
    glm::vec3 position;
    float     pad0;
    glm::vec3 normal;
    float     pad1;
    glm::vec2 uv;
    glm::vec2 pad2;
};
static_assert(sizeof(VGVertexData) == 48, "VGVertexData must be 48 bytes");

/// Material entry for the resolve shader's material table
struct VGMaterialEntry {
    glm::vec4 base_color;
    float     metallic;
    float     roughness;
    float     ao;
    uint32_t  albedo_tex;      ///< Bindless texture handle or array index
    uint32_t  normal_tex;
    uint32_t  orm_tex;         ///< ORM packed texture
    uint32_t  pad[2];
};
static_assert(sizeof(VGMaterialEntry) == 48, "VGMaterialEntry must be 48 bytes");

/// Resolve pass uniforms
struct VisBufferResolveUniforms {
    glm::mat4 inv_view_proj;
    glm::vec4 camera_pos;
    glm::vec4 screen_params;   ///< x=width, y=height, z=1/width, w=1/height
    uint32_t  cluster_count;
    uint32_t  material_count;
    uint32_t  pad[2];
};

class VisibilityBuffer {
public:
    VisibilityBuffer() = default;
    ~VisibilityBuffer() = default;

    void Init(uint32_t width, uint32_t height);
    void Shutdown();
    void Resize(uint32_t width, uint32_t height);

    /// Clear the visibility buffer for a new frame
    void Clear();

    /// Merge software rasterizer results into the visibility buffer
    void MergeSoftwareResults(const std::vector<VisBufferEntry>& sw_buffer,
                              uint32_t sw_width, uint32_t sw_height);

    /// Prepare vertex data for attribute recovery in the resolve pass
    void PrepareVertexData(const VirtualGeometryMesh& mesh, const glm::mat4& model);

    /// Add material entries
    uint32_t AddMaterial(const VGMaterialEntry& mat);

    /// Run CPU resolve (debug/fallback): for each pixel, recover attributes
    /// and write to output GBuffer-like arrays
    struct ResolvedPixel {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
        uint32_t  material_id;
        float     depth;
    };
    std::vector<ResolvedPixel> ResolveCPU(const glm::mat4& inv_view_proj,
                                           const glm::vec3& camera_pos);

    /// Get resolve uniforms for GPU
    VisBufferResolveUniforms GetResolveUniforms(const glm::mat4& inv_view_proj,
                                                 const glm::vec3& camera_pos) const;

    /// Access internal vis buffer
    const std::vector<VisBufferEntry>& GetBuffer() const { return buffer_; }
    const std::vector<VGVertexData>& GetVertexData() const { return vertex_data_; }
    const std::vector<VGMaterialEntry>& GetMaterials() const { return materials_; }

    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }

private:
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    std::vector<VisBufferEntry> buffer_;
    std::vector<VGVertexData> vertex_data_;
    std::vector<VGMaterialEntry> materials_;
    std::vector<ClusterGPU> cluster_info_;
};

}  // namespace vg
}  // namespace render
}  // namespace dse

#endif  // DSE_ENABLE_VIRTUAL_GEOMETRY
#endif  // DSE_VISIBILITY_BUFFER_H
