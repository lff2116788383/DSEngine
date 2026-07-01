/**
 * @file repl_aoi.h
 * @brief AOI（Area of Interest）兴趣管理 — 按距离裁剪每个连接能看到的实体子集。
 *
 * 策略：
 *   - Always：实体对所有连接可见（全局 NPC、环境物）
 *   - Distance：以实体 owner 的位置为视点，半径 R 内的实体可见
 *
 * 用法：
 *   AoiManager aoi;
 *   aoi.SetPolicy(AoiPolicy::Distance, 200.0f);
 *   aoi.Update(registry, net2ent, owner_positions);
 *   for (auto id : aoi.GetRelevantSet(conn)) { ... }
 */
#ifndef DSE_NET_REPLICATION_REPL_AOI_H
#define DSE_NET_REPLICATION_REPL_AOI_H

#include <cstdint>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <entt/entt.hpp>

#include "engine/ecs/transform.h"
#include "engine/net/net_types.h"
#include "engine/net/replication/repl_protocol.h"

namespace dse::net::repl {

enum class AoiPolicy : uint8_t {
    Always   = 0,  ///< 所有实体对所有连接可见（默认，等同无 AOI）
    Distance = 1,  ///< 距离裁剪：半径 R 内可见
};

class AoiManager {
public:
    void SetPolicy(AoiPolicy policy, float radius = 0.0f) {
        policy_ = policy;
        radius_ = radius;
        radius_sq_ = radius * radius;
    }

    AoiPolicy GetPolicy() const { return policy_; }
    float     GetRadius() const { return radius_; }

    /// 更新每个连接的可见实体集。
    /// viewpoints: 每个连接的观察点位置（通常是 owner 实体的 position）。
    void Update(
        const entt::registry& world,
        const std::unordered_map<NetId, entt::entity>& net2ent,
        const std::unordered_map<ConnectionId, glm::vec3>& viewpoints
    ) {
        relevance_.clear();

        if (policy_ == AoiPolicy::Always) {
            // 所有实体对所有连接可见
            for (auto& [conn, _] : viewpoints) {
                auto& set = relevance_[conn];
                for (auto& [id, e] : net2ent) {
                    if (world.valid(e)) set.insert(id);
                }
            }
            return;
        }

        // Distance policy
        for (auto& [conn, vp] : viewpoints) {
            auto& set = relevance_[conn];
            for (auto& [id, e] : net2ent) {
                if (!world.valid(e)) continue;
                const auto* t = world.try_get<TransformComponent>(e);
                if (!t) continue;
                float dx = t->position.x - vp.x;
                float dy = t->position.y - vp.y;
                float dz = t->position.z - vp.z;
                float dist_sq = dx * dx + dy * dy + dz * dz;
                if (dist_sq <= radius_sq_) {
                    set.insert(id);
                }
            }
        }
    }

    /// 获取某连接的可见实体集。
    const std::unordered_set<NetId>& GetRelevantSet(ConnectionId conn) const {
        static const std::unordered_set<NetId> empty;
        auto it = relevance_.find(conn);
        return it != relevance_.end() ? it->second : empty;
    }

    /// 检查某实体对某连接是否可见。
    bool IsRelevant(ConnectionId conn, NetId id) const {
        auto it = relevance_.find(conn);
        if (it == relevance_.end()) return false;
        return it->second.count(id) > 0;
    }

private:
    AoiPolicy policy_ = AoiPolicy::Always;
    float radius_    = 0.0f;
    float radius_sq_ = 0.0f;
    std::unordered_map<ConnectionId, std::unordered_set<NetId>> relevance_;
};

} // namespace dse::net::repl

#endif // DSE_NET_REPLICATION_REPL_AOI_H
