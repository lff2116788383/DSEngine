#include "catch/catch.hpp"
#include "engine/scene/transform_system.h"
#include "engine/ecs/components_2d.h"
#include <glm/gtx/quaternion.hpp>
#include <cmath>

namespace {
glm::vec3 ExtractTranslation(const glm::mat4& matrix) {
    return glm::vec3(matrix[3]);
}
}

// 正向测试：父子层级下更新变换后，子节点世界坐标应叠加父节点平移。
TEST_CASE("Given_ParentChildTransforms_When_Update_Then_ChildUsesParentWorldMatrix", "[engine][unit][transform]") {
    World world;
    auto parent = world.CreateEntity();
    auto child = world.CreateEntity();

    auto& parent_tf = world.registry().emplace<TransformComponent>(parent);
    parent_tf.position = glm::vec3(10.0f, 0.0f, 0.0f);

    auto& child_tf = world.registry().emplace<TransformComponent>(child);
    child_tf.position = glm::vec3(2.0f, 0.0f, 0.0f);
    world.registry().emplace<ParentComponent>(child, parent);

    TransformSystem system;
    system.Update(world);

    REQUIRE(ExtractTranslation(parent_tf.local_to_world).x == Approx(10.0f));
    REQUIRE(ExtractTranslation(child_tf.local_to_world).x == Approx(12.0f));
    REQUIRE_FALSE(parent_tf.dirty);
    REQUIRE_FALSE(child_tf.dirty);
}

// 边界测试：当父节点无效时，系统应回退为单位父矩阵，不影响本地变换结果。
TEST_CASE("Given_InvalidParentEntity_When_Update_Then_LocalMatrixIsUsed", "[engine][unit][transform]") {
    World world;
    auto entity = world.CreateEntity();

    auto& tf = world.registry().emplace<TransformComponent>(entity);
    tf.position = glm::vec3(3.0f, 4.0f, 0.0f);
    world.registry().emplace<ParentComponent>(entity, entt::null);

    TransformSystem system;
    system.Update(world);

    REQUIRE(ExtractTranslation(tf.local_to_world).x == Approx(3.0f));
    REQUIRE(ExtractTranslation(tf.local_to_world).y == Approx(4.0f));
}

// 反向测试：层级存在循环引用时，更新应可结束并保持矩阵为有限值。
TEST_CASE("Given_CyclicParentRelation_When_Update_Then_ComputationStillTerminates", "[engine][unit][transform]") {
    World world;
    auto a = world.CreateEntity();
    auto b = world.CreateEntity();

    auto& a_tf = world.registry().emplace<TransformComponent>(a);
    auto& b_tf = world.registry().emplace<TransformComponent>(b);
    a_tf.position = glm::vec3(1.0f, 0.0f, 0.0f);
    b_tf.position = glm::vec3(0.0f, 2.0f, 0.0f);

    world.registry().emplace<ParentComponent>(a, b);
    world.registry().emplace<ParentComponent>(b, a);

    TransformSystem system;
    system.Update(world);

    const auto a_pos = ExtractTranslation(a_tf.local_to_world);
    const auto b_pos = ExtractTranslation(b_tf.local_to_world);
    REQUIRE(std::isfinite(a_pos.x));
    REQUIRE(std::isfinite(a_pos.y));
    REQUIRE(std::isfinite(b_pos.x));
    REQUIRE(std::isfinite(b_pos.y));
}
