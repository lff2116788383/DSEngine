/**
 * @file hair_asset.cpp
 * @brief TressFX 风格毛发资产加载 / 程序化生成
 */

#include "engine/render/hair/hair_asset.h"
#include "engine/base/debug.h"

#include <cmath>
#include <fstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace dse {
namespace render {

// ============================================================================
// LoadHairAsset — .dhair 简单二进制格式
// ============================================================================

bool LoadHairAsset(const std::string& file_path, HairAsset& out_asset) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        DEBUG_LOG_ERROR("[HairAsset] Failed to open: {}", file_path);
        return false;
    }

    // .dhair 格式:
    // [4 bytes] magic 'DHFX'
    // [4 bytes] version (1)
    // [4 bytes] num_guide_strands
    // [4 bytes] vertices_per_strand
    // [4 bytes] num_follow_per_guide
    // [4 bytes] follow_root_offset_range (float)
    // [N * vertices_per_strand * sizeof(HairStrandVertex)] vertex data

    char magic[4];
    file.read(magic, 4);
    if (magic[0] != 'D' || magic[1] != 'H' || magic[2] != 'F' || magic[3] != 'X') {
        DEBUG_LOG_ERROR("[HairAsset] Invalid magic in: {}", file_path);
        return false;
    }

    uint32_t version = 0;
    file.read(reinterpret_cast<char*>(&version), 4);
    if (version != 1) {
        DEBUG_LOG_ERROR("[HairAsset] Unsupported version {} in: {}", version, file_path);
        return false;
    }

    uint32_t num_strands = 0, vps = 0, npg = 0;
    float offset_range = 0.0f;
    file.read(reinterpret_cast<char*>(&num_strands), 4);
    file.read(reinterpret_cast<char*>(&vps), 4);
    file.read(reinterpret_cast<char*>(&npg), 4);
    file.read(reinterpret_cast<char*>(&offset_range), 4);

    out_asset.name = file_path;
    out_asset.vertices_per_strand = vps;
    out_asset.num_follow_per_guide = npg;
    out_asset.follow_root_offset_range = offset_range;

    uint32_t total_verts = num_strands * vps;
    out_asset.vertices.resize(total_verts);
    file.read(reinterpret_cast<char*>(out_asset.vertices.data()),
              total_verts * sizeof(HairStrandVertex));

    out_asset.strands.resize(num_strands);
    for (uint32_t i = 0; i < num_strands; ++i) {
        out_asset.strands[i].vertex_offset = i * vps;
        out_asset.strands[i].vertex_count = vps;
    }

    if (!out_asset.IsValid()) {
        DEBUG_LOG_ERROR("[HairAsset] Loaded data is invalid: {}", file_path);
        return false;
    }

    DEBUG_LOG_INFO("[HairAsset] Loaded '{}': {} strands, {} verts/strand",
                   file_path, num_strands, vps);
    return true;
}

// ============================================================================
// GenerateTestHairAsset — 球面均匀分布的程序化发丝
// ============================================================================

void GenerateTestHairAsset(uint32_t num_guide_strands,
                            uint32_t verts_per_strand,
                            float hair_length,
                            float sphere_radius,
                            HairAsset& out_asset) {
    out_asset = {};
    out_asset.name = "procedural_sphere_hair";
    out_asset.vertices_per_strand = verts_per_strand;

    const float seg_length = hair_length / static_cast<float>(verts_per_strand - 1);
    const float golden_ratio = (1.0f + std::sqrt(5.0f)) / 2.0f;

    out_asset.strands.resize(num_guide_strands);
    out_asset.vertices.resize(num_guide_strands * verts_per_strand);

    for (uint32_t s = 0; s < num_guide_strands; ++s) {
        // Fibonacci sphere sampling
        float theta = 2.0f * static_cast<float>(M_PI) * static_cast<float>(s) / golden_ratio;
        float phi = std::acos(1.0f - 2.0f * (static_cast<float>(s) + 0.5f) / static_cast<float>(num_guide_strands));

        glm::vec3 normal(
            std::sin(phi) * std::cos(theta),
            std::cos(phi),
            std::sin(phi) * std::sin(theta)
        );
        glm::vec3 root_pos = normal * sphere_radius;

        out_asset.strands[s].vertex_offset = s * verts_per_strand;
        out_asset.strands[s].vertex_count = verts_per_strand;

        for (uint32_t v = 0; v < verts_per_strand; ++v) {
            uint32_t idx = s * verts_per_strand + v;
            float t = static_cast<float>(v) / static_cast<float>(verts_per_strand - 1);

            glm::vec3 pos = root_pos + normal * (seg_length * static_cast<float>(v));

            // 轻微弯曲 — 重力影响
            pos.y -= 0.5f * t * t * hair_length;

            auto& vert = out_asset.vertices[idx];
            vert.position = glm::vec4(pos, v == 0 ? 0.0f : seg_length);
            vert.tangent = glm::vec4(normal, 1.0f - t);  // thickness 从 1→0
        }

        // 重新计算切线
        for (uint32_t v = 0; v < verts_per_strand; ++v) {
            uint32_t idx = s * verts_per_strand + v;
            glm::vec3 tangent;
            if (v == 0) {
                glm::vec3 p0(out_asset.vertices[idx].position);
                glm::vec3 p1(out_asset.vertices[idx + 1].position);
                tangent = glm::normalize(p1 - p0);
            } else if (v == verts_per_strand - 1) {
                glm::vec3 p0(out_asset.vertices[idx - 1].position);
                glm::vec3 p1(out_asset.vertices[idx].position);
                tangent = glm::normalize(p1 - p0);
            } else {
                glm::vec3 p0(out_asset.vertices[idx - 1].position);
                glm::vec3 p2(out_asset.vertices[idx + 1].position);
                tangent = glm::normalize(p2 - p0);
            }
            float thickness = out_asset.vertices[idx].tangent.w;
            out_asset.vertices[idx].tangent = glm::vec4(tangent, thickness);
        }
    }

    DEBUG_LOG_INFO("[HairAsset] Generated procedural hair: {} strands × {} verts, length={:.1f}",
                   num_guide_strands, verts_per_strand, hair_length);
}

} // namespace render
} // namespace dse
