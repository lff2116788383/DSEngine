/**
 * @file octree_test.cpp
 * @brief Octree / AABB 八叉树与包围盒单元测试
 *
 * 覆盖场景：
 * - AABB 包含点检测
 * - AABB 相交检测
 * - Octree 插入与查询
 * - Octree 细分
 * - Octree Clear
 * - Octree 不相交的插入被忽略
 */

#include <gtest/gtest.h>
#include "engine/scene/octree.h"
#include <glm/glm.hpp>

using namespace dse::scene;

// ============================================================
// AABB 测试
// ============================================================

TEST(AABBTest, 包含内部点) {
    AABB box{glm::vec3(-1.0f), glm::vec3(1.0f)};
    EXPECT_TRUE(box.Contains(glm::vec3(0.0f)));
    EXPECT_TRUE(box.Contains(glm::vec3(-1.0f)));
    EXPECT_TRUE(box.Contains(glm::vec3(1.0f)));
}

TEST(AABBTest, 不包含外部点) {
    AABB box{glm::vec3(-1.0f), glm::vec3(1.0f)};
    EXPECT_FALSE(box.Contains(glm::vec3(2.0f, 0.0f, 0.0f)));
    EXPECT_FALSE(box.Contains(glm::vec3(0.0f, -2.0f, 0.0f)));
    EXPECT_FALSE(box.Contains(glm::vec3(0.0f, 0.0f, 2.0f)));
}

TEST(AABBTest, 重叠AABB相交) {
    AABB a{glm::vec3(0.0f), glm::vec3(2.0f)};
    AABB b{glm::vec3(1.0f), glm::vec3(3.0f)};
    EXPECT_TRUE(a.Intersects(b));
    EXPECT_TRUE(b.Intersects(a));
}

TEST(AABBTest, 不重叠AABB不相交) {
    AABB a{glm::vec3(0.0f), glm::vec3(1.0f)};
    AABB b{glm::vec3(2.0f), glm::vec3(3.0f)};
    EXPECT_FALSE(a.Intersects(b));
    EXPECT_FALSE(b.Intersects(a));
}

TEST(AABBTest, 包含关系的AABB相交) {
    AABB outer{glm::vec3(-5.0f), glm::vec3(5.0f)};
    AABB inner{glm::vec3(-1.0f), glm::vec3(1.0f)};
    EXPECT_TRUE(outer.Intersects(inner));
    EXPECT_TRUE(inner.Intersects(outer));
}

TEST(AABBTest, 仅边接触的AABB相交) {
    AABB a{glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 1.0f, 1.0f)};
    AABB b{glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(2.0f, 1.0f, 1.0f)};
    EXPECT_TRUE(a.Intersects(b)); // 边接触视为相交
}

// ============================================================
// Octree 基本功能
// ============================================================

TEST(OctreeTest, 插入并查询到数据) {
    AABB bounds{glm::vec3(-10.0f), glm::vec3(10.0f)};
    Octree tree(bounds, 4);

    entt::registry reg;
    auto entity = reg.create();
    OctreeData data{entity, AABB{glm::vec3(0.0f), glm::vec3(1.0f)}};
    tree.Insert(data);

    std::vector<OctreeData> found;
    AABB query_range{glm::vec3(-5.0f), glm::vec3(5.0f)};
    tree.Query(query_range, found);
    EXPECT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].entity, entity);
}

TEST(OctreeTest, 查询不相交范围返回空) {
    AABB bounds{glm::vec3(-10.0f), glm::vec3(10.0f)};
    Octree tree(bounds, 4);

    entt::registry reg;
    auto entity = reg.create();
    OctreeData data{entity, AABB{glm::vec3(0.0f), glm::vec3(1.0f)}};
    tree.Insert(data);

    std::vector<OctreeData> found;
    AABB query_range{glm::vec3(5.0f), glm::vec3(10.0f)};
    tree.Query(query_range, found);
    EXPECT_TRUE(found.empty());
}

TEST(OctreeTest, 插入超出边界的数据被忽略) {
    AABB bounds{glm::vec3(0.0f), glm::vec3(10.0f)};
    Octree tree(bounds, 4);

    entt::registry reg;
    auto entity = reg.create();
    // 此数据完全在树范围之外
    OctreeData data{entity, AABB{glm::vec3(20.0f), glm::vec3(30.0f)}};
    tree.Insert(data);

    std::vector<OctreeData> found;
    tree.Query(bounds, found);
    EXPECT_TRUE(found.empty());
}

TEST(OctreeTest, 超过容量时细分) {
    AABB bounds{glm::vec3(-10.0f), glm::vec3(10.0f)};
    Octree tree(bounds, /*capacity=*/2);

    entt::registry reg;
    for (int i = 0; i < 5; ++i) {
        auto entity = reg.create();
        OctreeData data{entity, AABB{glm::vec3(static_cast<float>(i)), glm::vec3(static_cast<float>(i) + 1.0f)}};
        tree.Insert(data);
    }

    // 插入 5 个元素（容量为 2）后应触发细分
    EXPECT_TRUE(tree.IsDivided());
}

TEST(OctreeTest, 清空后树为空) {
    AABB bounds{glm::vec3(-10.0f), glm::vec3(10.0f)};
    Octree tree(bounds, 4);

    entt::registry reg;
    auto entity = reg.create();
    tree.Insert({entity, AABB{glm::vec3(0.0f), glm::vec3(1.0f)}});

    tree.Clear();

    std::vector<OctreeData> found;
    tree.Query(bounds, found);
    EXPECT_TRUE(found.empty());
    EXPECT_FALSE(tree.IsDivided());
}

TEST(OctreeTest, 多个实体查询) {
    AABB bounds{glm::vec3(-10.0f), glm::vec3(10.0f)};
    Octree tree(bounds, 8);

    entt::registry reg;
    std::vector<entt::entity> entities;
    for (int i = 0; i < 10; ++i) {
        auto entity = reg.create();
        entities.push_back(entity);
        float pos = static_cast<float>(i) - 5.0f;
        tree.Insert({entity, AABB{glm::vec3(pos, 0.0f, 0.0f), glm::vec3(pos + 1.0f, 1.0f, 1.0f)}});
    }

    // 查询整个范围应找到所有 10 个实体
    std::vector<OctreeData> found;
    tree.Query(bounds, found);
    EXPECT_EQ(found.size(), 10u);
}
