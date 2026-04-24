/**
 * @file camera.h
 * @brief 摄像机与跟随组件
 */

#ifndef DSE_ECS_COMPONENTS_2D_CAMERA_H
#define DSE_ECS_COMPONENTS_2D_CAMERA_H

#include <glm/glm.hpp>
#include <entt/entt.hpp>

using Entity = entt::entity;

/**
 * @struct CameraComponent
 * @brief 摄像机组件，提供投影和视图矩阵的计算参数
 */
struct CameraComponent {
    bool orthographic = true;            ///< 是否为正交投影
    bool enabled = true;
    int priority = 0;
    float orthographic_size = 5.0f;      ///< 正交模式下摄像机垂直视野的一半大小
    float fov = 60.0f;                   ///< 透视投影的视场角 (度)
    float aspect_ratio = 1.333f;         ///< 透视投影的宽高比 (width / height)
    float near_clip = -1.0f;             ///< 近裁剪面
    float far_clip = 1.0f;               ///< 远裁剪面
    glm::mat4 view = glm::mat4(1.0f);    ///< 缓存的视图矩阵
    glm::mat4 projection = glm::mat4(1.0f);///< 缓存的投影矩阵
};

/**
 * @struct CameraFollowComponent
 * @brief 摄像机跟随组件，使实体平滑追踪目标
 */
struct CameraFollowComponent {
    Entity target = entt::null;                      ///< 追踪的目标实体
    glm::vec3 offset = glm::vec3(0.0f, 0.0f, 0.0f);  ///< 跟随目标的相对偏移
    glm::vec2 dead_zone = glm::vec2(0.0f, 0.0f);     ///< 死区，在此区域内摄像机不移动
    float damping = 0.12f;                           ///< 缓动阻尼系数 (0 瞬间到达, 值越大越平滑)
    bool follow_x = true;                            ///< 是否在 X 轴上追踪
    bool follow_y = true;                            ///< 是否在 Y 轴上追踪
    bool enabled = true;                             ///< 是否激活
};

#endif // DSE_ECS_COMPONENTS_2D_CAMERA_H
