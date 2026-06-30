/**
 * @file terrain_deformation.cpp
 * @brief 运行时地形形变系统实现
 */

#include "engine/terrain/terrain_deformation.h"
#include "engine/terrain/geometry_clipmap.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <cstring>

namespace dse {
namespace terrain {

void TerrainDeformationSystem::Init(const TerrainDeformConfig& config) {
    config_ = config;
    initialized_ = true;
}

void TerrainDeformationSystem::Shutdown() {
    history_.clear();
    undo_stack_.clear();
    heightmap_ = nullptr;
    initialized_ = false;
}

void TerrainDeformationSystem::SetHeightmap(float* data, uint32_t width, uint32_t height,
                                             float cell_size, const glm::vec3& origin) {
    heightmap_ = data;
    hm_width_ = width;
    hm_height_ = height;
    hm_cell_size_ = cell_size;
    hm_origin_ = origin;
}

uint32_t TerrainDeformationSystem::ApplyDeformation(const DeformationOp& op) {
    if (!heightmap_ || !initialized_) return 0;

    DeformationRecord record;
    record.id = next_record_id_++;
    record.op = op;

    ApplyOpToHeightmap(op, record);

    if (config_.enable_history) {
        history_.push_back(record);
        undo_stack_.clear(); // New op clears redo stack
        if (history_.size() > config_.max_history) {
            history_.erase(history_.begin());
        }
    }

    NotifyChange(op.center, op.radius);
    return record.id;
}

bool TerrainDeformationSystem::Undo() {
    if (history_.empty()) return false;

    auto record = std::move(history_.back());
    history_.pop_back();

    // Restore original heights
    for (int gz = 0; gz < record.grid_height_count; ++gz) {
        for (int gx = 0; gx < record.grid_width; ++gx) {
            int hx = record.grid_x + gx;
            int hz = record.grid_z + gz;
            if (hx >= 0 && hx < static_cast<int>(hm_width_) &&
                hz >= 0 && hz < static_cast<int>(hm_height_)) {
                int idx = hz * static_cast<int>(hm_width_) + hx;
                int backup_idx = gz * record.grid_width + gx;
                heightmap_[idx] = record.original_heights[backup_idx];
            }
        }
    }

    NotifyChange(record.op.center, record.op.radius);
    undo_stack_.push_back(std::move(record));
    return true;
}

bool TerrainDeformationSystem::Redo() {
    if (undo_stack_.empty()) return false;

    auto record = std::move(undo_stack_.back());
    undo_stack_.pop_back();

    // Re-apply the operation (re-backup current state)
    DeformationRecord new_record;
    new_record.id = record.id;
    new_record.op = record.op;
    ApplyOpToHeightmap(record.op, new_record);

    history_.push_back(std::move(new_record));
    NotifyChange(record.op.center, record.op.radius);
    return true;
}

void TerrainDeformationSystem::ClearHistory() {
    history_.clear();
    undo_stack_.clear();
}

float TerrainDeformationSystem::SampleHeight(float world_x, float world_z) const {
    if (!heightmap_) return 0.0f;

    float fx = (world_x - hm_origin_.x) / hm_cell_size_;
    float fz = (world_z - hm_origin_.z) / hm_cell_size_;

    int ix = static_cast<int>(std::floor(fx));
    int iz = static_cast<int>(std::floor(fz));

    if (ix < 0 || ix >= static_cast<int>(hm_width_) - 1 ||
        iz < 0 || iz >= static_cast<int>(hm_height_) - 1) {
        return 0.0f;
    }

    float tx = fx - ix;
    float tz = fz - iz;

    float h00 = heightmap_[iz * hm_width_ + ix];
    float h10 = heightmap_[iz * hm_width_ + ix + 1];
    float h01 = heightmap_[(iz + 1) * hm_width_ + ix];
    float h11 = heightmap_[(iz + 1) * hm_width_ + ix + 1];

    float h0 = h00 * (1.0f - tx) + h10 * tx;
    float h1 = h01 * (1.0f - tx) + h11 * tx;
    return h0 * (1.0f - tz) + h1 * tz;
}

void TerrainDeformationSystem::ApplyOpToHeightmap(const DeformationOp& op, DeformationRecord& record) {
    // Compute affected grid region
    int min_gx, min_gz, max_gx, max_gz;
    WorldToGrid(op.center.x - op.radius, op.center.z - op.radius, min_gx, min_gz);
    WorldToGrid(op.center.x + op.radius, op.center.z + op.radius, max_gx, max_gz);

    min_gx = std::max(0, min_gx);
    min_gz = std::max(0, min_gz);
    max_gx = std::min(static_cast<int>(hm_width_) - 1, max_gx);
    max_gz = std::min(static_cast<int>(hm_height_) - 1, max_gz);

    int width = max_gx - min_gx + 1;
    int height = max_gz - min_gz + 1;

    if (width <= 0 || height <= 0) return;

    record.grid_x = min_gx;
    record.grid_z = min_gz;
    record.grid_width = width;
    record.grid_height_count = height;
    record.original_heights.resize(width * height);

    // Backup and modify
    for (int gz = 0; gz < height; ++gz) {
        for (int gx = 0; gx < width; ++gx) {
            int hx = min_gx + gx;
            int hz = min_gz + gz;
            int idx = hz * static_cast<int>(hm_width_) + hx;
            int backup_idx = gz * width + gx;

            record.original_heights[backup_idx] = heightmap_[idx];

            // Compute world position of this grid cell
            float wx = hm_origin_.x + hx * hm_cell_size_;
            float wz = hm_origin_.z + hz * hm_cell_size_;
            float dx = wx - op.center.x;
            float dz = wz - op.center.z;
            float dist = std::sqrt(dx * dx + dz * dz);

            if (dist > op.radius) continue;

            float weight = ComputeBrushWeight(op, dist);
            float current_h = heightmap_[idx];
            float new_h = current_h;

            switch (op.type) {
                case DeformationType::Raise:
                    new_h = current_h + op.strength * weight;
                    break;
                case DeformationType::Lower:
                    new_h = current_h - op.strength * weight;
                    break;
                case DeformationType::Flatten:
                    new_h = current_h + (op.target_height - current_h) * weight * op.strength;
                    break;
                case DeformationType::Smooth: {
                    // Average neighbors
                    float sum = 0.0f;
                    int count = 0;
                    for (int dzi = -1; dzi <= 1; ++dzi) {
                        for (int dxi = -1; dxi <= 1; ++dxi) {
                            int nx = hx + dxi;
                            int nz = hz + dzi;
                            if (nx >= 0 && nx < static_cast<int>(hm_width_) &&
                                nz >= 0 && nz < static_cast<int>(hm_height_)) {
                                sum += heightmap_[nz * hm_width_ + nx];
                                ++count;
                            }
                        }
                    }
                    float avg = (count > 0) ? sum / count : current_h;
                    new_h = current_h + (avg - current_h) * weight * op.strength;
                    break;
                }
                case DeformationType::Noise:
                    // Simple deterministic noise based on position
                    new_h = current_h + op.strength * weight *
                        std::sin(wx * 3.7f) * std::cos(wz * 2.3f);
                    break;
                case DeformationType::Stamp:
                    if (!op.custom_brush.empty() && op.custom_brush_size > 0) {
                        float u = (dx / op.radius + 1.0f) * 0.5f;
                        float v = (dz / op.radius + 1.0f) * 0.5f;
                        int bx = static_cast<int>(u * op.custom_brush_size);
                        int bz = static_cast<int>(v * op.custom_brush_size);
                        bx = std::clamp(bx, 0, static_cast<int>(op.custom_brush_size) - 1);
                        bz = std::clamp(bz, 0, static_cast<int>(op.custom_brush_size) - 1);
                        float stamp_val = op.custom_brush[bz * op.custom_brush_size + bx];
                        new_h = current_h + stamp_val * op.strength * weight;
                    }
                    break;
            }

            // Clamp
            new_h = std::clamp(new_h, config_.min_height, config_.max_height);
            heightmap_[idx] = new_h;
        }
    }
}

float TerrainDeformationSystem::ComputeBrushWeight(const DeformationOp& op, float dist) const {
    if (op.radius <= 0.0f) return 0.0f;
    float t = dist / op.radius;
    if (t >= 1.0f) return 0.0f;

    switch (op.shape) {
        case BrushShape::Circle:
            return std::pow(1.0f - t, op.falloff);
        case BrushShape::Square:
            return 1.0f; // Square has full weight everywhere inside radius
        case BrushShape::Custom:
            return std::pow(1.0f - t, op.falloff);
    }
    return 0.0f;
}

void TerrainDeformationSystem::WorldToGrid(float wx, float wz, int& gx, int& gz) const {
    gx = static_cast<int>(std::floor((wx - hm_origin_.x) / hm_cell_size_));
    gz = static_cast<int>(std::floor((wz - hm_origin_.z) / hm_cell_size_));
}

void TerrainDeformationSystem::NotifyChange(const glm::vec3& center, float radius) {
    if (callback_) {
        callback_(center, radius);
    }
}

bool TerrainDeformationSystem::Serialize(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    uint32_t magic = 0x4D524644; // 'DFMR'
    uint32_t version = 1;
    uint32_t count = static_cast<uint32_t>(history_.size());

    file.write(reinterpret_cast<const char*>(&magic), 4);
    file.write(reinterpret_cast<const char*>(&version), 4);
    file.write(reinterpret_cast<const char*>(&count), 4);

    for (const auto& record : history_) {
        file.write(reinterpret_cast<const char*>(&record.op.type), sizeof(record.op.type));
        file.write(reinterpret_cast<const char*>(&record.op.shape), sizeof(record.op.shape));
        file.write(reinterpret_cast<const char*>(&record.op.center), sizeof(glm::vec3));
        file.write(reinterpret_cast<const char*>(&record.op.radius), sizeof(float));
        file.write(reinterpret_cast<const char*>(&record.op.strength), sizeof(float));
        file.write(reinterpret_cast<const char*>(&record.op.target_height), sizeof(float));
        file.write(reinterpret_cast<const char*>(&record.op.falloff), sizeof(float));
    }

    return file.good();
}

bool TerrainDeformationSystem::Deserialize(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    uint32_t magic, version, count;
    file.read(reinterpret_cast<char*>(&magic), 4);
    file.read(reinterpret_cast<char*>(&version), 4);
    file.read(reinterpret_cast<char*>(&count), 4);

    if (magic != 0x4D524644 || version != 1) return false;

    for (uint32_t i = 0; i < count; ++i) {
        DeformationOp op;
        file.read(reinterpret_cast<char*>(&op.type), sizeof(op.type));
        file.read(reinterpret_cast<char*>(&op.shape), sizeof(op.shape));
        file.read(reinterpret_cast<char*>(&op.center), sizeof(glm::vec3));
        file.read(reinterpret_cast<char*>(&op.radius), sizeof(float));
        file.read(reinterpret_cast<char*>(&op.strength), sizeof(float));
        file.read(reinterpret_cast<char*>(&op.target_height), sizeof(float));
        file.read(reinterpret_cast<char*>(&op.falloff), sizeof(float));
        ApplyDeformation(op);
    }

    return file.good();
}

} // namespace terrain
} // namespace dse
