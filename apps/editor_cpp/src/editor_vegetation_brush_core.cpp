#include "editor_vegetation_brush_core.h"
#include "editor_terrain_panel_core.h"  // GaussianFalloff

#include "engine/ecs/vegetation_mask.h"

#include <algorithm>
#include <cmath>

namespace dse::editor {

void EnsureVegetationMask(dse::VegetationDensityMask& mask,
                          const glm::vec2& world_min,
                          const glm::vec2& world_size,
                          int res_x, int res_z,
                          float init_value) {
    res_x = std::max(res_x, 2);
    res_z = std::max(res_z, 2);

    bool extents_changed = mask.resolution_x != res_x ||
                           mask.resolution_z != res_z ||
                           mask.world_min != world_min ||
                           mask.world_size != world_size;

    if (mask.active() && !extents_changed) return;

    mask.resolution_x = res_x;
    mask.resolution_z = res_z;
    mask.world_min = world_min;
    mask.world_size = world_size;
    mask.weights.assign(static_cast<size_t>(res_x) * res_z,
                        std::clamp(init_value, 0.0f, 1.0f));
}

void ApplyVegetationBrush(dse::VegetationDensityMask& mask,
                          const glm::vec3& world_hit,
                          float radius, float strength, float falloff,
                          bool plant, float delta_time) {
    if (!mask.active()) return;
    if (mask.world_size.x <= 0.0f || mask.world_size.y <= 0.0f) return;

    const int rx = mask.resolution_x;
    const int rz = mask.resolution_z;
    const float dx_world = mask.world_size.x / static_cast<float>(rx - 1);
    const float dz_world = mask.world_size.y / static_cast<float>(rz - 1);
    if (dx_world <= 0.0f || dz_world <= 0.0f) return;

    const int radius_cells_x = static_cast<int>(std::ceil(radius / dx_world));
    const int radius_cells_z = static_cast<int>(std::ceil(radius / dz_world));

    const float gx = (world_hit.x - mask.world_min.x) / mask.world_size.x *
                     static_cast<float>(rx - 1);
    const float gz = (world_hit.z - mask.world_min.y) / mask.world_size.y *
                     static_cast<float>(rz - 1);
    const int center_x = static_cast<int>(std::round(gx));
    const int center_z = static_cast<int>(std::round(gz));

    const float amount = strength * delta_time * 30.0f;  // 归一化到 ~30fps
    const float target = plant ? 1.0f : 0.0f;

    for (int z = center_z - radius_cells_z; z <= center_z + radius_cells_z; ++z) {
        for (int x = center_x - radius_cells_x; x <= center_x + radius_cells_x; ++x) {
            if (x < 0 || x >= rx || z < 0 || z >= rz) continue;

            float wx = mask.world_min.x + static_cast<float>(x) * dx_world;
            float wz = mask.world_min.y + static_cast<float>(z) * dz_world;
            float dist = std::sqrt((wx - world_hit.x) * (wx - world_hit.x) +
                                   (wz - world_hit.z) * (wz - world_hit.z));
            float w = GaussianFalloff(dist, radius, falloff);
            if (w <= 0.0f) continue;

            float& v = mask.weights[static_cast<size_t>(z) * rx + x];
            v += (target - v) * w * amount;
            v = std::clamp(v, 0.0f, 1.0f);
        }
    }
}

}  // namespace dse::editor
