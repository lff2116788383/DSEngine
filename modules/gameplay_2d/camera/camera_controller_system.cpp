/**
 * @file camera_controller_system.cpp
 * @brief 2D 相机控制器系统实现
 */

#include "modules/gameplay_2d/camera/camera_controller_system.h"
#include "engine/ecs/camera_controller_2d.h"
#include "engine/ecs/camera.h"
#include "engine/ecs/transform.h"
#include <cmath>
#include <algorithm>

// Simple noise for shake (using sine-based pseudo-random)
static float ShakeNoise(float t, float freq) {
    return std::sin(t * freq * 6.2832f) * std::cos(t * freq * 4.517f + 1.3f);
}

void CameraControllerSystem::Update(World& world, float delta_time) {
    auto& reg = world.registry();
    auto view = reg.view<CameraController2DComponent, TransformComponent, CameraComponent>();

    for (auto entity : view) {
        auto& ctrl = view.get<CameraController2DComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);
        auto& cam = view.get<CameraComponent>(entity);
        if (!ctrl.enabled) continue;

        // --- Screen Shake ---
        auto& shake = ctrl.shake;
        if (shake.trauma > 0.0f) {
            shake.time_acc += delta_time;
            shake.trauma = std::max(0.0f, shake.trauma - shake.decay_rate * delta_time);

            float intensity = shake.trauma * shake.trauma; // Quadratic for natural feel
            ctrl.shake_offset.x = ShakeNoise(shake.time_acc, shake.frequency) * shake.max_offset * intensity;
            ctrl.shake_offset.y = ShakeNoise(shake.time_acc + 100.0f, shake.frequency) * shake.max_offset * intensity;
            ctrl.shake_rotation = ShakeNoise(shake.time_acc + 200.0f, shake.frequency * 0.7f) * shake.max_rotation * intensity;
        } else {
            ctrl.shake_offset = {0.0f, 0.0f};
            ctrl.shake_rotation = 0.0f;
        }

        // Apply shake offset to transform
        transform.position.x += ctrl.shake_offset.x;
        transform.position.y += ctrl.shake_offset.y;

        // --- Look Ahead ---
        // Look ahead is based on camera follow target velocity (approximation via position delta)
        // Simple exponential approach to target look-ahead
        float approach = 1.0f - std::exp(-ctrl.look_ahead_speed * delta_time);
        glm::vec2 target_ahead = {ctrl.look_ahead_x, ctrl.look_ahead_y};
        ctrl.look_ahead_current = glm::mix(ctrl.look_ahead_current, target_ahead, approach);
        transform.position.x += ctrl.look_ahead_current.x;
        transform.position.y += ctrl.look_ahead_current.y;

        // --- Smooth Zoom ---
        float target = std::clamp(ctrl.target_zoom, ctrl.min_zoom, ctrl.max_zoom);
        float zoom_approach = 1.0f - std::exp(-ctrl.zoom_speed * delta_time);
        cam.orthographic_size = glm::mix(cam.orthographic_size, 5.0f / target, zoom_approach);

        // --- Bounds Clamping ---
        if (ctrl.bounds.enabled) {
            float half_h = cam.orthographic_size;
            float half_w = half_h * cam.aspect_ratio;
            transform.position.x = std::clamp(transform.position.x,
                ctrl.bounds.min_x + half_w, ctrl.bounds.max_x - half_w);
            transform.position.y = std::clamp(transform.position.y,
                ctrl.bounds.min_y + half_h, ctrl.bounds.max_y - half_h);
        }
    }
}
