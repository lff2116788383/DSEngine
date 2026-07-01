/**
 * @file software_rasterizer.h
 * @brief Compute-shader-based software rasterizer for small triangles
 *
 * Triangles whose screen-space area is below the configurable threshold
 * (default 32 pixels) are rasterized by a compute shader instead of the
 * fixed-function hardware rasterizer.  Each thread handles one triangle,
 * computing edge equations and iterating over the bounding rect to write
 * into the Visibility Buffer via atomic64 (depth + triangle ID packed).
 *
 * On APIs without 64-bit atomics the system falls back to 32-bit depth
 * with reduced precision (adequate for small triangles close together).
 */

#ifndef DSE_SOFTWARE_RASTERIZER_H
#define DSE_SOFTWARE_RASTERIZER_H

#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY

#include "engine/render/virtual_geometry/virtual_geometry_types.h"
#include "engine/render/virtual_geometry/virtual_geometry_config.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace dse {
namespace render {
namespace vg {

/// Per-triangle data uploaded to the software rasterizer's input SSBO
struct SWRasterTriangle {
    glm::vec4 v0;       ///< xyz = NDC position, w = 1/w (for perspective correction)
    glm::vec4 v1;
    glm::vec4 v2;
    uint32_t  cluster_id;
    uint32_t  triangle_id;  ///< Local triangle index within cluster
    uint32_t  material_id;
    uint32_t  instance_id;
};
static_assert(sizeof(SWRasterTriangle) == 64, "SWRasterTriangle must be 64 bytes");

// VisBufferEntry is defined in virtual_geometry_types.h

struct SoftwareRasterizerStats {
    uint32_t total_triangles = 0;
    uint32_t rasterized_triangles = 0;
    uint32_t pixels_written = 0;
};

class SoftwareRasterizer {
public:
    SoftwareRasterizer() = default;
    ~SoftwareRasterizer() = default;

    void Init(uint32_t width, uint32_t height);
    void Shutdown();
    void Resize(uint32_t width, uint32_t height);

    /// Prepare triangles for software rasterization
    /// Transforms cluster triangles to NDC and filters by screen-space size
    void PrepareTriangles(const std::vector<uint32_t>& sw_cluster_indices,
                          const VirtualGeometryMesh& mesh,
                          const glm::mat4& model,
                          const glm::mat4& view_proj,
                          uint32_t material_id, uint32_t instance_id);

    /// Execute CPU fallback rasterization (writes to internal vis buffer)
    void RasterizeCPU();

    /// Get triangle data for GPU upload
    const std::vector<SWRasterTriangle>& GetTriangles() const { return triangles_; }
    uint32_t GetTriangleCount() const { return static_cast<uint32_t>(triangles_.size()); }

    /// Access the CPU-side visibility buffer (width * height entries)
    const std::vector<VisBufferEntry>& GetVisBuffer() const { return vis_buffer_; }

    /// Clear the visibility buffer for a new frame
    void ClearVisBuffer();

    const SoftwareRasterizerStats& GetStats() const { return stats_; }

    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }

private:
    void RasterizeTriangle(const SWRasterTriangle& tri);
    static uint32_t FloatToSortableUint(float f);

    uint32_t width_ = 0;
    uint32_t height_ = 0;
    std::vector<SWRasterTriangle> triangles_;
    std::vector<VisBufferEntry> vis_buffer_;
    SoftwareRasterizerStats stats_;
};

}  // namespace vg
}  // namespace render
}  // namespace dse

#endif  // DSE_ENABLE_VIRTUAL_GEOMETRY
#endif  // DSE_SOFTWARE_RASTERIZER_H
