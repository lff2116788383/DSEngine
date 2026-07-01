/**
 * @file camera_controller_2d.h
 * @brief 2D 相机控制器组件 (屏幕震动、边界限制、平滑缩放)
 */

#ifndef DSE_ECS_CAMERA_CONTROLLER_2D_H
#define DSE_ECS_CAMERA_CONTROLLER_2D_H

#include <glm/glm.hpp>
#include <entt/entt.hpp>

using Entity = entt::entity;

/**
 * @struct ScreenShake2D
 * @brief 屏幕震动参数
 */
struct ScreenShake2D {
    float trauma = 0.0f;          ///< 当前震动强度 [0,1], 自动衰减
    float decay_rate = 1.5f;      ///< 每秒衰减量
    float max_offset = 0.5f;      ///< 最大平移偏移 (世界单位)
    float max_rotation = 5.0f;    ///< 最大旋转角度 (度)
    float frequency = 15.0f;      ///< 震动频率 (Hz)
    float time_acc = 0.0f;        ///< 内部时间累加器
};

/**
 * @struct CameraBounds2D
 * @brief 相机活动范围限制
 */
struct CameraBounds2D {
    bool enabled = false;
    float min_x = -100.0f;
    float max_x = 100.0f;
    float min_y = -100.0f;
    float max_y = 100.0f;
};

/**
 * @struct CameraController2DComponent
 * @brief 2D 相机控制器，提供高级相机行为
 */
struct CameraController2DComponent {
    // 平滑跟随 (增强 CameraFollowComponent)
    float look_ahead_x = 0.0f;       ///< 前瞻距离 X (朝运动方向偏移)
    float look_ahead_y = 0.0f;       ///< 前瞻距离 Y
    float look_ahead_speed = 2.0f;   ///< 前瞻追赶速度

    // 平滑缩放
    float target_zoom = 1.0f;        ///< 目标缩放倍数
    float zoom_speed = 3.0f;         ///< 缩放插值速度
    float min_zoom = 0.2f;           ///< 最小缩放
    float max_zoom = 5.0f;           ///< 最大缩放

    // 屏幕震动
    ScreenShake2D shake;

    // 边界限制
    CameraBounds2D bounds;

    // 内部状态
    glm::vec2 look_ahead_current = {0.0f, 0.0f};
    glm::vec2 shake_offset = {0.0f, 0.0f};
    float shake_rotation = 0.0f;
    bool enabled = true;
};

#endif // DSE_ECS_CAMERA_CONTROLLER_2D_H
