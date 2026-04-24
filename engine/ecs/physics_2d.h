/**
 * @file physics_2d.h
 * @brief 2D 物理组件，封装刚体与碰撞体
 */

#ifndef DSE_ECS_COMPONENTS_2D_PHYSICS_2D_H
#define DSE_ECS_COMPONENTS_2D_PHYSICS_2D_H

#include <glm/glm.hpp>
#include <functional>
#include <entt/entt.hpp>

using Entity = entt::entity;

// Box2D 前向声明
class b2Body;
class b2Fixture;

/**
 * @enum RigidBody2DType
 * @brief 2D 刚体类型，与 Box2D (b2BodyType) 的语义对应
 */
enum class RigidBody2DType {
    Static,    ///< 静态物体，不受力，零质量
    Kinematic, ///< 运动学物体，不受力，但可通过代码控制速度
    Dynamic    ///< 动态物体，完全受物理引擎的力与重力模拟
};

/**
 * @struct RigidBody2DComponent
 * @brief 2D 刚体组件，封装物理状态与速度
 */
struct RigidBody2DComponent {
    RigidBody2DType type = RigidBody2DType::Dynamic;
    glm::vec2 velocity = glm::vec2(0.0f, 0.0f);          ///< 线性速度
    float gravity_scale = 1.0f;                          ///< 重力缩放倍数
    bool fixed_rotation = false;                         ///< 是否锁定旋转
    
    // Internal Box2D body handle
    b2Body* runtime_body = nullptr;                         ///< 运行时绑定的 Box2D 刚体实例
    
    // Callbacks for collision events
    std::function<void(Entity other)> on_collision_enter;///< 物理碰撞进入回调
    std::function<void(Entity other)> on_collision_exit; ///< 物理碰撞离开回调
    std::function<void(Entity other)> on_trigger_enter;  ///< 触发器进入回调
    std::function<void(Entity other)> on_trigger_exit;   ///< 触发器离开回调
};

/**
 * @struct BoxCollider2DComponent
 * @brief 2D 矩形碰撞体组件，定义物理形状和材质属性
 */
struct BoxCollider2DComponent {
    glm::vec2 size = glm::vec2(1.0f, 1.0f);              ///< 碰撞体尺寸
    glm::vec2 offset = glm::vec2(0.0f, 0.0f);            ///< 相对实体的偏移
    float density = 1.0f;                                ///< 密度 (影响质量)
    float friction = 0.3f;                               ///< 摩擦系数
    float restitution = 0.0f;                            ///< 恢复系数 (弹性)
    bool is_trigger = false;                             ///< 是否为触发器 (仅检测不产生物理力)
    
    // Internal Box2D runtime fixture pointer
    b2Fixture* runtime_fixture = nullptr;                ///< 运行时绑定的 Box2D 夹具实例
};

#endif // DSE_ECS_COMPONENTS_2D_PHYSICS_2D_H
