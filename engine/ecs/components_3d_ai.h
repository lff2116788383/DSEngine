#ifndef DSE_COMPONENTS_3D_AI_H
#define DSE_COMPONENTS_3D_AI_H

#include <vector>
#include <glm/glm.hpp>

namespace dse {

struct SteeringComponent {
    bool enabled = true;
    
    // Steering behaviors
    bool seek_enabled = false;
    glm::vec3 seek_target = glm::vec3(0.0f);
    
    bool flee_enabled = false;
    glm::vec3 flee_target = glm::vec3(0.0f);
    
    bool arrive_enabled = false;
    glm::vec3 arrive_target = glm::vec3(0.0f);
    float arrive_deceleration_radius = 5.0f;
    
    // Physical properties
    float max_velocity = 5.0f;
    float max_force = 10.0f;
    float mass = 1.0f;
    
    // Current state
    glm::vec3 velocity = glm::vec3(0.0f);
};

#ifdef DSE_ENABLE_NAVMESH
/// NavMesh 寻路 Agent 组件
struct NavMeshAgentComponent {
    float speed           = 3.5f;    ///< 移动速度（单位/秒）
    float acceleration    = 8.0f;    ///< 加速度
    float stopping_dist   = 0.1f;    ///< 到达目标的停止距离
    float agent_radius    = 0.6f;    ///< Agent 半径（用于 navmesh 查询）
    float agent_height    = 2.0f;    ///< Agent 高度
    glm::vec3 destination = glm::vec3(0.0f); ///< 目标位置
    bool  has_path        = false;   ///< 当前是否持有有效路径
    bool  path_pending    = false;   ///< 需要重新计算路径
    bool  arrived         = true;    ///< 已到达目标
    // 运行时路径数据（由 NavAgentSystem 填充）
    std::vector<glm::vec3> path_points;
    int   current_waypoint = 0;
};
#endif

} // namespace dse

#endif // DSE_COMPONENTS_3D_AI_H
