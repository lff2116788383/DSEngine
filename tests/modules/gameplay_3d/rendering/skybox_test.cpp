#include "catch/catch.hpp"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"

using namespace dse;

// 正向测试：验证 SkyboxComponent 的默认初始状态
TEST_CASE("Given_DefaultSkyboxComponent_When_Created_Then_HandleIsZero", "[engine][unit][skybox]") {
    SkyboxComponent skybox;
    
    REQUIRE(skybox.enabled == true);
    REQUIRE(skybox.cubemap_handle == 0);
    REQUIRE(skybox.cubemap_path.empty() == true);
}

// 边界测试：赋予 SkyboxComponent 极端的句柄和路径
TEST_CASE("Given_SkyboxComponent_When_AssignedValues_Then_ValuesAreStored", "[engine][unit][skybox]") {
    SkyboxComponent skybox;
    
    skybox.cubemap_handle = 999999;
    skybox.cubemap_path = "assets/textures/skybox/daylight.dds";
    
    REQUIRE(skybox.cubemap_handle == 999999);
    REQUIRE(skybox.cubemap_path == "assets/textures/skybox/daylight.dds");
}

// 反向测试：禁用天空盒
TEST_CASE("Given_SkyboxComponent_When_Disabled_Then_EnabledIsFalse", "[engine][unit][skybox]") {
    SkyboxComponent skybox;
    
    skybox.enabled = false;
    REQUIRE(skybox.enabled == false);
}
