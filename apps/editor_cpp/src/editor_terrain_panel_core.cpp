#include "editor_terrain_panel_core.h"

#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"

#include <algorithm>
#include <cmath>

namespace dse::editor {

glm::vec2 WorldToScreen(const glm::vec3& world_pos,
                        const glm::mat4& view,
                        const glm::mat4& proj,
                        const glm::vec2& window_pos,
                        const glm::vec2& panel_size) {
    glm::vec4 clip = proj * view * glm::vec4(world_pos, 1.0f);
    if (std::abs(clip.w) < 1e-6f) return glm::vec2(-10000.0f);
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    float sx = window_pos.x + (ndc.x + 1.0f) * 0.5f * panel_size.x;
    float sy = window_pos.y + (1.0f - ndc.y) * 0.5f * panel_size.y;
    return glm::vec2(sx, sy);
}

glm::vec3 ScreenToWorldOnTerrain(const glm::vec2& screen_pos,
                                 const glm::mat4& view,
                                 const glm::mat4& proj,
                                 const glm::vec2& window_pos,
                                 const glm::vec2& panel_size,
                                 float plane_y) {
    float nx = (screen_pos.x - window_pos.x) / panel_size.x * 2.0f - 1.0f;
    float ny = 1.0f - (screen_pos.y - window_pos.y) / panel_size.y * 2.0f;

    glm::mat4 inv_vp = glm::inverse(proj * view);
    glm::vec4 near_pt = inv_vp * glm::vec4(nx, ny, -1.0f, 1.0f);
    glm::vec4 far_pt  = inv_vp * glm::vec4(nx, ny,  1.0f, 1.0f);
    near_pt /= near_pt.w;
    far_pt  /= far_pt.w;

    glm::vec3 ray_origin(near_pt);
    glm::vec3 ray_dir = glm::normalize(glm::vec3(far_pt) - ray_origin);

    // Intersect with Y = plane_y horizontal plane (terrain is XZ)
    if (std::abs(ray_dir.y) < 1e-6f) return glm::vec3(0.0f);
    float t = (plane_y - ray_origin.y) / ray_dir.y;
    return ray_origin + ray_dir * t;
}

bool WorldToTerrainGrid(const glm::vec3& world_pos,
                        const TerrainComponent& terrain,
                        const TransformComponent& tf,
                        float& out_gx, float& out_gz) {
    // Transform to terrain-local space
    float local_x = world_pos.x - tf.position.x;
    float local_z = world_pos.z - tf.position.z;

    // Terrain grid goes from -width/2 to +width/2
    float half_w = terrain.width * 0.5f;
    float half_d = terrain.depth * 0.5f;

    out_gx = (local_x + half_w) / terrain.width * static_cast<float>(terrain.resolution_x - 1);
    out_gz = (local_z + half_d) / terrain.depth * static_cast<float>(terrain.resolution_z - 1);

    return out_gx >= 0 && out_gx < terrain.resolution_x &&
           out_gz >= 0 && out_gz < terrain.resolution_z;
}

float GaussianFalloff(float dist, float radius, float falloff) {
    if (dist >= radius) return 0.0f;
    float t = dist / radius;
    // Lerp between hard (1.0 until edge) and soft (gaussian)
    float hard = 1.0f;
    float soft = std::exp(-4.0f * t * t);
    return hard * (1.0f - falloff) + soft * falloff;
}

void ApplyBrush(TerrainComponent& terrain,
                const TransformComponent& tf,
                const glm::vec3& world_hit,
                const TerrainEditorState& state,
                float delta_time) {
    if (terrain.height_data.empty()) return;
    if (terrain.resolution_x < 2 || terrain.resolution_z < 2) return;

    float gx, gz;
    WorldToTerrainGrid(world_hit, terrain, tf, gx, gz);

    float dx_world = terrain.width / static_cast<float>(terrain.resolution_x - 1);
    float dz_world = terrain.depth / static_cast<float>(terrain.resolution_z - 1);

    // Radius in grid cells
    int radius_cells_x = static_cast<int>(std::ceil(state.brush_radius / dx_world));
    int radius_cells_z = static_cast<int>(std::ceil(state.brush_radius / dz_world));

    int center_x = static_cast<int>(std::round(gx));
    int center_z = static_cast<int>(std::round(gz));

    // For smooth: compute average in brush area first
    float avg_height = 0.0f;
    float total_weight = 0.0f;
    if (state.brush_mode == TerrainBrushMode::Smooth) {
        for (int z = center_z - radius_cells_z; z <= center_z + radius_cells_z; z++) {
            for (int x = center_x - radius_cells_x; x <= center_x + radius_cells_x; x++) {
                if (x < 0 || x >= terrain.resolution_x || z < 0 || z >= terrain.resolution_z) continue;
                float wx = static_cast<float>(x) * dx_world - terrain.width * 0.5f + tf.position.x;
                float wz = static_cast<float>(z) * dz_world - terrain.depth * 0.5f + tf.position.z;
                float dist = std::sqrt((wx - world_hit.x) * (wx - world_hit.x) +
                                       (wz - world_hit.z) * (wz - world_hit.z));
                float w = GaussianFalloff(dist, state.brush_radius, state.brush_falloff);
                if (w > 0.0f) {
                    avg_height += terrain.height_data[z * terrain.resolution_x + x] * w;
                    total_weight += w;
                }
            }
        }
        if (total_weight > 0.0f) avg_height /= total_weight;
    }

    float strength = state.brush_strength * delta_time * 30.0f; // Normalize to ~30fps

    for (int z = center_z - radius_cells_z; z <= center_z + radius_cells_z; z++) {
        for (int x = center_x - radius_cells_x; x <= center_x + radius_cells_x; x++) {
            if (x < 0 || x >= terrain.resolution_x || z < 0 || z >= terrain.resolution_z) continue;

            float wx = static_cast<float>(x) * dx_world - terrain.width * 0.5f + tf.position.x;
            float wz = static_cast<float>(z) * dz_world - terrain.depth * 0.5f + tf.position.z;
            float dist = std::sqrt((wx - world_hit.x) * (wx - world_hit.x) +
                                   (wz - world_hit.z) * (wz - world_hit.z));
            float w = GaussianFalloff(dist, state.brush_radius, state.brush_falloff);
            if (w <= 0.0f) continue;

            int idx = z * terrain.resolution_x + x;
            float& h = terrain.height_data[idx];

            switch (state.brush_mode) {
                case TerrainBrushMode::Raise:
                    h += w * strength;
                    break;
                case TerrainBrushMode::Lower:
                    h -= w * strength;
                    break;
                case TerrainBrushMode::Smooth:
                    h += (avg_height - h) * w * strength;
                    break;
                case TerrainBrushMode::Flatten:
                    h += (state.flatten_target_height - h) * w * strength;
                    break;
            }
            h = std::clamp(h, 0.0f, terrain.max_height);
        }
    }

    terrain.is_dirty = true;
}

void EnsureSplatData(TerrainComponent& terrain) {
    int required = terrain.resolution_x * terrain.resolution_z * 4;
    if (static_cast<int>(terrain.splat_data.size()) != required) {
        terrain.splat_data.resize(required, 0.0f);
        // Initialize layer 0 to 1.0 (default base layer)
        for (int i = 0; i < terrain.resolution_x * terrain.resolution_z; i++) {
            terrain.splat_data[i * 4 + 0] = 1.0f;
            terrain.splat_data[i * 4 + 1] = 0.0f;
            terrain.splat_data[i * 4 + 2] = 0.0f;
            terrain.splat_data[i * 4 + 3] = 0.0f;
        }
    }
}

void ApplySplatBrush(TerrainComponent& terrain,
                     const TransformComponent& tf,
                     const glm::vec3& world_hit,
                     const TerrainEditorState& state,
                     float delta_time) {
    EnsureSplatData(terrain);
    if (terrain.resolution_x < 2 || terrain.resolution_z < 2) return;

    float gx, gz;
    WorldToTerrainGrid(world_hit, terrain, tf, gx, gz);

    float dx_world = terrain.width / static_cast<float>(terrain.resolution_x - 1);
    float dz_world = terrain.depth / static_cast<float>(terrain.resolution_z - 1);

    int radius_cells_x = static_cast<int>(std::ceil(state.brush_radius / dx_world));
    int radius_cells_z = static_cast<int>(std::ceil(state.brush_radius / dz_world));
    int center_x = static_cast<int>(std::round(gx));
    int center_z = static_cast<int>(std::round(gz));

    float opacity = state.splat_brush_opacity * delta_time * 30.0f;
    int layer = std::clamp(state.active_splat_layer, 0, 3);

    for (int z = center_z - radius_cells_z; z <= center_z + radius_cells_z; z++) {
        for (int x = center_x - radius_cells_x; x <= center_x + radius_cells_x; x++) {
            if (x < 0 || x >= terrain.resolution_x || z < 0 || z >= terrain.resolution_z) continue;

            float wx = static_cast<float>(x) * dx_world - terrain.width * 0.5f + tf.position.x;
            float wz = static_cast<float>(z) * dz_world - terrain.depth * 0.5f + tf.position.z;
            float dist = std::sqrt((wx - world_hit.x) * (wx - world_hit.x) +
                                   (wz - world_hit.z) * (wz - world_hit.z));
            float w = GaussianFalloff(dist, state.brush_radius, state.brush_falloff);
            if (w <= 0.0f) continue;

            int base = (z * terrain.resolution_x + x) * 4;
            float add = w * opacity;

            // Increase target layer, decrease others proportionally so sum stays ~1.0
            float old_val = terrain.splat_data[base + layer];
            float new_val = std::min(old_val + add, 1.0f);
            float actual_add = new_val - old_val;
            terrain.splat_data[base + layer] = new_val;

            // Subtract from other layers proportionally
            float others_sum = 0.0f;
            for (int l = 0; l < 4; l++) {
                if (l != layer) others_sum += terrain.splat_data[base + l];
            }
            if (others_sum > 0.0f && actual_add > 0.0f) {
                for (int l = 0; l < 4; l++) {
                    if (l != layer) {
                        float ratio = terrain.splat_data[base + l] / others_sum;
                        terrain.splat_data[base + l] = std::max(0.0f, terrain.splat_data[base + l] - actual_add * ratio);
                    }
                }
            }
        }
    }
    terrain.splat_dirty = true;
}

}  // namespace dse::editor
