/**
 * @file transform.h
 * @brief 变换与层级组件，定义实体在世界中的空间关系
 */

#ifndef DSE_ECS_COMPONENTS_2D_TRANSFORM_H
#define DSE_ECS_COMPONENTS_2D_TRANSFORM_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <entt/entt.hpp>

using Entity = entt::entity;

/**
 * @struct TransformComponent
 * @brief 空间变换组件，定义实体在世界中的位置、旋转和缩放
 */
struct TransformComponent {
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);    ///< 本地坐标
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); ///< 本地旋转(四元数)
    glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);       ///< 本地缩放
    glm::mat4 local_to_world = glm::mat4(1.0f);          ///< 缓存的模型矩阵(世界坐标)
    bool dirty = true;                                   ///< 标记是否需要重新计算模型矩阵
};

/**
 * @struct ParentComponent
 * @brief 场景层级组件，指明当前实体的父节点
 */
struct ParentComponent {
    Entity parent = entt::null; ///< 父实体的 ID
};

#endif // DSE_ECS_COMPONENTS_2D_TRANSFORM_H
