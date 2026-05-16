#ifndef DSE_NAV_AGENT_SYSTEM_H
#define DSE_NAV_AGENT_SYSTEM_H

#ifdef DSE_ENABLE_NAVMESH

#include "engine/ecs/world.h"

namespace dse::gameplay3d {

/**
 * @class NavAgentSystem
 * @brief 处理 NavMeshAgentComponent 的 ECS 系统
 *
 * 职责:
 * - 对 path_pending=true 的 Agent 调用 NavMeshSystem::FindPath
 * - 沿路径点线性移动实体 Transform
 * - 到达目标时标记 arrived=true
 */
class NavAgentSystem {
public:
    NavAgentSystem() = default;
    ~NavAgentSystem() = default;

    void Update(World& world, float delta_time);
};

} // namespace dse::gameplay3d

#endif // DSE_ENABLE_NAVMESH

#endif // DSE_NAV_AGENT_SYSTEM_H
