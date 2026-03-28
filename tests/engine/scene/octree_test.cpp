#include "catch/catch.hpp"
#include "engine/scene/octree.h"
#include "engine/ecs/world.h"

using namespace dse::scene;

// 正向测试：基础 AABB 相交测试
TEST_CASE("Given_AABB_When_CheckIntersection_Then_ReturnsCorrectResult", "[engine][unit][octree]") {
    AABB box1{glm::vec3(-1.0f), glm::vec3(1.0f)};
    AABB box2{glm::vec3(0.0f), glm::vec3(2.0f)};
    AABB box3{glm::vec3(2.0f), glm::vec3(3.0f)};

    REQUIRE(box1.Intersects(box2) == true);
    REQUIRE(box2.Intersects(box1) == true);
    REQUIRE(box1.Intersects(box3) == false);
}

// 正向测试：向 Octree 插入实体并能够查询出结果
TEST_CASE("Given_Octree_When_InsertAndQuery_Then_FindsIntersectingElements", "[engine][unit][octree]") {
    AABB bounds{glm::vec3(-10.0f), glm::vec3(10.0f)};
    Octree tree(bounds, 2, 4); // capacity = 2, max_depth = 4

    // 插入三个实体，触发分割
    tree.Insert({(entt::entity)1, AABB{glm::vec3(1.0f), glm::vec3(2.0f)}});
    tree.Insert({(entt::entity)2, AABB{glm::vec3(3.0f), glm::vec3(4.0f)}});
    tree.Insert({(entt::entity)3, AABB{glm::vec3(-5.0f), glm::vec3(-4.0f)}});

    REQUIRE(tree.IsDivided() == true);

    std::vector<OctreeData> results;
    // 查询右上方区域
    tree.Query(AABB{glm::vec3(0.0f), glm::vec3(5.0f)}, results);
    
    REQUIRE(results.size() == 2);
    bool found_1 = false, found_2 = false;
    for (const auto& data : results) {
        if (data.entity == (entt::entity)1) found_1 = true;
        if (data.entity == (entt::entity)2) found_2 = true;
    }
    REQUIRE(found_1 == true);
    REQUIRE(found_2 == true);
}

// 边界测试：实体完全在边界外，或在八叉树最大深度限制时的行为
TEST_CASE("Given_Octree_When_InsertOutsideOrReachMaxDepth_Then_HandlesCorrectly", "[engine][unit][octree]") {
    AABB bounds{glm::vec3(0.0f), glm::vec3(10.0f)};
    Octree tree(bounds, 1, 1); // 极小的容量和最大深度限制

    // 插入外部实体，不应被包含
    tree.Insert({(entt::entity)1, AABB{glm::vec3(-5.0f), glm::vec3(-2.0f)}});
    std::vector<OctreeData> results;
    tree.Query(bounds, results);
    REQUIRE(results.empty() == true);

    // 触发分割
    tree.Insert({(entt::entity)2, AABB{glm::vec3(1.0f), glm::vec3(2.0f)}});
    tree.Insert({(entt::entity)3, AABB{glm::vec3(2.0f), glm::vec3(3.0f)}});
    
    // 因为 max_depth = 1，初始是 0，分割一次后到达 1
    // 即使插入更多在同一个子节点，也不会再分割
    tree.Insert({(entt::entity)4, AABB{glm::vec3(1.5f), glm::vec3(2.5f)}});
    tree.Insert({(entt::entity)5, AABB{glm::vec3(1.8f), glm::vec3(2.8f)}});

    results.clear();
    tree.Query(bounds, results);
    REQUIRE(results.size() == 4); // 2, 3, 4, 5
}

// 反向测试：处理非预期的空查询或清除树后的状态
TEST_CASE("Given_Octree_When_Cleared_Then_EmptyAndUndivided", "[engine][unit][octree]") {
    AABB bounds{glm::vec3(-10.0f), glm::vec3(10.0f)};
    Octree tree(bounds, 1, 4);

    tree.Insert({(entt::entity)1, AABB{glm::vec3(1.0f), glm::vec3(2.0f)}});
    tree.Insert({(entt::entity)2, AABB{glm::vec3(3.0f), glm::vec3(4.0f)}});
    REQUIRE(tree.IsDivided() == true);

    tree.Clear();
    REQUIRE(tree.IsDivided() == false);
    REQUIRE(tree.GetElements().empty() == true);

    std::vector<OctreeData> results;
    tree.Query(bounds, results);
    REQUIRE(results.empty() == true);
}
