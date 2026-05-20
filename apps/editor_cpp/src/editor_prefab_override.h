#pragma once

#include <string>
#include <vector>
#include <entt/entt.hpp>

namespace dse::editor {

struct EditorContext;

/// Represents a single property override in a prefab instance
struct PrefabPropertyOverride {
    std::string component_name;
    std::string property_name;
    std::string original_value;  // JSON string of original value
    std::string current_value;   // JSON string of current value
};

/// Tracks overrides for a prefab instance entity
struct PrefabOverrideInfo {
    entt::entity entity = entt::null;
    std::string prefab_source_path;  // path to .dprefab file
    std::vector<PrefabPropertyOverride> overrides;
    bool has_new_components = false;
    bool has_removed_components = false;
};

/// Compute the override diff between a prefab instance and its source .dprefab file.
/// Returns empty info if the entity is not a prefab instance.
PrefabOverrideInfo ComputePrefabOverrides(entt::registry& registry, entt::entity entity);

/// Revert a single property override to its prefab source value
bool RevertPrefabOverride(entt::registry& registry, entt::entity entity,
                           const PrefabPropertyOverride& override_info);

/// Revert all overrides on a prefab instance to match the source prefab
bool RevertAllPrefabOverrides(entt::registry& registry, entt::entity entity);

/// Apply current instance values back to the source prefab file
bool ApplyOverridesToPrefab(entt::registry& registry, entt::entity entity);

/// Draw the Prefab Override inspector section (call from within Inspector panel)
void DrawPrefabOverrideSection(EditorContext& ctx);

} // namespace dse::editor
