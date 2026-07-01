/**
 * @file entity_interpolation.h
 * @brief Snapshot interpolation for remote (non-locally-controlled) entities.
 *
 * Buffers received server snapshots and interpolates between them at render time
 * to produce smooth movement despite network jitter.
 *
 * Usage:
 *   EntityInterpolation interp;
 *   interp.SetInterpolationDelay(0.1f);  // 100ms buffer
 *   // On snapshot received:
 *   interp.PushSnapshot(net_id, server_time, position, rotation);
 *   // Each render frame:
 *   interp.Interpolate(current_server_time, registry, net2ent_map);
 */
#ifndef DSE_NET_PREDICTION_ENTITY_INTERPOLATION_H
#define DSE_NET_PREDICTION_ENTITY_INTERPOLATION_H

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <entt/entt.hpp>

#include "engine/net/replication/repl_protocol.h"

namespace dse::net {

class EntityInterpolation {
public:
    /// Set the interpolation delay (seconds). Render time = server_time - delay.
    /// Larger values absorb more jitter but increase visual latency.
    void SetInterpolationDelay(float seconds);
    float GetInterpolationDelay() const { return interp_delay_; }

    /// Push a new authoritative snapshot for a remote entity.
    void PushSnapshot(repl::NetId net_id, double server_time,
                      const glm::vec3& position, const glm::quat& rotation);

    /// Interpolate all tracked entities and write results to the registry.
    /// @param server_time  Estimated current server time.
    /// @param registry     ECS registry containing TransformComponents.
    /// @param net2ent      Mapping from NetId to entt::entity.
    void Interpolate(double server_time,
                     entt::registry& registry,
                     const std::unordered_map<repl::NetId, entt::entity>& net2ent);

    /// Stop tracking an entity (e.g. on despawn).
    void RemoveEntity(repl::NetId net_id);

    /// Clear all buffered snapshots.
    void Reset();

    /// Maximum number of snapshots to buffer per entity (oldest are dropped).
    void SetMaxSnapshots(size_t max) { max_snapshots_ = max; }

private:
    struct Snapshot {
        double    time = 0.0;
        glm::vec3 position{0.0f};
        glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    };

    struct EntityBuffer {
        std::vector<Snapshot> snapshots;
    };

    std::unordered_map<repl::NetId, EntityBuffer> buffers_;
    float  interp_delay_  = 0.1f;
    size_t max_snapshots_  = 32;
};

} // namespace dse::net

#endif // DSE_NET_PREDICTION_ENTITY_INTERPOLATION_H
