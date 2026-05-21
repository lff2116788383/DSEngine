#ifndef DSE_VEHICLE_SYSTEM_H
#define DSE_VEHICLE_SYSTEM_H

#include "engine/ecs/world.h"

namespace dse {
struct VehicleComponent;

namespace physics3d { class IPhysics3DSystem; }

namespace gameplay3d {

/**
 * @class VehicleSystem
 * @brief Raycast 车辆物理系统（不依赖 PhysXVehicle SDK）
 *
 * 每帧模拟流程：
 *   1. 从 VehicleComponent 读取油门/刹车/转向输入
 *   2. 对每个车轮发射 Raycast 检测地面
 *   3. 计算悬挂弹簧力
 *   4. 计算驱动力、刹车力
 *   5. 通过 RigidBody3D 的 AddForce 施加到车体
 *   6. 更新车轮旋转/压缩等视觉状态
 */
class VehicleSystem {
public:
    VehicleSystem() = default;
    ~VehicleSystem() = default;

    void SetPhysics3D(physics3d::IPhysics3DSystem* physics3d);

    void FixedUpdate(World& world, float dt);

private:
    physics3d::IPhysics3DSystem* physics3d_ = nullptr;

    void InitializeVehicle(World& world, entt::entity entity, VehicleComponent& vehicle);
    void SimulateVehicle(World& world, entt::entity entity, VehicleComponent& vehicle, float dt);
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_VEHICLE_SYSTEM_H
