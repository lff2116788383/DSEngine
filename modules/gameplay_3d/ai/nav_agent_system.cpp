#ifdef DSE_ENABLE_NAVMESH

#include "modules/gameplay_3d/ai/nav_agent_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/navigation/nav_mesh_system.h"
#include "engine/core/service_locator.h"
#include <glm/glm.hpp>

namespace dse::gameplay3d {

void NavAgentSystem::Update(World& world, float delta_time) {
    auto* nav = core::ServiceLocator::Instance().Get<navigation::NavMeshSystem>();
    if (!nav || !nav->IsReady()) return;

    auto& registry = world.registry();
    auto view = registry.view<NavMeshAgentComponent, TransformComponent>();

    for (auto entity : view) {
        auto& agent = view.get<NavMeshAgentComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        // 重新计算路径
        if (agent.path_pending) {
            agent.path_pending = false;
            agent.path_points.clear();
            agent.current_waypoint = 0;
            agent.arrived = false;
            agent.has_path = nav->FindPath(
                transform.position, agent.destination, agent.path_points);
        }

        // 如果没有路径或已到达，跳过
        if (!agent.has_path || agent.arrived) continue;
        if (agent.current_waypoint >= (int)agent.path_points.size()) {
            agent.arrived = true;
            agent.has_path = false;
            continue;
        }

        // 向当前路径点移动
        glm::vec3 target = agent.path_points[agent.current_waypoint];
        glm::vec3 dir = target - transform.position;
        // 忽略 Y 轴距离做水平判断（可选：如果需要 3D 判断则使用 length(dir)）
        float dist_xz = glm::length(glm::vec2(dir.x, dir.z));

        if (dist_xz <= agent.stopping_dist) {
            // 到达当前路径点，前进到下一个
            agent.current_waypoint++;
            if (agent.current_waypoint >= (int)agent.path_points.size()) {
                agent.arrived = true;
                agent.has_path = false;
            }
            continue;
        }

        // 水平方向标准化，但保持 Y（让地形适配）
        float full_dist = glm::length(dir);
        if (full_dist < 1e-5f) {
            agent.current_waypoint++;
            continue;
        }
        glm::vec3 move_dir = dir / full_dist;
        float move_dist = agent.speed * delta_time;
        if (move_dist > full_dist) move_dist = full_dist;

        transform.position += move_dir * move_dist;
        // Y 分量使用路径点高度（navmesh 投影）
        float t = move_dist / full_dist;
        transform.position.y = glm::mix(transform.position.y, target.y, t);
    }
}

} // namespace dse::gameplay3d

#endif // DSE_ENABLE_NAVMESH
