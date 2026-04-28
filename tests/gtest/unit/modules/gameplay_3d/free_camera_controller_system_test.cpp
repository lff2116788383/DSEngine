/**
* @file free_camera_controller_system_test.cpp
* @brief 自由相机控制器系统单元测试，验证 FreeCameraControllerSystem::Update 对空/有实体 World 的行为
*/

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"
#include "modules/gameplay_3d/camera/free_camera_controller_system.h"

using namespace dse;
using namespace gameplay3d;

TEST(FreeCameraControllerSystemTest, 空World调用Update不崩溃) {
    World world;
    FreeCameraControllerSystem system;
    EXPECT_NO_THROW(system.Update(world, 0.016f));
}

TEST(FreeCameraControllerSystemTest, 带相机实体Update不崩溃) {
    World world;
    FreeCameraControllerSystem system;
    auto entity = world.CreateEntity();
    world.registry().emplace<TransformComponent>(entity);
    world.registry().emplace<Camera3DComponent>(entity);
    EXPECT_NO_THROW(system.Update(world, 0.016f));
}
