/**
 * @file camera_controller_system.h
 * @brief 2D 相机控制器系统 (震动、边界、平滑缩放、前瞻)
 */

#ifndef DSE_CAMERA_CONTROLLER_SYSTEM_H
#define DSE_CAMERA_CONTROLLER_SYSTEM_H

#include "engine/ecs/world.h"

class CameraControllerSystem {
public:
    void Update(World& world, float delta_time);
};

#endif // DSE_CAMERA_CONTROLLER_SYSTEM_H
