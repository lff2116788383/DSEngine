/**
 * @file client_prediction.h
 * @brief Client-side prediction with input buffering and server reconciliation.
 *
 * Usage:
 *   ClientPrediction pred;
 *   pred.SetMoveApplyFunc([](entt::registry& r, entt::entity e, const MoveInput& m) { ... });
 *   // Each frame:
 *   pred.PredictMove(registry, entity, input);              // apply locally + buffer
 *   pred.SendPendingMoves(client);                          // send buffered inputs
 *   // On snapshot received:
 *   pred.OnServerSnapshot(seq, entity, server_transform);   // reconcile
 */
#ifndef DSE_NET_PREDICTION_CLIENT_PREDICTION_H
#define DSE_NET_PREDICTION_CLIENT_PREDICTION_H

#include <cstdint>
#include <functional>
#include <vector>

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <entt/entt.hpp>

#include "engine/net/replication/repl_protocol.h"

namespace dse::net::repl { class ReplicationClient; }

namespace dse::net {

struct MoveInput {
    float dx = 0.0f;
    float dy = 0.0f;
    float dz = 0.0f;
    float dt = 0.0f;
};

struct PredictedState {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

class ClientPrediction {
public:
    using MoveApplyFunc = std::function<void(entt::registry&, entt::entity, const MoveInput&)>;

    void SetMoveApplyFunc(MoveApplyFunc func);

    /// Record + locally apply an input. Returns the sequence number assigned.
    uint32_t PredictMove(entt::registry& registry, entt::entity entity, const MoveInput& input);

    /// Send all un-acknowledged moves to the server via ReplicationClient.
    void SendPendingMoves(repl::ReplicationClient& client, repl::NetId net_id);

    /// Called when the server's authoritative snapshot arrives.
    /// Reconciles predicted state: drops acknowledged inputs, replays remaining.
    void OnServerSnapshot(uint32_t server_seq,
                          entt::registry& registry,
                          entt::entity entity,
                          const glm::vec3& server_position,
                          const glm::quat& server_rotation);

    /// Discard all buffered inputs (e.g. on disconnect).
    void Reset();

    uint32_t PendingInputCount() const { return static_cast<uint32_t>(pending_inputs_.size()); }

    /// Tolerance for position mismatch before full correction (squared distance).
    void SetCorrectionThresholdSq(float threshold_sq) { correction_threshold_sq_ = threshold_sq; }

private:
    struct BufferedInput {
        uint32_t  seq = 0;
        MoveInput input;
        PredictedState state_after;
    };

    MoveApplyFunc apply_func_;
    std::vector<BufferedInput> pending_inputs_;
    uint32_t next_seq_ = 1;
    uint32_t last_acked_seq_ = 0;
    float correction_threshold_sq_ = 0.0001f;
};

} // namespace dse::net

#endif // DSE_NET_PREDICTION_CLIENT_PREDICTION_H
