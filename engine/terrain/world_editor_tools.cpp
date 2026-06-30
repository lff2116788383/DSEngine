/**
 * @file world_editor_tools.cpp
 * @brief 编辑器世界编辑工具实现
 */

#include "engine/terrain/world_editor_tools.h"
#include <cmath>
#include <algorithm>
#include <random>

namespace dse {
namespace terrain {

void WorldEditorTools::Init() {
    initialized_ = true;
}

void WorldEditorTools::Shutdown() {
    foliage_instances_.clear();
    cell_states_.clear();
    undo_stack_.clear();
    redo_stack_.clear();
    initialized_ = false;
}

float WorldEditorTools::SampleFalloff(float distance, float radius, float falloff) const {
    if (distance >= radius) return 0.0f;
    float normalized = distance / radius;
    if (falloff <= 0.0f) return 1.0f; // Hard edge
    float t = 1.0f - normalized;
    // Smooth hermite falloff
    return t * t * (3.0f - 2.0f * t * (1.0f - falloff));
}

float WorldEditorTools::CalculateBrushWeight(const glm::vec3& point, const BrushParams& params) const {
    float dx = point.x - params.center.x;
    float dz = point.z - params.center.z;

    float dist = 0.0f;
    if (params.shape == BrushShape::Circle) {
        dist = std::sqrt(dx * dx + dz * dz);
    } else { // Square
        dist = std::max(std::abs(dx), std::abs(dz));
    }

    return SampleFalloff(dist, params.radius, params.falloff) * params.strength;
}

glm::vec4 WorldEditorTools::GetBrushPreview(const BrushParams& params) const {
    return glm::vec4(
        params.center.x - params.radius,
        params.center.z - params.radius,
        params.center.x + params.radius,
        params.center.z + params.radius
    );
}

uint32_t WorldEditorTools::ApplyTerrainBrush(TerrainBrushOp op, const BrushParams& params) {
    if (!initialized_) return 0;

    EditorOperation edit_op;
    edit_op.type = EditorOperation::TerrainEdit;
    edit_op.description = "Terrain brush op=" + std::to_string(static_cast<int>(op));

    // Calculate affected grid cells
    int grid_min_x = static_cast<int>(std::floor(params.center.x - params.radius));
    int grid_max_x = static_cast<int>(std::ceil(params.center.x + params.radius));
    int grid_min_z = static_cast<int>(std::floor(params.center.z - params.radius));
    int grid_max_z = static_cast<int>(std::ceil(params.center.z + params.radius));

    for (int gz = grid_min_z; gz <= grid_max_z; ++gz) {
        for (int gx = grid_min_x; gx <= grid_max_x; ++gx) {
            glm::vec3 grid_pos(static_cast<float>(gx), 0.0f, static_cast<float>(gz));
            float weight = CalculateBrushWeight(grid_pos, params);
            if (weight <= 0.0f) continue;

            float current_height = 0.0f;
            if (height_read_func_) current_height = height_read_func_(grid_pos.x, grid_pos.z);

            float new_height = current_height;
            switch (op) {
                case TerrainBrushOp::RaiseHeight:
                    new_height += weight;
                    break;
                case TerrainBrushOp::LowerHeight:
                    new_height -= weight;
                    break;
                case TerrainBrushOp::FlattenHeight:
                    new_height = current_height + (params.target_height - current_height) * weight;
                    break;
                case TerrainBrushOp::SmoothHeight: {
                    // Average neighbors
                    float avg = current_height;
                    int count = 1;
                    if (height_read_func_) {
                        for (int dz = -1; dz <= 1; ++dz) {
                            for (int dx = -1; dx <= 1; ++dx) {
                                if (dx == 0 && dz == 0) continue;
                                avg += height_read_func_(grid_pos.x + dx, grid_pos.z + dz);
                                count++;
                            }
                        }
                    }
                    avg /= count;
                    new_height = current_height + (avg - current_height) * weight;
                    break;
                }
                default:
                    break;
            }

            if (height_write_func_ && new_height != current_height) {
                height_write_func_(gx, gz, new_height);
            }
        }
    }

    uint32_t op_id = next_op_id_++;
    PushUndoOp(std::move(edit_op));
    return op_id;
}

uint32_t WorldEditorTools::PlaceFoliage(const FoliageBrushParams& params) {
    if (!initialized_) return 0;

    EditorOperation edit_op;
    edit_op.type = EditorOperation::FoliagePlace;

    uint32_t placed = 0;
    std::mt19937 rng(static_cast<uint32_t>(params.center.x * 1000 + params.center.z));
    std::uniform_real_distribution<float> dist_pos(-params.radius, params.radius);
    std::uniform_real_distribution<float> dist_scale(params.min_scale, params.max_scale);
    std::uniform_real_distribution<float> dist_rot(0.0f, params.random_rotation);

    // Calculate expected instance count from density
    float area = 3.14159f * params.radius * params.radius;
    int target_count = static_cast<int>(area * params.density);

    for (int i = 0; i < target_count; ++i) {
        float ox = dist_pos(rng);
        float oz = dist_pos(rng);

        // Check if within circle
        if (ox * ox + oz * oz > params.radius * params.radius) continue;

        glm::vec3 pos = params.center + glm::vec3(ox, 0, oz);

        // Sample terrain height
        if (height_read_func_) {
            pos.y = height_read_func_(pos.x, pos.z);
        }

        FoliageInstance inst;
        inst.position = pos;
        inst.rotation = glm::vec3(0, dist_rot(rng), 0);
        inst.scale = dist_scale(rng);
        inst.mesh_path = params.mesh_path;
        inst.instance_id = next_instance_id_++;

        foliage_instances_.push_back(inst);
        placed++;
    }

    edit_op.description = "Place " + std::to_string(placed) + " foliage instances";
    PushUndoOp(std::move(edit_op));
    return placed;
}

uint32_t WorldEditorTools::EraseFoliage(const glm::vec3& center, float radius) {
    uint32_t erased = 0;
    float r2 = radius * radius;

    foliage_instances_.erase(
        std::remove_if(foliage_instances_.begin(), foliage_instances_.end(),
            [&](const FoliageInstance& inst) {
                float dx = inst.position.x - center.x;
                float dz = inst.position.z - center.z;
                if (dx * dx + dz * dz <= r2) { erased++; return true; }
                return false;
            }),
        foliage_instances_.end());

    if (erased > 0) {
        EditorOperation edit_op;
        edit_op.type = EditorOperation::FoliageErase;
        edit_op.description = "Erase " + std::to_string(erased) + " foliage";
        PushUndoOp(std::move(edit_op));
    }
    return erased;
}

std::vector<FoliageInstance> WorldEditorTools::GetFoliageInRadius(const glm::vec3& center, float radius) const {
    std::vector<FoliageInstance> result;
    float r2 = radius * radius;
    for (const auto& inst : foliage_instances_) {
        float dx = inst.position.x - center.x;
        float dz = inst.position.z - center.z;
        if (dx * dx + dz * dz <= r2) result.push_back(inst);
    }
    return result;
}

uint32_t WorldEditorTools::BeginRoadDraw(float road_width) {
    (void)road_width;
    return next_road_session_++;
}

void WorldEditorTools::AddRoadPoint(uint32_t session_id, const glm::vec3& point) {
    (void)session_id; (void)point;
    // Integration with SplineSystem handled at editor level
}

void WorldEditorTools::EndRoadDraw(uint32_t session_id) {
    (void)session_id;
    EditorOperation edit_op;
    edit_op.type = EditorOperation::RoadDraw;
    edit_op.description = "Draw road";
    PushUndoOp(std::move(edit_op));
}

void WorldEditorTools::CancelRoadDraw(uint32_t session_id) {
    (void)session_id;
}

void WorldEditorTools::UpdatePartitionVisualization(const glm::vec3& camera_pos, float cell_size) {
    cell_states_.clear();
    int view_range = 4; // Show 4 cells in each direction

    for (int cz = -view_range; cz <= view_range; ++cz) {
        for (int cx = -view_range; cx <= view_range; ++cx) {
            int abs_cx = static_cast<int>(std::floor(camera_pos.x / cell_size)) + cx;
            int abs_cz = static_cast<int>(std::floor(camera_pos.z / cell_size)) + cz;

            CellVisualState state;
            state.cell_x = abs_cx;
            state.cell_y = abs_cz;
            state.distance_to_camera = std::sqrt(
                static_cast<float>(cx * cx + cz * cz)) * cell_size;

            // Determine load state based on distance
            if (state.distance_to_camera < cell_size * 2) {
                state.loaded = true;
                state.lod_level = 0;
            } else if (state.distance_to_camera < cell_size * 3) {
                state.loaded = true;
                state.lod_level = 1;
            } else {
                state.loaded = false;
                state.lod_level = 2;
            }

            cell_states_.push_back(state);
        }
    }
}

bool WorldEditorTools::Undo() {
    if (undo_stack_.empty()) return false;
    redo_stack_.push_back(std::move(undo_stack_.back()));
    undo_stack_.pop_back();
    return true;
}

bool WorldEditorTools::Redo() {
    if (redo_stack_.empty()) return false;
    undo_stack_.push_back(std::move(redo_stack_.back()));
    redo_stack_.pop_back();
    return true;
}

void WorldEditorTools::PushUndoOp(EditorOperation op) {
    undo_stack_.push_back(std::move(op));
    redo_stack_.clear(); // Clear redo on new action
}

} // namespace terrain
} // namespace dse
