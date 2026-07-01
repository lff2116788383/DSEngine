/**
 * @file software_rasterizer.cpp
 * @brief CPU fallback software rasterizer implementation
 *
 * For each small triangle, computes edge equations in screen space and
 * iterates over its bounding rectangle, testing each pixel against the
 * three half-plane equations.  Visible pixels are written to the
 * visibility buffer with an atomic-style depth test (on CPU: simple compare).
 *
 * The GPU compute shader version mirrors this logic in GLSL 450.
 */

#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY

#include "engine/render/virtual_geometry/software_rasterizer.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>

namespace dse {
namespace render {
namespace vg {

void SoftwareRasterizer::Init(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    vis_buffer_.resize(width * height);
    ClearVisBuffer();
}

void SoftwareRasterizer::Shutdown() {
    triangles_.clear();
    vis_buffer_.clear();
    width_ = height_ = 0;
}

void SoftwareRasterizer::Resize(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    vis_buffer_.resize(width * height);
    ClearVisBuffer();
}

void SoftwareRasterizer::ClearVisBuffer() {
    VisBufferEntry clear{};
    clear.depth_bits = 0xFFFFFFFF;  // Far plane
    clear.payload = 0xFFFFFFFF;     // Invalid
    std::fill(vis_buffer_.begin(), vis_buffer_.end(), clear);
    stats_ = {};
}

uint32_t SoftwareRasterizer::FloatToSortableUint(float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, sizeof(bits));
    // IEEE 754 float comparison trick: flip sign bit, or invert all if negative
    if (bits & 0x80000000u) {
        bits = ~bits;
    } else {
        bits ^= 0x80000000u;
    }
    return bits;
}

void SoftwareRasterizer::PrepareTriangles(
        const std::vector<uint32_t>& sw_cluster_indices,
        const VirtualGeometryMesh& mesh,
        const glm::mat4& model,
        const glm::mat4& view_proj,
        uint32_t material_id, uint32_t instance_id) {

    glm::mat4 mvp = view_proj * model;

    for (uint32_t ci : sw_cluster_indices) {
        if (ci >= mesh.clusters.size()) continue;
        const auto& cluster = mesh.clusters[ci];

        for (uint32_t t = 0; t < cluster.triangle_count; ++t) {
            uint32_t base = cluster.triangle_offset + t * 3;

            SWRasterTriangle tri{};
            for (int k = 0; k < 3; ++k) {
                uint8_t local_vi = mesh.cluster_triangles[base + k];
                uint32_t global_vi = mesh.cluster_vertices[cluster.vertex_offset + local_vi];
                glm::vec4 clip = mvp * glm::vec4(mesh.positions[global_vi], 1.0f);

                glm::vec4* dst = (k == 0) ? &tri.v0 : (k == 1) ? &tri.v1 : &tri.v2;
                if (clip.w > 0.0f) {
                    dst->x = clip.x / clip.w;
                    dst->y = clip.y / clip.w;
                    dst->z = clip.z / clip.w;
                    dst->w = 1.0f / clip.w;
                } else {
                    dst->x = dst->y = dst->z = 0.0f;
                    dst->w = 0.0f;
                }
            }
            tri.cluster_id = ci;
            tri.triangle_id = t;
            tri.material_id = material_id;
            tri.instance_id = instance_id;
            triangles_.push_back(tri);
        }
    }
}

void SoftwareRasterizer::RasterizeCPU() {
    stats_.total_triangles = static_cast<uint32_t>(triangles_.size());
    stats_.rasterized_triangles = 0;
    stats_.pixels_written = 0;

    for (const auto& tri : triangles_) {
        RasterizeTriangle(tri);
    }
}

void SoftwareRasterizer::RasterizeTriangle(const SWRasterTriangle& tri) {
    // Convert NDC [-1,1] to screen space [0, width/height]
    float hw = static_cast<float>(width_) * 0.5f;
    float hh = static_cast<float>(height_) * 0.5f;

    float x0 = (tri.v0.x * 0.5f + 0.5f) * width_;
    float y0 = (tri.v0.y * 0.5f + 0.5f) * height_;
    float z0 = tri.v0.z * 0.5f + 0.5f;

    float x1 = (tri.v1.x * 0.5f + 0.5f) * width_;
    float y1 = (tri.v1.y * 0.5f + 0.5f) * height_;
    float z1 = tri.v1.z * 0.5f + 0.5f;

    float x2 = (tri.v2.x * 0.5f + 0.5f) * width_;
    float y2 = (tri.v2.y * 0.5f + 0.5f) * height_;
    float z2 = tri.v2.z * 0.5f + 0.5f;

    // Bounding rect (clamped to screen)
    int min_x = std::max(0, static_cast<int>(std::floor(std::min({x0, x1, x2}))));
    int max_x = std::min(static_cast<int>(width_) - 1,
                         static_cast<int>(std::ceil(std::max({x0, x1, x2}))));
    int min_y = std::max(0, static_cast<int>(std::floor(std::min({y0, y1, y2}))));
    int max_y = std::min(static_cast<int>(height_) - 1,
                         static_cast<int>(std::ceil(std::max({y0, y1, y2}))));

    if (min_x > max_x || min_y > max_y) return;

    // Edge equations
    float area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0);
    if (std::abs(area) < 1e-6f) return;  // Degenerate
    float inv_area = 1.0f / area;

    // Winding order: positive area = CCW
    bool ccw = area > 0.0f;

    ++stats_.rasterized_triangles;

    uint32_t payload = ((tri.cluster_id & 0xFFFFu) << 16) |
                       ((tri.triangle_id & 0xFFu) << 8) |
                       (tri.material_id & 0xFFu);

    for (int py = min_y; py <= max_y; ++py) {
        for (int px = min_x; px <= max_x; ++px) {
            float cx = static_cast<float>(px) + 0.5f;
            float cy = static_cast<float>(py) + 0.5f;

            // Barycentric coordinates
            float w0 = ((x1 - cx) * (y2 - cy) - (x2 - cx) * (y1 - cy)) * inv_area;
            float w1 = ((x2 - cx) * (y0 - cy) - (x0 - cx) * (y2 - cy)) * inv_area;
            float w2 = 1.0f - w0 - w1;

            // Inside test (handle both winding orders)
            if (ccw) {
                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) continue;
            } else {
                if (w0 > 0.0f || w1 > 0.0f || w2 > 0.0f) continue;
                w0 = -w0; w1 = -w1; w2 = -w2;
            }

            float z = w0 * z0 + w1 * z1 + w2 * z2;
            uint32_t z_bits = FloatToSortableUint(z);

            uint32_t idx = py * width_ + px;
            if (z_bits < vis_buffer_[idx].depth_bits) {
                vis_buffer_[idx].depth_bits = z_bits;
                vis_buffer_[idx].payload = payload;
                ++stats_.pixels_written;
            }
        }
    }
}

}  // namespace vg
}  // namespace render
}  // namespace dse

#endif  // DSE_ENABLE_VIRTUAL_GEOMETRY
