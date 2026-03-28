#include "catch/catch.hpp"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"

using namespace dse;

// 正向测试：验证 3D 相机组件默认参数是否符合透视投影需求
TEST_CASE("Given_DefaultCamera3DComponent_When_Created_Then_ProjectionParametersAreValid", "[engine][unit][camera_3d]") {
    Camera3DComponent camera;
    
    REQUIRE(camera.enabled == true);
    REQUIRE(camera.fov == 60.0f);
    REQUIRE(camera.aspect_ratio == 1.333f);
    REQUIRE(camera.near_clip == 0.1f);
    REQUIRE(camera.far_clip == 1000.0f);
    
    // 验证矩阵被初始化为单位矩阵
    REQUIRE(camera.view == glm::mat4(1.0f));
    REQUIRE(camera.projection == glm::mat4(1.0f));
}

// 边界测试：修改相机的极端视野(FOV)与剪裁面
TEST_CASE("Given_Camera3DComponent_When_SettingExtremeFOVAndClip_Then_ValuesAreUpdated", "[engine][unit][camera_3d]") {
    Camera3DComponent camera;
    
    camera.fov = 179.0f; // 极广角
    camera.near_clip = 0.0001f;
    camera.far_clip = 100000.0f;
    
    REQUIRE(camera.fov == 179.0f);
    REQUIRE(camera.near_clip == 0.0001f);
    REQUIRE(camera.far_clip == 100000.0f);
}

// 正向测试：验证自由相机控制器默认参数
TEST_CASE("Given_DefaultFreeCameraController_When_Created_Then_MovementParamsAreValid", "[engine][unit][camera_3d]") {
    FreeCameraControllerComponent controller;
    
    REQUIRE(controller.enabled == true);
    REQUIRE(controller.move_speed == 5.0f);
    REQUIRE(controller.mouse_sensitivity == 0.1f);
    REQUIRE(controller.pitch == 0.0f);
    REQUIRE(controller.yaw == -90.0f); // 默认朝向 -Z
}

// 反向测试：处理异常的自由相机旋转参数（逻辑上应在System限制，但组件层保持数据）
TEST_CASE("Given_FreeCameraController_When_ExtremePitchAssigned_Then_ValuesAreMaintained", "[engine][unit][camera_3d]") {
    FreeCameraControllerComponent controller;
    
    controller.pitch = 150.0f; // 超过 90 度的俯仰角
    REQUIRE(controller.pitch == 150.0f);
}
