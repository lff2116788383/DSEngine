#include "catch/catch.hpp"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"

using namespace dse;

// 正向测试：CSM默认的层级配置应为3，并有合理的距离。
TEST_CASE("Given_DefaultDirectionalLight_When_Created_Then_CSMParametersAreValid", "[engine][unit][csm]") {
    DirectionalLight3DComponent light;
    REQUIRE(light.enabled == true);
    REQUIRE(light.cast_shadow == true);
    
    // 检查默认的三级 CSM 分割
    REQUIRE(CSM_CASCADES == 3);
    REQUIRE(light.cascade_splits[0] == 20.0f);
    REQUIRE(light.cascade_splits[1] == 60.0f);
    REQUIRE(light.cascade_splits[2] == 200.0f);
}

// 边界测试：修改 CSM 级联分割距离。
TEST_CASE("Given_DirectionalLight_When_ModifyingSplits_Then_ValuesAreUpdated", "[engine][unit][csm]") {
    DirectionalLight3DComponent light;
    
    light.cascade_splits[0] = 50.0f;
    light.cascade_splits[1] = 150.0f;
    light.cascade_splits[2] = 500.0f;
    
    REQUIRE(light.cascade_splits[0] == 50.0f);
    REQUIRE(light.cascade_splits[2] == 500.0f);
}

// 反向测试：处理极端的光照强度。
TEST_CASE("Given_DirectionalLight_When_ExtremeIntensity_Then_Maintained", "[engine][unit][csm]") {
    DirectionalLight3DComponent light;
    
    light.intensity = -1.0f; // 某些逻辑可能需要负光照或允许负值，此处仅测试数据保持
    REQUIRE(light.intensity == -1.0f);
    
    light.shadow_strength = 2.0f;
    REQUIRE(light.shadow_strength == 2.0f);
}
