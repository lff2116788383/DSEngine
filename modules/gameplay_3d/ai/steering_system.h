#ifndef DSE_STEERING_SYSTEM_H
#define DSE_STEERING_SYSTEM_H

#include "engine/ecs/world.h"

namespace dse::gameplay3d {

/**
 * @class SteeringSystem
 * @brief 从 VSEngine2.1 的 VSSteer 提取的转向行为算法系统。
 * 负责计算 Seek, Flee, Arrive 等行为，更新实体的 TransformComponent。
 */
class SteeringSystem {
public:
    SteeringSystem() = default;
    ~SteeringSystem() = default;

    /**
     * @brief 执行转向行为更新
     * @param world 当前的世界对象
     * @param delta_time 帧时间
     */
    void Update(World& world, float delta_time);
};

} // namespace dse::gameplay3d

#endif // DSE_STEERING_SYSTEM_H