#include "catch/catch.hpp"
#include "modules/gameplay_2d/camera/camera_system.h"
#include "engine/ecs/components_2d.h"
#include <cmath>

// 正向测试：跟随目标且阻尼为 1 时，摄像机应移动到目标偏移位置。
TEST_CASE("Given_FollowEnabledCamera_When_Update_Then_PositionTracksTarget", "[engine][unit][camera]") {
    World world;
    auto target = world.CreateEntity();
    auto camera_entity = world.CreateEntity();

    auto& target_tf = world.registry().emplace<TransformComponent>(target);
    target_tf.position = glm::vec3(5.0f, 6.0f, 0.0f);

    auto& camera_tf = world.registry().emplace<TransformComponent>(camera_entity);
    camera_tf.position = glm::vec3(0.0f, 0.0f, 0.0f);

    world.registry().emplace<CameraComponent>(camera_entity);
    auto& follow = world.registry().emplace<CameraFollowComponent>(camera_entity);
    follow.target = target;
    follow.offset = glm::vec3(1.0f, -2.0f, 0.0f);
    follow.damping = 1.0f;

    CameraSystem system;
    system.Update(world, 2.0f);

    REQUIRE(camera_tf.position.x == Approx(6.0f));
    REQUIRE(camera_tf.position.y == Approx(4.0f));
}

// 边界测试：宽高比非法时应自动回退，不应导致投影矩阵异常。
TEST_CASE("Given_InvalidAspectRatio_When_Update_Then_ProjectionRemainsFinite", "[engine][unit][camera]") {
    World world;
    auto camera_entity = world.CreateEntity();
    world.registry().emplace<TransformComponent>(camera_entity);
    auto& camera = world.registry().emplace<CameraComponent>(camera_entity);
    camera.orthographic = true;

    CameraSystem system;
    system.Update(world, 0.0f);

    REQUIRE(std::isfinite(camera.projection[0][0]));
    REQUIRE(std::isfinite(camera.projection[1][1]));
}

// 反向测试：跟随阻尼为负值时应被夹紧到 0，位置不应发生跳变。
TEST_CASE("Given_NegativeDamping_When_Update_Then_CameraPositionDoesNotChange", "[engine][unit][camera]") {
    World world;
    auto target = world.CreateEntity();
    auto camera_entity = world.CreateEntity();

    auto& target_tf = world.registry().emplace<TransformComponent>(target);
    target_tf.position = glm::vec3(10.0f, 10.0f, 0.0f);
    auto& camera_tf = world.registry().emplace<TransformComponent>(camera_entity);
    camera_tf.position = glm::vec3(1.0f, 2.0f, 0.0f);
    world.registry().emplace<CameraComponent>(camera_entity);
    auto& follow = world.registry().emplace<CameraFollowComponent>(camera_entity);
    follow.target = target;
    follow.damping = -1.0f;

    CameraSystem system;
    system.Update(world, 1.0f);

    REQUIRE(camera_tf.position.x == Approx(1.0f));
    REQUIRE(camera_tf.position.y == Approx(2.0f));
}
