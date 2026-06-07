/**
 * @file physics_2d.h
 * @brief 2D 物理组件，封装刚体与碰撞体
 */

#ifndef DSE_ECS_COMPONENTS_2D_PHYSICS_2D_H
#define DSE_ECS_COMPONENTS_2D_PHYSICS_2D_H

#include <glm/glm.hpp>
#include <deque>
#include <functional>
#include <vector>
#include <entt/entt.hpp>

using Entity = entt::entity;

struct Physics2DContactEvent {
    Entity other = entt::null;           ///< 另一方实体
    bool is_trigger = false;             ///< 是否来自触发器
    bool is_enter = true;                ///< true=进入，false=离开
};

// Box2D 前向声明
class b2Body;
class b2Fixture;
class b2Joint;

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
    
    /// 增量同步标记：仅当 dirty 时才同步 ECS → Box2D
    bool sync_dirty_ = true;

    // Internal Box2D body handle
    b2Body* runtime_body = nullptr;                         ///< 运行时绑定的 Box2D 刚体实例
    
    // Callbacks for collision events
    std::function<void(Entity other)> on_collision_enter;///< 物理碰撞进入回调
    std::function<void(Entity other)> on_collision_exit; ///< 物理碰撞离开回调
    std::function<void(Entity other)> on_trigger_enter;  ///< 触发器进入回调
    std::function<void(Entity other)> on_trigger_exit;   ///< 触发器离开回调
    std::deque<Physics2DContactEvent> pending_contact_events;  ///< 运行时接触事件队列，供脚本/工具轮询，不应序列化
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

/**
 * @struct CircleCollider2DComponent
 * @brief 2D 圆形碰撞体组件，定义物理形状和材质属性
 */
struct CircleCollider2DComponent {
    float radius = 0.5f;                                ///< 碰撞体半径
    glm::vec2 offset = glm::vec2(0.0f, 0.0f);            ///< 相对实体的偏移
    float density = 1.0f;                                ///< 密度 (影响质量)
    float friction = 0.3f;                               ///< 摩擦系数
    float restitution = 0.0f;                            ///< 恢复系数 (弹性)
    bool is_trigger = false;                             ///< 是否为触发器 (仅检测不产生物理力)

    // Internal Box2D runtime fixture pointer
    b2Fixture* runtime_fixture = nullptr;                ///< 运行时绑定的 Box2D 夹具实例
};

/**
 * @struct PolygonCollider2DComponent
 * @brief 2D 凸多边形碰撞体组件（最多 8 顶点，Box2D 限制）
 */
struct PolygonCollider2DComponent {
    std::vector<glm::vec2> vertices;                     ///< 凸多边形顶点（局部坐标，逆时针）
    glm::vec2 offset = glm::vec2(0.0f, 0.0f);            ///< 相对实体的偏移
    float density = 1.0f;                                ///< 密度
    float friction = 0.3f;                               ///< 摩擦系数
    float restitution = 0.0f;                            ///< 恢复系数
    bool is_trigger = false;                             ///< 是否为触发器

    b2Fixture* runtime_fixture = nullptr;                ///< 运行时绑定的 Box2D 夹具实例
};

/**
 * @enum Joint2DType
 * @brief 2D 关节类型
 */
enum class Joint2DType {
    Revolute,   ///< 铰链关节（hinge/pin），支持角度限制和马达
    Distance,   ///< 距离关节（弹簧/绳索）
    Prismatic,  ///< 棱柱关节（滑块），支持位移限制和马达
    Weld,       ///< 焊接关节（刚性连接）
};

/**
 * @struct Joint2DComponent
 * @brief 2D 关节组件，将两个带刚体的实体通过 Box2D 约束连接起来
 *
 * 使用方式：
 *   1. 设置 entity_a / entity_b（两者都需要 RigidBody2DComponent）
 *   2. 设置 type 和对应参数
 *   3. 物理系统在下一次 FixedUpdate 时自动创建 Box2D 关节
 */
struct Joint2DComponent {
    Joint2DType type = Joint2DType::Revolute;
    Entity entity_a = entt::null;           ///< 关节端 A（通常为此 Component 所在实体）
    Entity entity_b = entt::null;           ///< 关节端 B
    glm::vec2 anchor_a = {0.0f, 0.0f};     ///< 刚体 A 局部坐标系锚点
    glm::vec2 anchor_b = {0.0f, 0.0f};     ///< 刚体 B 局部坐标系锚点
    bool collide_connected = false;         ///< 连接的刚体是否互相碰撞

    // --- 铰链 (Revolute) 专属 ---
    bool enable_limit = false;
    float lower_angle = 0.0f;              ///< 铰链下限角度（度）
    float upper_angle = 0.0f;              ///< 铰链上限角度（度）
    bool enable_motor = false;
    float motor_speed = 0.0f;              ///< 马达目标角速度（度/秒）
    float max_motor_torque = 0.0f;         ///< 马达最大扭矩

    // --- 距离 (Distance) 专属 ---
    float min_length = 0.0f;              ///< 距离关节最小长度
    float max_length = 1.0f;              ///< 距离关节最大长度
    float stiffness = 0.0f;               ///< 弹簧刚度（0=刚性，>0=弹性）
    float damping = 0.0f;                 ///< 弹簧阻尼

    // --- 棱柱 (Prismatic) 专属 ---
    glm::vec2 prismatic_axis = {1.0f, 0.0f}; ///< 滑动轴方向（世界坐标，会被归一化）
    float lower_translation = 0.0f;        ///< 棱柱下限位移
    float upper_translation = 0.0f;        ///< 棱柱上限位移
    float prismatic_motor_speed = 0.0f;    ///< 棱柱马达速度
    float max_motor_force = 0.0f;          ///< 棱柱马达最大力

    // 运行时句柄（不应序列化，由 Physics2DSystem 管理）
    b2Joint* runtime_joint = nullptr;
};

#endif // DSE_ECS_COMPONENTS_2D_PHYSICS_2D_H
