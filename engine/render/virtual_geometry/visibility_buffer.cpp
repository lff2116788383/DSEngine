/**
 * @file visibility_buffer.cpp
 * @brief Visibility Buffer implementation
 */

#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY

#include "engine/render/virtual_geometry/visibility_buffer.h"
#include "engine/render/virtual_geometry/software_rasterizer.h"
#include <algorithm>
#include <cstring>

namespace dse {
namespace render {
namespace vg {

void VisibilityBuffer::Init(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    buffer_.resize(width * height);
    Clear();
}

void VisibilityBuffer::Shutdown() {
    buffer_.clear();
    vertex_data_.clear();
    materials_.clear();
    cluster_info_.clear();
    width_ = height_ = 0;
}

void VisibilityBuffer::Resize(uint32_t width, uint32_t height) {
    width_ = width;
    height_ = height;
    buffer_.resize(width * height);
    Clear();
}

void VisibilityBuffer::Clear() {
    VisBufferEntry clear{};
    clear.depth_bits = 0xFFFFFFFF;
    clear.payload = 0xFFFFFFFF;
    std::fill(buffer_.begin(), buffer_.end(), clear);
    vertex_data_.clear();
    cluster_info_.clear();
}

void VisibilityBuffer::MergeSoftwareResults(const std::vector<VisBufferEntry>& sw_buffer,
                                             uint32_t sw_width, uint32_t sw_height) {
    uint32_t w = std::min(sw_width, width_);
    uint32_t h = std::min(sw_height, height_);

    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            uint32_t src_idx = y * sw_width + x;
            uint32_t dst_idx = y * width_ + x;

            if (sw_buffer[src_idx].depth_bits < buffer_[dst_idx].depth_bits) {
                buffer_[dst_idx] = sw_buffer[src_idx];
            }
        }
    }
}

void VisibilityBuffer::PrepareVertexData(const VirtualGeometryMesh& mesh,
                                          const glm::mat4& model) {
    glm::mat3 normal_mat = glm::transpose(glm::inverse(glm::mat3(model)));

    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        VGVertexData vd{};
        glm::vec3 world_pos = glm::vec3(model * glm::vec4(mesh.positions[i], 1.0f));
        vd.position = world_pos;
        vd.pad0 = 0.0f;

        if (i < mesh.normals.size()) {
            vd.normal = glm::normalize(normal_mat * mesh.normals[i]);
        } else {
            vd.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        vd.pad1 = 0.0f;

        if (i < mesh.uvs.size()) {
            vd.uv = mesh.uvs[i];
        }
        vd.pad2 = glm::vec2(0.0f);

        vertex_data_.push_back(vd);
    }
}

uint32_t VisibilityBuffer::AddMaterial(const VGMaterialEntry& mat) {
    uint32_t id = static_cast<uint32_t>(materials_.size());
    materials_.push_back(mat);
    return id;
}

VisBufferResolveUniforms VisibilityBuffer::GetResolveUniforms(
        const glm::mat4& inv_view_proj,
        const glm::vec3& camera_pos) const {
    VisBufferResolveUniforms u{};
    u.inv_view_proj = inv_view_proj;
    u.camera_pos = glm::vec4(camera_pos, 0.0f);
    u.screen_params = glm::vec4(
        static_cast<float>(width_),
        static_cast<float>(height_),
        1.0f / static_cast<float>(width_),
        1.0f / static_cast<float>(height_));
    u.cluster_count = static_cast<uint32_t>(cluster_info_.size());
    u.material_count = static_cast<uint32_t>(materials_.size());
    u.pad[0] = u.pad[1] = 0;
    return u;
}

std::vector<VisibilityBuffer::ResolvedPixel> VisibilityBuffer::ResolveCPU(
        const glm::mat4& inv_view_proj,
        const glm::vec3& camera_pos) {
    std::vector<ResolvedPixel> resolved;
    resolved.reserve(width_ * height_ / 4);

    for (uint32_t y = 0; y < height_; ++y) {
        for (uint32_t x = 0; x < width_; ++x) {
            uint32_t idx = y * width_ + x;
            const auto& entry = buffer_[idx];
            if (entry.payload == 0xFFFFFFFF) continue;

            ResolvedPixel pixel{};
            uint32_t cluster_id  = (entry.payload >> 16) & 0xFFFF;
            uint32_t triangle_id = (entry.payload >> 8) & 0xFF;
            pixel.material_id    = entry.payload & 0xFF;

            // Reconstruct depth from sortable uint
            uint32_t bits = entry.depth_bits;
            if (bits & 0x80000000u) {
                bits ^= 0x80000000u;
            } else {
                bits = ~bits;
            }
            std::memcpy(&pixel.depth, &bits, sizeof(float));

            // Reconstruct world position from screen coords + depth
            float ndc_x = (static_cast<float>(x) + 0.5f) / width_ * 2.0f - 1.0f;
            float ndc_y = (static_cast<float>(y) + 0.5f) / height_ * 2.0f - 1.0f;
            float ndc_z = pixel.depth * 2.0f - 1.0f;
            glm::vec4 clip(ndc_x, ndc_y, ndc_z, 1.0f);
            glm::vec4 world = inv_view_proj * clip;
            if (world.w != 0.0f) world /= world.w;
            pixel.position = glm::vec3(world);

            pixel.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            pixel.uv = glm::vec2(0.0f);

            resolved.push_back(pixel);
        }
    }
    return resolved;
}

}  // namespace vg
}  // namespace render
}  // namespace dse

#endif  // DSE_ENABLE_VIRTUAL_GEOMETRY
