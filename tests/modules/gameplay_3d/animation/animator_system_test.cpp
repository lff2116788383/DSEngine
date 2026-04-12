#include "catch/catch.hpp"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"

using namespace dse;

// 正向测试：默认 Animator3DComponent 状态应为禁用高级树且拥有默认参数。
TEST_CASE("Given_DefaultAnimator3DComponent_When_Created_Then_AnimTreeIsDisabled", "[engine][unit][animator3d]") {
    Animator3DComponent animator;
    REQUIRE(animator.enabled == true);
    REQUIRE(animator.use_anim_tree == false);
    REQUIRE(animator.blend_nodes.empty());
    REQUIRE(animator.speed == 1.0f);
    REQUIRE(animator.blend_parameter == "speed");
    REQUIRE(animator.blend_parameter_value == Approx(0.0f));
}

// 正向测试：配置使用 AnimTree 的实体，状态应被正确记录。
TEST_CASE("Given_AnimTreeEnabled_When_AddingBlendNodes_Then_NodesAreStored", "[engine][unit][animator3d]") {
    Animator3DComponent animator;
    animator.use_anim_tree = true;
    animator.blend_parameter = "locomotion_speed";
    animator.blend_parameter_value = 2.5f;

    AnimBlendNode node1;
    node1.name = "walk";
    node1.danim_path = "walk.danim";
    node1.weight = 0.8f;
    node1.threshold = 1.0f;

    AnimBlendNode node2;
    node2.name = "run";
    node2.danim_path = "run.danim";
    node2.weight = 0.2f;
    node2.threshold = 4.0f;

    animator.blend_nodes.push_back(node1);
    animator.blend_nodes.push_back(node2);

    REQUIRE(animator.use_anim_tree == true);
    REQUIRE(animator.blend_parameter == "locomotion_speed");
    REQUIRE(animator.blend_parameter_value == Approx(2.5f));
    REQUIRE(animator.blend_nodes.size() == 2);
    REQUIRE(animator.blend_nodes[0].name == "walk");
    REQUIRE(animator.blend_nodes[0].weight == 0.8f);
    REQUIRE(animator.blend_nodes[0].threshold == Approx(1.0f));
    REQUIRE(animator.blend_nodes[1].name == "run");
    REQUIRE(animator.blend_nodes[1].weight == 0.2f);
    REQUIRE(animator.blend_nodes[1].threshold == Approx(4.0f));
}

// 边界测试：空 AnimTree 节点列表
TEST_CASE("Given_EmptyAnimTree_When_Configured_Then_HandledSafely", "[engine][unit][animator3d]") {
    Animator3DComponent animator;
    animator.use_anim_tree = true;
    animator.blend_nodes.clear();
    
    REQUIRE(animator.blend_nodes.empty());
}
