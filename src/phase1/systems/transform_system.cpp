#include "phase1/systems/transform_system.h"
#include <glm/gtx/quaternion.hpp>

void TransformSystem::Update(Phase1World& world) {
    auto view = world.registry().view<TransformComponent>();
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        if (!transform.dirty) {
            continue;
        }
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), transform.position);
        glm::mat4 rotation = glm::toMat4(transform.rotation);
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), transform.scale);
        transform.local_to_world = translation * rotation * scale;
        transform.dirty = false;
    }
}
