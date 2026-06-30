/**
 * @file geometry_clipmap.cpp
 * @brief Geometry Clipmap 连续 LOD 地形实现
 */

#include "engine/terrain/geometry_clipmap.h"
#include <cmath>
#include <algorithm>

namespace dse {
namespace terrain {

void GeometryClipmapSystem::Init(const GeometryClipmapConfig& config) {
    config_ = config;
    levels_.resize(config.num_levels);

    for (int i = 0; i < config.num_levels; ++i) {
        auto& level = levels_[i];
        level.grid_size = config.grid_size;
        level.cell_size = config.base_cell_size * static_cast<float>(1 << i);
        level.extent = level.cell_size * static_cast<float>(config.grid_size / 2);
        level.origin_x = 0;
        level.origin_z = 0;
        level.height_data.resize(config.grid_size * config.grid_size, 0.0f);
        level.gpu_dirty = true;
    }

    initialized_ = true;
}

void GeometryClipmapSystem::SetHeightSampler(HeightSampleFunc func, void* user_data) {
    height_sampler_ = func;
    sampler_user_data_ = user_data;
}

void GeometryClipmapSystem::Update(const glm::vec3& viewer_pos) {
    if (!initialized_) return;

    for (int i = 0; i < static_cast<int>(levels_.size()); ++i) {
        UpdateLevel(i, viewer_pos);
    }

    last_viewer_pos_ = viewer_pos;
}

void GeometryClipmapSystem::UpdateLevel(int level, const glm::vec3& viewer_pos) {
    auto& lv = levels_[level];
    float cell = lv.cell_size;
    int grid = lv.grid_size;
    int half = grid / 2;

    // 计算观察者对应的网格原点（snap to grid）
    int new_origin_x = static_cast<int>(std::floor(viewer_pos.x / cell)) - half;
    int new_origin_z = static_cast<int>(std::floor(viewer_pos.z / cell)) - half;

    int dx = new_origin_x - lv.origin_x;
    int dz = new_origin_z - lv.origin_z;

    if (dx == 0 && dz == 0 && !lv.needs_fill) return;

    // 如果移动超过半个网格，或首次需要填充，完全重填
    if (std::abs(dx) > half || std::abs(dz) > half || lv.needs_fill) {
        lv.origin_x = new_origin_x;
        lv.origin_z = new_origin_z;
        // 完全重新采样
        for (int z = 0; z < grid; ++z) {
            for (int x = 0; x < grid; ++x) {
                float wx = (lv.origin_x + x) * cell;
                float wz = (lv.origin_z + z) * cell;
                lv.height_data[z * grid + x] = FetchHeight(wx, wz);
            }
        }
        lv.gpu_dirty = true;
        lv.needs_fill = false;
        return;
    }

    // Toroidal update：只更新移动方向上的新行/列
    if (dx != 0) {
        int start_x = dx > 0 ? (grid - dx) : 0;
        int count_x = std::abs(dx);
        int new_ox = lv.origin_x + dx;

        for (int col = 0; col < count_x; ++col) {
            int local_x = (start_x + col) % grid;
            int world_gx = new_ox + (dx > 0 ? (grid - count_x + col) : col);
            for (int z = 0; z < grid; ++z) {
                float wx = world_gx * cell;
                float wz = (lv.origin_z + z) * cell;
                // Toroidal write
                int local_z = ((z % grid) + grid) % grid;
                lv.height_data[local_z * grid + local_x] = FetchHeight(wx, wz);
            }
        }
        lv.origin_x = new_ox;
        lv.gpu_dirty = true;
    }

    if (dz != 0) {
        int start_z = dz > 0 ? (grid - dz) : 0;
        int count_z = std::abs(dz);
        int new_oz = lv.origin_z + dz;

        for (int row = 0; row < count_z; ++row) {
            int local_z = (start_z + row) % grid;
            int world_gz = new_oz + (dz > 0 ? (grid - count_z + row) : row);
            for (int x = 0; x < grid; ++x) {
                float wx = (lv.origin_x + x) * cell;
                float wz = world_gz * cell;
                int local_x = ((x % grid) + grid) % grid;
                lv.height_data[local_z * grid + local_x] = FetchHeight(wx, wz);
            }
        }
        lv.origin_z = new_oz;
        lv.gpu_dirty = true;
    }
}

float GeometryClipmapSystem::FetchHeight(float world_x, float world_z) const {
    if (height_sampler_) {
        return height_sampler_(world_x, world_z, sampler_user_data_) * config_.height_scale;
    }
    return 0.0f;
}

float GeometryClipmapSystem::SampleHeight(float world_x, float world_z) const {
    if (!initialized_ || levels_.empty()) return 0.0f;

    // 使用最精细层插值
    const auto& lv = levels_[0];
    float cell = lv.cell_size;
    int grid = lv.grid_size;

    float local_x = (world_x / cell) - lv.origin_x;
    float local_z = (world_z / cell) - lv.origin_z;

    // Clamp to grid bounds
    local_x = std::max(0.0f, std::min(local_x, static_cast<float>(grid - 2)));
    local_z = std::max(0.0f, std::min(local_z, static_cast<float>(grid - 2)));

    int ix = static_cast<int>(local_x);
    int iz = static_cast<int>(local_z);
    float fx = local_x - ix;
    float fz = local_z - iz;

    // Bilinear interpolation
    float h00 = lv.height_data[iz * grid + ix];
    float h10 = lv.height_data[iz * grid + ix + 1];
    float h01 = lv.height_data[(iz + 1) * grid + ix];
    float h11 = lv.height_data[(iz + 1) * grid + ix + 1];

    float h0 = h00 * (1.0f - fx) + h10 * fx;
    float h1 = h01 * (1.0f - fx) + h11 * fx;
    return h0 * (1.0f - fz) + h1 * fz;
}

std::vector<int> GeometryClipmapSystem::GetDirtyLevels() const {
    std::vector<int> dirty;
    for (int i = 0; i < static_cast<int>(levels_.size()); ++i) {
        if (levels_[i].gpu_dirty) dirty.push_back(i);
    }
    return dirty;
}

void GeometryClipmapSystem::ClearDirty(int level) {
    if (level >= 0 && level < static_cast<int>(levels_.size())) {
        levels_[level].gpu_dirty = false;
    }
}

void GeometryClipmapSystem::Shutdown() {
    levels_.clear();
    initialized_ = false;
    height_sampler_ = nullptr;
    sampler_user_data_ = nullptr;
}

} // namespace terrain
} // namespace dse
