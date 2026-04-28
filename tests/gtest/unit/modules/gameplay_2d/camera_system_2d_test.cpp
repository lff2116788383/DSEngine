/**
 * @file camera_system_2d_test.cpp
 * @brief CameraSystem (2D) 摄像机系统的单元测试
 *
 * 覆盖场景：
 * - Update 调用不崩溃（空 World）
 * - 带摄像机组件实体的 Update
 */

#include <gtest/gtest.h>
#include "modules/gameplay_2d/camera/camera_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/camera.h"

TEST(CameraSystem2DTest, 空World调用Update不崩溃) {
    World world;
    CameraSystem sys;
    sys.Update(world, 16.0f / 9.0f);
}

TEST(CameraSystem2DTest, 带CameraComponent实体Update不崩溃) {
    World world;
    CameraSystem sys;
    auto e = world.CreateEntity();
    auto& reg = world.registry();
    auto& cam = reg.emplace<CameraComponent>(e);
    cam.orthographic = true;
    cam.orthographic_size = 5.0f;

    sys.Update(world, 16.0f / 9.0f);
}
