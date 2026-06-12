/**
 * @file event_id_test.cpp
 * @brief EventId 编译期哈希的单元测试
 *
 * 覆盖场景：
 * - FNV-1a 编译期哈希的一致性
 * - EventId 常量跨编译单元稳定性
 * - 预定义事件常量的基本验证
 *
 * 注意：EventBus 相关测试已统一在 event_bus_test.cpp 中
 */

#include <gtest/gtest.h>
#include "engine/core/event_id.h"

using namespace dse::core;

// ============================================================
// EventId 测试
// ============================================================

TEST(EventIdTest, Fnv1aHashconsistency) {
    // 相同字符串必须产生相同哈希值
    constexpr EventId id1 = MakeEventId("TestEvent");
    constexpr EventId id2 = MakeEventId("TestEvent");
    EXPECT_EQ(id1, id2);
}

TEST(EventIdTest, Fnv1aHashDifferentStringsProduceDifferentValues) {
    constexpr EventId id1 = MakeEventId("EventA");
    constexpr EventId id2 = MakeEventId("EventB");
    EXPECT_NE(id1, id2);
}

TEST(EventIdTest, EmptyFNVoffset) {
    constexpr EventId empty_hash = MakeEventId("");
    constexpr EventId fnv_offset = 0xcbf29ce484222325ull;
    EXPECT_EQ(empty_hash, fnv_offset);
}

TEST(EventIdTest, EventIDNonZero) {
    EXPECT_NE(events::kUiClick, 0u);
    EXPECT_NE(events::kResourceLoaded, 0u);
    EXPECT_NE(events::kSceneLifecycle, 0u);
}

TEST(EventIdTest, EventIDNotEqual) {
    EXPECT_NE(events::kUiClick, events::kResourceLoaded);
    EXPECT_NE(events::kUiClick, events::kSceneLifecycle);
    EXPECT_NE(events::kResourceLoaded, events::kSceneLifecycle);
}

TEST(EventIdTest, TestCase6) {
    // 验证 MakeEventId 可以在编译期使用（constexpr）
    static_assert(MakeEventId("CompileTime") != 0, "MakeEventId must be constexpr");
    SUCCEED();
}
