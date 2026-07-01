/**
 * @file blueprint_system.h
 * @brief ECS system that ticks all BlueprintComponents via ParallelEach
 */

#ifndef DSE_ECS_BLUEPRINT_SYSTEM_H
#define DSE_ECS_BLUEPRINT_SYSTEM_H

namespace dse {

/**
 * @class BlueprintSystem
 * @brief Iterates all entities with BlueprintComponent and ticks their VM.
 *        Uses ECS ParallelEach for multi-threaded execution when possible.
 *        Supports LOD scheduling: entities with tick_interval > 0 only tick
 *        at the specified interval (reuses AI LOD pattern).
 */
class BlueprintSystem {
public:
    /// Initialize the system, register default extern functions in the VM
    static void Init();

    /// Tick all blueprint components. Called once per frame from game loop.
    /// @param dt Delta time in seconds
    static void Update(float dt);

    /// Get number of blueprints ticked last frame (for profiling)
    static int GetLastTickCount();

    /// Hot-reload: recompile a specific blueprint asset and update all instances
    static void HotReload(const char* asset_path);

private:
    static int s_last_tick_count;
};

} // namespace dse

#endif // DSE_ECS_BLUEPRINT_SYSTEM_H
