#ifndef DSE_ROPE_SYSTEM_H
#define DSE_ROPE_SYSTEM_H

#include "engine/ecs/world.h"

namespace dse {
struct RopeComponent;

namespace gameplay3d {

/**
 * @class RopeSystem
 * @brief Verlet 积分绳索/链条模拟系统
 *
 * 每帧模拟流程：
 *   1. 初始化：沿起始方向生成 segment_count+1 个粒子
 *   2. Verlet 积分更新位置（含重力）
 *   3. 迭代求解距离约束
 *   4. 处理锚点约束（跟随实体位置）
 *   5. 简单地面碰撞
 */
class RopeSystem {
public:
    RopeSystem() = default;
    ~RopeSystem() = default;

    void FixedUpdate(World& world, float dt);

private:
    void InitializeRope(World& world, entt::entity entity, RopeComponent& rope);
    void Simulate(World& world, entt::entity entity, RopeComponent& rope, float dt);
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_ROPE_SYSTEM_H
