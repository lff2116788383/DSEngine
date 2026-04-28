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
    std::unordered_map<std::uint32_t, bool> changed_state;
    auto entity_key = [](entt::entity entity) {
        return static_cast<std::uint32_t>(entity);
    };
    for (auto entity : view) {
        visit_state[entity_key(entity)] = 0;
    }
    auto resolve_world_matrix = [&](auto&& self, entt::entity entity) -> std::pair<glm::mat4, bool> {
        auto key = entity_key(entity);
        auto it = visit_state.find(key);
        if (it != visit_state.end()) {
            if (it->second == 2) {
                return {registry.get<TransformComponent>(entity).local_to_world, changed_state[key]};
            }
            if (it->second == 1) {
                return {glm::mat4(1.0f), false};
            }
            it->second = 1;
        }
        auto& transform = registry.get<TransformComponent>(entity);
        glm::mat4 parent_matrix = glm::mat4(1.0f);
        bool parent_changed = false;
        if (registry.all_of<ParentComponent>(entity)) {
            auto parent = registry.get<ParentComponent>(entity).parent;
            if (parent != entt::null && registry.valid(parent) && registry.all_of<TransformComponent>(parent)) {
                auto parent_result = self(self, parent);
                parent_matrix = parent_result.first;
                parent_changed = parent_result.second;
            }
        }

        const bool should_update = transform.dirty || parent_changed;
        if (should_update) {
            glm::mat4 translation = glm::translate(glm::mat4(1.0f), transform.position);
            glm::mat4 rotation = glm::toMat4(transform.rotation);
            glm::mat4 scale = glm::scale(glm::mat4(1.0f), transform.scale);
            glm::mat4 local_matrix = translation * rotation * scale;
            transform.local_to_world = parent_matrix * local_matrix;
            transform.dirty = false;
        }

        if (it != visit_state.end()) {
            it->second = 2;
            changed_state[key] = should_update;
        }
        return {transform.local_to_world, should_update};
    };
    for (auto entity : view) {
        resolve_world_matrix(resolve_world_matrix, entity);
    }
}
