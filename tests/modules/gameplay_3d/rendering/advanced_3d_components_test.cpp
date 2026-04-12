#include "catch/catch.hpp"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"

using namespace dse;

// 正向测试：点光源、聚光灯和天光组件的默认值
TEST_CASE("Given_AdvancedLightComponents_When_Created_Then_DefaultValuesAreSet", "[engine][unit][rendering]") {
    PointLightComponent point_light;
    REQUIRE(point_light.enabled == true);
    REQUIRE(point_light.intensity == 1.0f);
    REQUIRE(point_light.radius == 10.0f);
    REQUIRE(point_light.falloff == 1.0f);
    REQUIRE(point_light.cast_shadow == false);

    SpotLightComponent spot_light;
    REQUIRE(spot_light.enabled == true);
    REQUIRE(spot_light.radius == 20.0f);
    REQUIRE(spot_light.falloff == 1.0f);
    REQUIRE(spot_light.inner_cone_angle == 12.5f);
    REQUIRE(spot_light.outer_cone_angle == 17.5f);

    SkyLightComponent sky_light;
    REQUIRE(sky_light.enabled == true);
    REQUIRE(sky_light.up_color.x == Approx(0.2f));
    REQUIRE(sky_light.up_color.y == Approx(0.2f));
    REQUIRE(sky_light.up_color.z == Approx(0.2f));
    REQUIRE(sky_light.down_color.x == Approx(0.0f));
    REQUIRE(sky_light.down_color.y == Approx(0.0f));
    REQUIRE(sky_light.down_color.z == Approx(0.5f));
    REQUIRE(sky_light.intensity == 1.0f);
}

// 边界测试：修改灯光组件参数并在 ECS 中正常存储
TEST_CASE("Given_World_When_AddingLightComponents_Then_DataIsRetained", "[engine][unit][rendering]") {
    World world;
    auto entity = world.CreateEntity();

    auto& p_light = world.registry().emplace<PointLightComponent>(entity);
    p_light.radius = 100.0f;
    p_light.falloff = 2.0f;
    p_light.color = glm::vec3(1.0f, 0.0f, 0.0f);

    auto& s_light = world.registry().emplace<SpotLightComponent>(entity);
    s_light.falloff = 3.0f;
    s_light.outer_cone_angle = 45.0f;

    auto& sky_light = world.registry().emplace<SkyLightComponent>(entity);
    sky_light.up_color = glm::vec3(0.9f, 0.8f, 0.7f);
    sky_light.down_color = glm::vec3(0.1f, 0.2f, 0.3f);
    sky_light.intensity = 1.8f;

    auto& retrieved_p = world.registry().get<PointLightComponent>(entity);
    REQUIRE(retrieved_p.radius == 100.0f);
    REQUIRE(retrieved_p.falloff == 2.0f);
    REQUIRE(retrieved_p.color.x == 1.0f);

    auto& retrieved_s = world.registry().get<SpotLightComponent>(entity);
    REQUIRE(retrieved_s.falloff == 3.0f);
    REQUIRE(retrieved_s.outer_cone_angle == 45.0f);

    auto& retrieved_sky = world.registry().get<SkyLightComponent>(entity);
    REQUIRE(retrieved_sky.up_color.x == Approx(0.9f));
    REQUIRE(retrieved_sky.up_color.y == Approx(0.8f));
    REQUIRE(retrieved_sky.up_color.z == Approx(0.7f));
    REQUIRE(retrieved_sky.down_color.z == Approx(0.3f));
    REQUIRE(retrieved_sky.intensity == Approx(1.8f));
}

// 正向测试：MorphComponent 数据结构测试
TEST_CASE("Given_MorphComponent_When_TargetsAdded_Then_StoredCorrectly", "[engine][unit][rendering]") {
    MorphComponent morph;
    morph.enabled = true;
    
    MorphTarget target1{"smile", 0.5f};
    MorphTarget target2{"blink", 1.0f};
    
    morph.targets.push_back(target1);
    morph.targets.push_back(target2);

    REQUIRE(morph.targets.size() == 2);
    REQUIRE(morph.targets[0].name == "smile");
    REQUIRE(morph.targets[0].weight == 0.5f);
    REQUIRE(morph.targets[1].weight == 1.0f);
}

// 反向测试：禁用组件时状态正常保持
TEST_CASE("Given_MorphComponent_When_Disabled_Then_DataIsKeptButFlagIsFalse", "[engine][unit][rendering]") {
    MorphComponent morph;
    morph.targets.push_back({"sad", 0.8f});
    morph.enabled = false;

    REQUIRE(morph.enabled == false);
    REQUIRE(morph.targets.size() == 1);
}
