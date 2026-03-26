/**
 * @file transform_system.cpp
 * @brief 引擎核心模块，提供基础功能支持
 */

#include "engine/scene/transform_system.h"
#include <glm/gtx/quaternion.hpp>
#include <cstdint>
#include <unordered_map>

void TransformSystem::Update(World& world) {
    auto& registry = world.registry();
    auto view = registry.view<TransformComponent>();
    std::unordered_map<std::uint32_t, unsigned char> visit_state;
    auto entity_key = [](entt::entity entity) {
        return static_cast<std::uint32_t>(entity);
    };
    for (auto entity : view) {
        visit_state[entity_key(entity)] = 0;
    }
    auto resolve_world_matrix = [&](auto&& self, entt::entity entity) -> glm::mat4 {
        auto it = visit_state.find(entity_key(entity));
        if (it != visit_state.end()) {
            if (it->second == 2) {
                return registry.get<TransformComponent>(entity).local_to_world;
            }
            if (it->second == 1) {
                return glm::mat4(1.0f);
            }
            it->second = 1;
        }
        auto& transform = registry.get<TransformComponent>(entity);
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), transform.position);
        glm::mat4 rotation = glm::toMat4(transform.rotation);
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), transform.scale);
        glm::mat4 local_matrix = translation * rotation * scale;
        glm::mat4 parent_matrix = glm::mat4(1.0f);
        if (registry.all_of<ParentComponent>(entity)) {
            auto parent = registry.get<ParentComponent>(entity).parent;
            if (parent != entt::null && registry.valid(parent) && registry.all_of<TransformComponent>(parent)) {
                parent_matrix = self(self, parent);
            }
        }
        transform.local_to_world = parent_matrix * local_matrix;
        transform.dirty = false;
        if (it != visit_state.end()) {
            it->second = 2;
        }
        return transform.local_to_world;
    };
    for (auto entity : view) {
        resolve_world_matrix(resolve_world_matrix, entity);
    }
}
