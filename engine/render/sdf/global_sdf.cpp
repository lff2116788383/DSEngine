/**
 * @file global_sdf.cpp
 * @brief Global SDF 实现：级联体素化 + 增量更新
 */

#include "engine/render/sdf/global_sdf.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace dse {
namespace render {

void GlobalSDFSystem::Init(const GlobalSDFConfig& config) {
    config_ = config;
    cascades_.resize(config.num_cascades);

    for (int i = 0; i < config.num_cascades; ++i) {
        auto& c = cascades_[i];
        c.resolution = config.base_resolution;
        c.voxel_size = config.base_voxel_size * std::pow(config.cascade_ratio, static_cast<float>(i));
        c.extent = c.voxel_size * static_cast<float>(c.resolution) * 0.5f;
        c.center = glm::vec3(0.0f);
        c.distance_data.resize(c.resolution * c.resolution * c.resolution,
                               std::numeric_limits<float>::max());
        c.gpu_dirty = true;
    }

    initialized_ = true;
}

void GlobalSDFSystem::Update(const glm::vec3& viewer_pos) {
    if (!initialized_) return;

    for (auto& cascade : cascades_) {
        // Snap center to voxel grid
        float snap = cascade.voxel_size * 4.0f; // snap 粒度 = 4 voxels
        glm::vec3 snapped;
        snapped.x = std::floor(viewer_pos.x / snap) * snap;
        snapped.y = std::floor(viewer_pos.y / snap) * snap;
        snapped.z = std::floor(viewer_pos.z / snap) * snap;

        if (snapped != cascade.center) {
            cascade.center = snapped;
            cascade.gpu_dirty = true;
            // 增量更新移动方向切片（简化：标记脏即可，渲染时 GPU 重建）
        }
    }
}

void GlobalSDFSystem::SubmitStaticMesh(const SDFMeshInput& mesh) {
    static_meshes_.push_back(mesh);
}

void GlobalSDFSystem::RebuildAll() {
    if (!initialized_) return;

    for (auto& cascade : cascades_) {
        int res = cascade.resolution;
        float voxel = cascade.voxel_size;
        glm::vec3 origin = cascade.center - glm::vec3(cascade.extent);

        for (int z = 0; z < res; ++z) {
            for (int y = 0; y < res; ++y) {
                for (int x = 0; x < res; ++x) {
                    glm::vec3 voxel_center = origin + glm::vec3(
                        (x + 0.5f) * voxel,
                        (y + 0.5f) * voxel,
                        (z + 0.5f) * voxel
                    );
                    cascade.distance_data[z * res * res + y * res + x] =
                        ComputeDistanceAtVoxel(voxel_center);
                }
            }
        }
        cascade.gpu_dirty = true;
    }
}

float GlobalSDFSystem::QueryDistance(const glm::vec3& world_pos) const {
    if (!initialized_ || cascades_.empty()) return std::numeric_limits<float>::max();

    // 从最精细级联开始查找
    for (const auto& cascade : cascades_) {
        glm::vec3 local = world_pos - (cascade.center - glm::vec3(cascade.extent));
        float voxel = cascade.voxel_size;
        int res = cascade.resolution;

        int ix = static_cast<int>(local.x / voxel);
        int iy = static_cast<int>(local.y / voxel);
        int iz = static_cast<int>(local.z / voxel);

        // 检查是否在此级联范围内（留 1 voxel 边界用于插值）
        if (ix >= 1 && ix < res - 1 && iy >= 1 && iy < res - 1 && iz >= 1 && iz < res - 1) {
            return cascade.distance_data[iz * res * res + iy * res + ix];
        }
    }

    return std::numeric_limits<float>::max();
}

float GlobalSDFSystem::ComputeDistanceAtVoxel(const glm::vec3& voxel_center) const {
    float min_dist = std::numeric_limits<float>::max();

    for (const auto& mesh : static_meshes_) {
        if (!mesh.positions || !mesh.indices) continue;

        for (uint32_t i = 0; i + 2 < mesh.index_count; i += 3) {
            glm::vec3 a = glm::vec3(mesh.transform * glm::vec4(mesh.positions[mesh.indices[i]], 1.0f));
            glm::vec3 b = glm::vec3(mesh.transform * glm::vec4(mesh.positions[mesh.indices[i + 1]], 1.0f));
            glm::vec3 c = glm::vec3(mesh.transform * glm::vec4(mesh.positions[mesh.indices[i + 2]], 1.0f));

            float d = PointTriangleDistance(voxel_center, a, b, c);
            min_dist = std::min(min_dist, d);
        }
    }

    return min_dist;
}

float GlobalSDFSystem::PointTriangleDistance(const glm::vec3& p,
                                             const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
    glm::vec3 ab = b - a;
    glm::vec3 ac = c - a;
    glm::vec3 ap = p - a;

    float d1 = glm::dot(ab, ap);
    float d2 = glm::dot(ac, ap);
    if (d1 <= 0.0f && d2 <= 0.0f) return glm::length(p - a);

    glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp);
    float d4 = glm::dot(ac, bp);
    if (d3 >= 0.0f && d4 <= d3) return glm::length(p - b);

    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
        float v = d1 / (d1 - d3);
        return glm::length(p - (a + ab * v));
    }

    glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp);
    float d6 = glm::dot(ac, cp);
    if (d6 >= 0.0f && d5 <= d6) return glm::length(p - c);

    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
        float w = d2 / (d2 - d6);
        return glm::length(p - (a + ac * w));
    }

    float va = d3 * d6 - d5 * d4;
    if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
        float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return glm::length(p - (b + (c - b) * w));
    }

    float denom = 1.0f / (va + vb + vc);
    float v = vb * denom;
    float w = vc * denom;
    glm::vec3 closest = a + ab * v + ac * w;
    return glm::length(p - closest);
}

std::vector<int> GlobalSDFSystem::GetDirtyCascades() const {
    std::vector<int> dirty;
    for (int i = 0; i < static_cast<int>(cascades_.size()); ++i) {
        if (cascades_[i].gpu_dirty) dirty.push_back(i);
    }
    return dirty;
}

void GlobalSDFSystem::ClearDirty(int cascade) {
    if (cascade >= 0 && cascade < static_cast<int>(cascades_.size())) {
        cascades_[cascade].gpu_dirty = false;
    }
}

void GlobalSDFSystem::Shutdown() {
    cascades_.clear();
    static_meshes_.clear();
    initialized_ = false;
}

} // namespace render
} // namespace dse
