#include "modules/gameplay_2d/camera/camera_system.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

void CameraSystem::Update(World& world, float aspect_ratio) {
    if (aspect_ratio <= 0.0f) {
        aspect_ratio = 1.0f;
    }

    auto view = world.registry().view<CameraComponent, TransformComponent>();
    for (auto entity : view) {
        auto& camera = view.get<CameraComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        if (camera.orthographic) {
            float half_height = camera.orthographic_size;
            float half_width = half_height * aspect_ratio;
            camera.projection = glm::ortho(-half_width, half_width, -half_height, half_height, camera.near_clip, camera.far_clip);
        } else {
            // For future 3D
            camera.projection = glm::perspective(glm::radians(45.0f), aspect_ratio, camera.near_clip, camera.far_clip);
        }

        // View matrix is the inverse of the camera's transform matrix
        // Assuming camera looks at -Z
        glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        camera.view = glm::lookAt(transform.position, transform.position + front, up);
    }
}
