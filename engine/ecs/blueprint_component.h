/**
 * @file blueprint_component.h
 * @brief ECS component for Blueprint instances - per-entity runtime state
 */

#ifndef DSE_ECS_BLUEPRINT_COMPONENT_H
#define DSE_ECS_BLUEPRINT_COMPONENT_H

#include <string>
#include <cstdint>

/**
 * @struct BlueprintComponent
 * @brief Attaches a compiled blueprint to an entity. The BlueprintSystem
 *        ticks all entities with this component each frame.
 *
 * Design: shared class table (methods) + per-entity C++ state.
 * The compiled bytecode is shared across all instances of the same blueprint.
 * Only the variable state buffer is unique per-entity.
 */
struct BlueprintComponent {
    std::string blueprint_asset_path;  ///< Path to the .dbp asset file
    uint32_t compiled_index = 0;       ///< Index into shared compiled blueprint cache
    bool enabled = true;               ///< Whether to tick this blueprint
    bool initialized = false;          ///< Whether on_init has been called
    float tick_interval = 0.0f;        ///< LOD: if > 0, tick at this interval instead of every frame
    float time_accumulator = 0.0f;     ///< Accumulator for LOD tick scheduling
};

#endif // DSE_ECS_BLUEPRINT_COMPONENT_H
