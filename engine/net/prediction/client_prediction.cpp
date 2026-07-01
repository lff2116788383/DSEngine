#include "engine/net/prediction/client_prediction.h"

#include "engine/ecs/transform.h"
#include "engine/net/replication/replication_client.h"

#include <glm/glm.hpp>
#include <algorithm>

namespace dse::net {

void ClientPrediction::SetMoveApplyFunc(MoveApplyFunc func) {
    apply_func_ = std::move(func);
}

uint32_t ClientPrediction::PredictMove(entt::registry& registry, entt::entity entity,
                                       const MoveInput& input) {
    if (!apply_func_) return 0;

    const uint32_t seq = next_seq_++;

    // Apply the input locally (prediction).
    apply_func_(registry, entity, input);

    // Capture state after application for later reconciliation.
    BufferedInput buf;
    buf.seq = seq;
    buf.input = input;
    if (auto* t = registry.try_get<TransformComponent>(entity)) {
        buf.state_after.position = t->position;
        buf.state_after.rotation = t->rotation;
    }
    pending_inputs_.push_back(buf);

    return seq;
}

void ClientPrediction::SendPendingMoves(repl::ReplicationClient& client, repl::NetId net_id) {
    for (const auto& buf : pending_inputs_) {
        client.SendMove(net_id, buf.input.dx, buf.input.dy, buf.input.dz);
    }
}

void ClientPrediction::OnServerSnapshot(uint32_t server_seq,
                                        entt::registry& registry,
                                        entt::entity entity,
                                        const glm::vec3& server_position,
                                        const glm::quat& server_rotation) {
    // Drop all inputs that the server has already processed.
    pending_inputs_.erase(
        std::remove_if(pending_inputs_.begin(), pending_inputs_.end(),
                       [server_seq](const BufferedInput& b) { return b.seq <= server_seq; }),
        pending_inputs_.end());
    last_acked_seq_ = server_seq;

    auto* t = registry.try_get<TransformComponent>(entity);
    if (!t) return;

    // Check if the server state diverges from our prediction.
    const glm::vec3 delta = t->position - server_position;
    const float dist_sq = glm::dot(delta, delta);

    if (dist_sq > correction_threshold_sq_) {
        // Snap to server state, then replay un-acknowledged inputs.
        t->position = server_position;
        t->rotation = server_rotation;
        t->dirty = true;

        if (apply_func_) {
            for (auto& buf : pending_inputs_) {
                apply_func_(registry, entity, buf.input);
                buf.state_after.position = t->position;
                buf.state_after.rotation = t->rotation;
            }
        }
    }
}

void ClientPrediction::Reset() {
    pending_inputs_.clear();
    next_seq_ = 1;
    last_acked_seq_ = 0;
}

} // namespace dse::net
