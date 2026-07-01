#include "engine/net/prediction/entity_interpolation.h"

#include "engine/ecs/transform.h"

#include <glm/glm.hpp>
#include <algorithm>

namespace dse::net {

void EntityInterpolation::SetInterpolationDelay(float seconds) {
    interp_delay_ = seconds > 0.0f ? seconds : 0.0f;
}

void EntityInterpolation::PushSnapshot(repl::NetId net_id, double server_time,
                                       const glm::vec3& position, const glm::quat& rotation) {
    auto& buf = buffers_[net_id];
    Snapshot snap;
    snap.time = server_time;
    snap.position = position;
    snap.rotation = rotation;
    buf.snapshots.push_back(snap);

    // Keep buffer bounded.
    if (buf.snapshots.size() > max_snapshots_) {
        buf.snapshots.erase(buf.snapshots.begin(),
                            buf.snapshots.begin() +
                                static_cast<ptrdiff_t>(buf.snapshots.size() - max_snapshots_));
    }
}

void EntityInterpolation::Interpolate(double server_time,
                                      entt::registry& registry,
                                      const std::unordered_map<repl::NetId, entt::entity>& net2ent) {
    const double render_time = server_time - static_cast<double>(interp_delay_);

    for (auto& [net_id, buf] : buffers_) {
        if (buf.snapshots.size() < 2) continue;

        auto ent_it = net2ent.find(net_id);
        if (ent_it == net2ent.end()) continue;
        auto* t = registry.try_get<TransformComponent>(ent_it->second);
        if (!t) continue;

        // Find the two snapshots bracketing render_time.
        const Snapshot* from = nullptr;
        const Snapshot* to = nullptr;

        for (size_t i = 0; i + 1 < buf.snapshots.size(); ++i) {
            if (buf.snapshots[i].time <= render_time &&
                buf.snapshots[i + 1].time >= render_time) {
                from = &buf.snapshots[i];
                to = &buf.snapshots[i + 1];
                break;
            }
        }

        if (!from || !to) {
            // Render time is beyond the buffer — extrapolate from the last two snapshots
            // or snap to the latest if we only have old data.
            if (render_time > buf.snapshots.back().time) {
                // Use the two most recent for linear extrapolation (clamped).
                from = &buf.snapshots[buf.snapshots.size() - 2];
                to = &buf.snapshots[buf.snapshots.size() - 1];
            } else {
                // Render time is before all snapshots — snap to earliest.
                t->position = buf.snapshots.front().position;
                t->rotation = buf.snapshots.front().rotation;
                t->dirty = true;
                continue;
            }
        }

        const double span = to->time - from->time;
        float alpha = (span > 1e-9) ? static_cast<float>((render_time - from->time) / span) : 1.0f;
        // Clamp to [0, 1.5] to allow mild extrapolation but prevent runaway.
        alpha = glm::clamp(alpha, 0.0f, 1.5f);

        t->position = glm::mix(from->position, to->position, alpha);
        t->rotation = glm::slerp(from->rotation, to->rotation, alpha);
        t->dirty = true;

        // Prune snapshots that are too old (well before render_time).
        while (buf.snapshots.size() > 2 && buf.snapshots[1].time < render_time) {
            buf.snapshots.erase(buf.snapshots.begin());
        }
    }
}

void EntityInterpolation::RemoveEntity(repl::NetId net_id) {
    buffers_.erase(net_id);
}

void EntityInterpolation::Reset() {
    buffers_.clear();
}

} // namespace dse::net
