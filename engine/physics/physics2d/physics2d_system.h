/**
 * @file physics2d_system.h
 * @brief 2D物理系统，封装 Box2D 物理引擎，处理碰撞检测、刚体动力学模拟和射线检测
 */

#ifndef DSE_PHYSICS2D_SYSTEM_H
#define DSE_PHYSICS2D_SYSTEM_H

#include "engine/ecs/world.h"
#include <box2d/box2d.h>
#include <entt/entt.hpp>
#include <memory>
#include <set>
#include <tuple>
#include <vector>


/**
 * @class Physics2DSystem
 * @brief 2D 物理引擎系统，负责同步 ECS 组件和 Box2D 刚体状态，并执行固定步长的物理模拟
 */
class Physics2DSystem {
public:
    Physics2DSystem();
    ~Physics2DSystem();

    /**
     * @brief 初始化物理世界，绑定接触监听器并创建初始刚体
     * @param world 包含物理组件的实体世界
     * @example
     * // physics_system.Init(world);
     */
    void Init(World& world);

    /**
     * @brief 显式释放 2D 物理世界与监听器，清空运行时状态
     */
    void Shutdown();
    
    /**
     * @brief 固定步长更新物理模拟，并将物理运算结果同步回实体的 Transform 组件
     * @param world 包含物理组件的实体世界
     * @param fixed_delta_time 物理步长(如 1/60 秒)
     */
    void FixedUpdate(World& world, float fixed_delta_time);
    
    /**
     * @brief 执行 2D 射线投射检测
     * @param start 射线起点(世界坐标)
     * @param end 射线终点(世界坐标)
     * @param out_entity 输出被击中的实体 ID
     * @param out_point 输出射线击中的确切世界坐标点
     * @param out_normal 输出击中表面的法线向量
     * @return 如果射线击中了任何带有碰撞体的实体，则返回 true
     * @example
     * // bool hit = physics.Raycast(start, end, hit_ent, hit_point, hit_normal);
     */
    bool Raycast(const glm::vec2& start, const glm::vec2& end, Entity& out_entity, glm::vec2& out_point, glm::vec2& out_normal);

    /**
     * @brief 销毁指定实体上 Joint2DComponent 对应的 Box2D 关节
     * @param world 实体世界
     * @param entity 拥有 Joint2DComponent 的实体
     */
    void DestroyJoint(World& world, Entity entity);

    /**
     * @brief 运行时设置铰链关节马达角速度（度/秒）
     */
    void SetRevoluteMotorSpeed(World& world, Entity joint_entity, float speed);

    /**
     * @brief 运行时设置铰链关节马达最大扭矩
     */
    void SetRevoluteMotorTorque(World& world, Entity joint_entity, float max_torque);

    /**
     * @brief 运行时设置棱柱关节马达线速度
     */
    void SetPrismaticMotorSpeed(World& world, Entity joint_entity, float speed);

    /**
     * @brief 运行时设置棱柱关节马达最大力
     */
    void SetPrismaticMotorForce(World& world, Entity joint_entity, float max_force);

    /// ECS 实体销毁前由 World 或系统调用，清理 Box2D body/joint
    void DestroyPhysicsForEntity(World& world, Entity entity);

private:
    using ContactPair = std::tuple<Entity, Entity, bool>;

    void OnRigidBody2DDestroyed(entt::registry& reg, entt::entity entity);
    void OnJoint2DDestroyed(entt::registry& reg, entt::entity entity);

    b2World* physics_world_ = nullptr;
    std::vector<entt::connection> destroy_connections_;

    std::set<ContactPair> active_contact_pairs_;
    int velocity_iterations_ = 8;
    int position_iterations_ = 3;
};

#endif
