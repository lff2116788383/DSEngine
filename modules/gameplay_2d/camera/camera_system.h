/**
 * @file camera_system.h
 * @brief 摄像机系统，管理视图矩阵、投影矩阵和屏幕视口映射
 */

#ifndef DSE_CAMERA_SYSTEM_H
#define DSE_CAMERA_SYSTEM_H

#include "engine/ecs/world.h"

/**
 * @class CameraSystem
 * @brief 摄像机管理系统，根据摄像机组件的属性（正交/透视、视野大小）计算投影矩阵
 */
class CameraSystem {
public:
    /**
     * @brief 执行摄像机矩阵的更新
     * @param world 包含摄像机组件的实体世界
     * @param aspect_ratio 当前屏幕的宽高比
     * @example
     * // camera_system.Update(world, Screen::width() / (float)Screen::height());
     */
    void Update(World& world, float aspect_ratio);
};

#endif
