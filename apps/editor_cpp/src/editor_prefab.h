#pragma once

#include <string>
#include <entt/entt.hpp>

class World;

namespace dse::editor {

/// Save an entity and its components as a .dprefab JSON file
bool SaveEntityAsPrefab(entt::registry& registry, entt::entity entity, const std::string& file_path);

/// Instantiate a prefab from a .dprefab JSON file, returns the new entity
entt::entity InstantiatePrefab(World& world, entt::registry& registry, const std::string& file_path);

/// Check if an entity is a prefab instance
bool IsPrefabInstance(entt::registry& registry, entt::entity entity);

} // namespace dse::editor
