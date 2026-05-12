#ifndef DSE_BUOYANCY_SYSTEM_H
#define DSE_BUOYANCY_SYSTEM_H

#include "engine/ecs/world.h"

namespace dse {
struct BuoyancyComponent;

namespace physics3d { class Physics3DSystem; }

namespace gameplay3d {

/**
 * @class BuoyancySystem
 * @brief 浮力模拟系统
 *
 * 每帧模拟流程：
 *   1. 获取水面高度（全局 water_level 或 FluidSystem 水面）
 *   2. 对每个采样点计算淹没深度
 *   3. 计算浮力 = buoyancy_force * submerge_ratio * gravity * dt
 *   4. 计算水阻力（线性 + 角）
 *   5. 通过 Physics3DSystem::AddForce 施加到刚体
 */
class BuoyancySystem {
public:
    BuoyancySystem() = default;
    ~BuoyancySystem() = default;

    void SetPhysics3D(physics3d::Physics3DSystem* physics3d);

    void FixedUpdate(World& world, float dt);

private:
    physics3d::Physics3DSystem* physics3d_ = nullptr;

    float GetWaterLevel(World& world, const glm::vec3& pos, const BuoyancyComponent& buoyancy) const;
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_BUOYANCY_SYSTEM_H
