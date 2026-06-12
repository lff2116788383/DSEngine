/**
 * @file quad_tree_test.cpp
 * @brief QuadTree / Rect 四叉树与矩形区域单元测试
 *
 * 覆盖场景：
 * - Rect 包含点检测
 * - Rect 相交检测
 * - QuadTree 插入与查询
 * - QuadTree 细分
 * - QuadTree Clear
 * - 不相交的插入被忽略
 */

#include <gtest/gtest.h>
#include "engine/scene/quad_tree.h"
#include <glm/glm.hpp>

using namespace dse::scene;

// ============================================================
// Rect 测试
// ============================================================

// 测试 矩形：Insidepoint
TEST(RectTest, Insidepoint) {
    Rect r{0.0f, 0.0f, 10.0f, 10.0f};
    EXPECT_TRUE(r.Contains(glm::vec2(5.0f, 5.0f)));
    EXPECT_TRUE(r.Contains(glm::vec2(0.0f, 0.0f)));
    EXPECT_TRUE(r.Contains(glm::vec2(10.0f, 10.0f)));
}

// 测试 矩形：不Contain外部点
TEST(RectTest, DoesNotContainExternalPoints) {
    Rect r{0.0f, 0.0f, 10.0f, 10.0f};
    EXPECT_FALSE(r.Contains(glm::vec2(15.0f, 5.0f)));
    EXPECT_FALSE(r.Contains(glm::vec2(5.0f, 15.0f)));
    EXPECT_FALSE(r.Contains(glm::vec2(-5.0f, 5.0f)));
}

// 测试 矩形：矩形
TEST(RectTest, Rect) {
    Rect a{0.0f, 0.0f, 10.0f, 10.0f};
    Rect b{5.0f, 5.0f, 10.0f, 10.0f};
    EXPECT_TRUE(a.Intersects(b));
    EXPECT_TRUE(b.Intersects(a));
}

// 测试 矩形：无重叠Rectdisjoint
TEST(RectTest, NoOverlapRectdisjoint) {
    Rect a{0.0f, 0.0f, 10.0f, 10.0f};
    Rect b{20.0f, 20.0f, 10.0f, 10.0f};
    EXPECT_FALSE(a.Intersects(b));
    EXPECT_FALSE(b.Intersects(a));
}

// 测试 矩形：矩形2
TEST(RectTest, Rect_2) {
    Rect a{0.0f, 0.0f, 10.0f, 10.0f};
    Rect b{10.0f, 0.0f, 10.0f, 10.0f};
    EXPECT_TRUE(a.Intersects(b));
}

// ============================================================
// QuadTree 基本功能
// ============================================================

// 测试 四边形树：查询Todata
TEST(QuadTreeTest, QueryTodata) {
    Rect bounds{0.0f, 0.0f, 100.0f, 100.0f};
    QuadTree tree(bounds, 4);

    entt::registry reg;
    auto entity = reg.create();
    QuadTreeData data{entity, Rect{40.0f, 40.0f, 20.0f, 20.0f}};
    tree.Insert(data);

    std::vector<QuadTreeData> found;
    Rect query_range{0.0f, 0.0f, 100.0f, 100.0f};
    tree.Query(query_range, found);
    EXPECT_EQ(found.size(), 1u);
    EXPECT_EQ(found[0].entity, entity);
}

// 测试 四边形树：Querydisjoint返回空
TEST(QuadTreeTest, QuerydisjointReturnsEmpty) {
    Rect bounds{0.0f, 0.0f, 100.0f, 100.0f};
    QuadTree tree(bounds, 4);

    entt::registry reg;
    auto entity = reg.create();
    tree.Insert({entity, Rect{10.0f, 10.0f, 10.0f, 10.0f}});

    std::vector<QuadTreeData> found;
    Rect query_range{50.0f, 50.0f, 10.0f, 10.0f};
    tree.Query(query_range, found);
    EXPECT_TRUE(found.empty());
}

// 测试 四边形树：当
TEST(QuadTreeTest, When) {
    Rect bounds{0.0f, 0.0f, 100.0f, 100.0f};
    // capacity = 2，插入 5 个应触发细分
    QuadTree tree(bounds, /*capacity=*/2);

    entt::registry reg;
    for (int i = 0; i < 5; ++i) {
        auto entity = reg.create();
        float x = static_cast<float>(i) * 10.0f;
        tree.Insert({entity, Rect{x, 0.0f, 10.0f, 10.0f}});
    }

    // 超过容量后应触发细分（内部 divided_ 应为 true）
    // 由于 QuadTree 没有暴露 IsDivided 接口，通过查询验证即可
    std::vector<QuadTreeData> found;
    tree.Query(bounds, found);
    EXPECT_EQ(found.size(), 5u);
}

// 测试 四边形树：清空之后为空
TEST(QuadTreeTest, ClearAfterIsEmpty) {
    Rect bounds{0.0f, 0.0f, 100.0f, 100.0f};
    QuadTree tree(bounds, 4);

    entt::registry reg;
    auto entity = reg.create();
    tree.Insert({entity, Rect{10.0f, 10.0f, 10.0f, 10.0f}});

    tree.Clear();

    std::vector<QuadTreeData> found;
    tree.Query(bounds, found);
    EXPECT_TRUE(found.empty());
}

// 测试 四边形树：多实体查询
TEST(QuadTreeTest, MultiEntityQuery) {
    Rect bounds{0.0f, 0.0f, 100.0f, 100.0f};
    QuadTree tree(bounds, 8);

    entt::registry reg;
    std::vector<entt::entity> entities;
    for (int i = 0; i < 10; ++i) {
        auto entity = reg.create();
        entities.push_back(entity);
        float x = static_cast<float>(i) * 10.0f;
        tree.Insert({entity, Rect{x, 0.0f, 5.0f, 5.0f}});
    }

    // 查询整个范围应找到所有 10 个实体
    std::vector<QuadTreeData> found;
    tree.Query(bounds, found);
    EXPECT_EQ(found.size(), 10u);
}

// 测试 四边形树：数据按
TEST(QuadTreeTest, DataBy) {
    Rect bounds{0.0f, 0.0f, 10.0f, 10.0f};
    QuadTree tree(bounds, 4);

    entt::registry reg;
    auto entity = reg.create();
    // 数据完全在树范围之外
    tree.Insert({entity, Rect{50.0f, 50.0f, 10.0f, 10.0f}});

    std::vector<QuadTreeData> found;
    tree.Query(bounds, found);
    EXPECT_TRUE(found.empty());
}
