/**
 * @file rhi_handle_test.cpp
 * @brief TypedHandle 编译期与运行时行为的单元测试
 *
 * 覆盖场景：
 * - sizeof / trivial 属性 static_assert
 * - 默认值、比较运算符、bool 转换
 * - 不同 Tag 类型之间不可隐式转换（编译期保证，此处仅验证运行时语义）
 * - std::hash 特化正确性
 * - from_raw / raw 互操作
 */

#include <gtest/gtest.h>
#include "engine/render/rhi/rhi_handle.h"

#include <unordered_set>
#include <set>

using namespace dse::render;

// ============================================================
// 编译期属性（重复验证，确保测试目标头文件被正确 include）
// ============================================================

static_assert(sizeof(TextureHandle) == 4);
static_assert(sizeof(BufferHandle) == 4);
static_assert(sizeof(VertexArrayHandle) == 4);
static_assert(sizeof(RenderTargetHandle) == 4);
static_assert(sizeof(PipelineHandle) == 4);

static_assert(std::is_trivially_copyable_v<TextureHandle>);
static_assert(std::is_trivially_destructible_v<BufferHandle>);
static_assert(std::is_standard_layout_v<VertexArrayHandle>);

// ============================================================
// 默认值
// ============================================================

TEST(TypedHandleTest, DefaultIsZero) {
    BufferHandle h;
    EXPECT_EQ(h.id, 0u);
    EXPECT_FALSE(static_cast<bool>(h));
}

// ============================================================
// 构造与 bool 转换
// ============================================================

TEST(TypedHandleTest, ExplicitConstruct) {
    BufferHandle h{42};
    EXPECT_EQ(h.id, 42u);
    EXPECT_TRUE(static_cast<bool>(h));
}

TEST(TypedHandleTest, FromRawAndRaw) {
    auto h = TextureHandle::from_raw(123);
    EXPECT_EQ(h.raw(), 123u);
    EXPECT_EQ(h.id, 123u);
}

// ============================================================
// 比较运算符
// ============================================================

TEST(TypedHandleTest, Equality) {
    BufferHandle a{10}, b{10}, c{20};
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a == c);
    EXPECT_FALSE(a != b);
    EXPECT_TRUE(a != c);
}

TEST(TypedHandleTest, LessThan) {
    BufferHandle a{5}, b{10};
    EXPECT_TRUE(a < b);
    EXPECT_FALSE(b < a);
    EXPECT_FALSE(a < a);
}

// ============================================================
// std::hash — unordered 容器可用
// ============================================================

TEST(TypedHandleTest, HashInUnorderedSet) {
    std::unordered_set<BufferHandle> s;
    s.insert(BufferHandle{1});
    s.insert(BufferHandle{2});
    s.insert(BufferHandle{1});  // duplicate
    EXPECT_EQ(s.size(), 2u);
    EXPECT_TRUE(s.count(BufferHandle{1}) == 1);
    EXPECT_TRUE(s.count(BufferHandle{3}) == 0);
}

// ============================================================
// operator< — ordered 容器可用
// ============================================================

TEST(TypedHandleTest, OrderedSet) {
    std::set<TextureHandle> s;
    s.insert(TextureHandle{30});
    s.insert(TextureHandle{10});
    s.insert(TextureHandle{20});
    auto it = s.begin();
    EXPECT_EQ(it->id, 10u); ++it;
    EXPECT_EQ(it->id, 20u); ++it;
    EXPECT_EQ(it->id, 30u);
}

// ============================================================
// 类型隔离 — 不同 Tag 的 handle 是不同类型
// ============================================================

TEST(TypedHandleTest, DifferentTagsAreDifferentTypes) {
    // 编译期保证：以下赋值若取消注释则编译失败
    // BufferHandle bh{1};
    // TextureHandle th = bh;  // ERROR: 类型不匹配

    // 运行时验证：同 id 不同类型不会混淆
    EXPECT_FALSE((std::is_same_v<BufferHandle, TextureHandle>));
    EXPECT_FALSE((std::is_same_v<BufferHandle, VertexArrayHandle>));
    EXPECT_FALSE((std::is_same_v<TextureHandle, RenderTargetHandle>));
    EXPECT_FALSE((std::is_same_v<RenderTargetHandle, PipelineHandle>));
}
