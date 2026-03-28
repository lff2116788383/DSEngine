/**
 * @file camera_system.cpp
 * @brief 摄像机系统，管理视图矩阵、投影矩阵和屏幕视口映射
 */

#include "modules/gameplay_2d/camera/camera_system.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

void CameraSystem::Update(World& world, float aspect_ratio) {
    if (aspect_ratio <= 0.0f) {
        aspect_ratio = 1.0f;
    }

    auto view3d = world.registry().view<dse::Camera3DComponent, TransformComponent>();
    for (auto entity : view3d) {
        auto& camera = view3d.get<dse::Camera3DComponent>(entity);
        auto& transform = view3d.get<TransformComponent>(entity);
        auto* follow = world.registry().try_get<CameraFollowComponent>(entity);
        if (follow && follow->enabled && follow->target != entt::null && world.registry().valid(follow->target) && world.registry().all_of<TransformComponent>(follow->target)) {
            const auto& target_tf = world.registry().get<TransformComponent>(follow->target);
            glm::vec3 desired = target_tf.position + follow->offset;
            glm::vec3 delta = desired - transform.position;
            if (!follow->follow_x || glm::abs(delta.x) <= follow->dead_zone.x) {
                delta.x = 0.0f;
            }
            if (!follow->follow_y || glm::abs(delta.y) <= follow->dead_zone.y) {
                delta.y = 0.0f;
            }
            float smoothing = follow->damping;
            if (smoothing < 0.0f) {
                smoothing = 0.0f;
            }
            if (smoothing > 1.0f) {
                smoothing = 1.0f;
            }
            transform.position += delta * smoothing;
            transform.dirty = true;
        }

        camera.aspect_ratio = aspect_ratio;
        float fov = camera.fov;
        if (fov < 1.0f) {
            fov = 1.0f;
        }
        if (fov > 179.0f) {
            fov = 179.0f;
        }
        float near_clip = camera.near_clip;
        if (near_clip <= 0.0f) {
            near_clip = 0.1f;
        }
        float far_clip = camera.far_clip;
        if (far_clip <= near_clip) {
            far_clip = near_clip + 1000.0f;
        }
        camera.projection = glm::perspective(glm::radians(fov), aspect_ratio, near_clip, far_clip);

        glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        camera.view = glm::lookAt(transform.position, transform.position + front, up);
    }

    auto view = world.registry().view<CameraComponent, TransformComponent>();
    for (auto entity : view) {
        auto& camera = view.get<CameraComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);
        if (!camera.enabled) {
            continue;
        }
        auto* follow = world.registry().try_get<CameraFollowComponent>(entity);
        if (follow && follow->enabled && follow->target != entt::null && world.registry().valid(follow->target) && world.registry().all_of<TransformComponent>(follow->target)) {
            const auto& target_tf = world.registry().get<TransformComponent>(follow->target);
            glm::vec3 desired = target_tf.position + follow->offset;
            glm::vec3 delta = desired - transform.position;
            if (!follow->follow_x || glm::abs(delta.x) <= follow->dead_zone.x) {
                delta.x = 0.0f;
            }
            if (!follow->follow_y || glm::abs(delta.y) <= follow->dead_zone.y) {
                delta.y = 0.0f;
            }
            float smoothing = follow->damping;
            if (smoothing < 0.0f) {
                smoothing = 0.0f;
            }
            if (smoothing > 1.0f) {
                smoothing = 1.0f;
            }
            transform.position += delta * smoothing;
            transform.dirty = true;
        }

        if (camera.orthographic) {
            float half_height = camera.orthographic_size;
            float half_width = half_height * aspect_ratio;
            camera.projection = glm::ortho(-half_width, half_width, -half_height, half_height, camera.near_clip, camera.far_clip);
        } else {
            camera.aspect_ratio = aspect_ratio;
            float fov = camera.fov;
            if (fov < 1.0f) {
                fov = 1.0f;
            }
            if (fov > 179.0f) {
                fov = 179.0f;
            }
            float near_clip = camera.near_clip;
            if (near_clip <= 0.0f) {
                near_clip = 0.1f;
            }
            float far_clip = camera.far_clip;
            if (far_clip <= near_clip) {
                far_clip = near_clip + 1000.0f;
            }
            camera.projection = glm::perspective(glm::radians(fov), aspect_ratio, near_clip, far_clip);
        }

        // View matrix is the inverse of the camera's transform matrix
        // Assuming camera looks at -Z
        glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        camera.view = glm::lookAt(transform.position, transform.position + front, up);
    }
}
