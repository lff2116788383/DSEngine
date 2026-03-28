#include "modules/gameplay_3d/camera/free_camera_controller_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_2d.h" // For TransformComponent
#include "engine/input/input.h"
#include "engine/input/key_code.h"
#include <glm/gtx/quaternion.hpp>
#include <algorithm>

namespace dse {
namespace gameplay3d {

void FreeCameraControllerSystem::Update(World& world, float delta_time) {
    auto view = world.registry().view<FreeCameraControllerComponent, TransformComponent>();

    for (auto entity : view) {
        auto& controller = view.get<FreeCameraControllerComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        if (!controller.enabled) {
            continue;
        }

        // Mouse look (Right click to rotate)
        if (Input::GetMouseButton(MOUSE_BUTTON_RIGHT)) {
            glm::vec2 delta = Input::GetSwipeDelta();
            
            controller.yaw += delta.x * controller.mouse_sensitivity;
            controller.pitch -= delta.y * controller.mouse_sensitivity;

            // Constrain pitch
            controller.pitch = std::clamp(controller.pitch, -89.0f, 89.0f);

            glm::vec3 euler(glm::radians(controller.pitch), glm::radians(controller.yaw), 0.0f);
            transform.rotation = glm::quat(euler);
            transform.dirty = true;
        }

        // Keyboard movement
        glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 right = transform.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f); // World up

        glm::vec3 velocity(0.0f);
        if (Input::GetKey(KEY_CODE_W)) velocity += front;
        if (Input::GetKey(KEY_CODE_S)) velocity -= front;
        if (Input::GetKey(KEY_CODE_A)) velocity -= right;
        if (Input::GetKey(KEY_CODE_D)) velocity += right;
        if (Input::GetKey(KEY_CODE_E)) velocity += up;
        if (Input::GetKey(KEY_CODE_Q)) velocity -= up;

        if (glm::length(velocity) > 0.0f) {
            velocity = glm::normalize(velocity);
            float speed_mult = Input::GetKey(KEY_CODE_LEFT_SHIFT) ? 3.0f : 1.0f;
            transform.position += velocity * controller.move_speed * speed_mult * delta_time;
            transform.dirty = true;
        }
    }
}

} // namespace gameplay3d
} // namespace dse
