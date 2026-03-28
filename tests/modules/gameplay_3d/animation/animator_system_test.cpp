#include "catch/catch.hpp"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d.h"

using namespace dse;

// 正向测试：默认 Animator3DComponent 状态应为禁用高级树且拥有默认的空参数。
TEST_CASE("Given_DefaultAnimator3DComponent_When_Created_Then_AnimTreeIsDisabled", "[engine][unit][animator3d]") {
    Animator3DComponent animator;
    REQUIRE(animator.enabled == true);
    REQUIRE(animator.use_anim_tree == false);
    REQUIRE(animator.blend_nodes.empty());
    REQUIRE(animator.speed == 1.0f);
}

// 正向测试：配置使用 AnimTree 的实体，状态应被正确记录。
TEST_CASE("Given_AnimTreeEnabled_When_AddingBlendNodes_Then_NodesAreStored", "[engine][unit][animator3d]") {
    Animator3DComponent animator;
    animator.use_anim_tree = true;
    
    AnimBlendNode node1;
    node1.danim_path = "walk.danim";
    node1.weight = 0.8f;
    
    AnimBlendNode node2;
    node2.danim_path = "run.danim";
    node2.weight = 0.2f;
    
    animator.blend_nodes.push_back(node1);
    animator.blend_nodes.push_back(node2);
    
    REQUIRE(animator.use_anim_tree == true);
    REQUIRE(animator.blend_nodes.size() == 2);
    REQUIRE(animator.blend_nodes[0].weight == 0.8f);
    REQUIRE(animator.blend_nodes[1].weight == 0.2f);
}

// 边界测试：空 AnimTree 节点列表
TEST_CASE("Given_EmptyAnimTree_When_Configured_Then_HandledSafely", "[engine][unit][animator3d]") {
    Animator3DComponent animator;
    animator.use_anim_tree = true;
    animator.blend_nodes.clear();
    
    REQUIRE(animator.blend_nodes.empty());
}
