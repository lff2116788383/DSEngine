/**
 * @file world_canvas.cpp
 * @brief WorldCanvasSystem implementation — billboard, distance cull, scale.
 */

#include "engine/ui/world_canvas.h"

#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"

#include <cmath>
#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

namespace dse::ui {

void WorldCanvasSystem::Update(const World& world,
                               const glm::vec3& camera_position,
                               const glm::mat4& view,
                               const glm::mat4& /*projection*/) {
    visible_.clear();
    const auto& reg = world.registry();

    auto canvas_view = reg.view<TransformComponent, WorldCanvasComponent>();
    for (auto entity : canvas_view) {
        const auto& tf = canvas_view.get<TransformComponent>(entity);
        const auto& wc = canvas_view.get<WorldCanvasComponent>(entity);
        if (!wc.enabled) continue;

        const glm::vec3 world_pos = tf.position + wc.world_offset;
        const glm::vec3 to_cam = camera_position - world_pos;
        const float dist = glm::length(to_cam);

        // Distance culling
        if (dist > wc.max_distance) continue;

        // Compute distance-based scale
        float applied_scale = 1.0f;
        if (wc.scale_by_distance && wc.reference_distance > 0.0f) {
            applied_scale = dist / wc.reference_distance;
            applied_scale = std::clamp(applied_scale, wc.min_scale, wc.max_scale);
        }

        // Build billboard (or axis-aligned) model matrix
        glm::mat4 model(1.0f);
        if (wc.billboard && dist > 0.001f) {
            // Extract camera right/up from view matrix (transpose of upper-left 3x3)
            const glm::vec3 cam_right = glm::vec3(view[0][0], view[1][0], view[2][0]);
            const glm::vec3 cam_up    = glm::vec3(view[0][1], view[1][1], view[2][1]);
            const glm::vec3 cam_fwd   = -glm::vec3(view[0][2], view[1][2], view[2][2]);

            const float s = wc.canvas_scale * applied_scale;
            model[0] = glm::vec4(cam_right * s, 0.0f);
            model[1] = glm::vec4(cam_up * s, 0.0f);
            model[2] = glm::vec4(cam_fwd * s, 0.0f);
            model[3] = glm::vec4(world_pos, 1.0f);
        } else {
            const float s = wc.canvas_scale * applied_scale;
            model = glm::translate(glm::mat4(1.0f), world_pos);
            model = glm::scale(model, glm::vec3(s));
        }

        WorldCanvasInstance inst;
        inst.transform = model;
        inst.distance = dist;
        inst.applied_scale = applied_scale;
        inst.entity_index = static_cast<uint32_t>(entity);
        inst.canvas = &wc;
        visible_.push_back(inst);
    }

    // Sort front-to-back for transparency ordering (painter's algorithm reversed)
    std::sort(visible_.begin(), visible_.end(),
              [](const WorldCanvasInstance& a, const WorldCanvasInstance& b) {
                  return a.distance < b.distance;
              });
}

} // namespace dse::ui
