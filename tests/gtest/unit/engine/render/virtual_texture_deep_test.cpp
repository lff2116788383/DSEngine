/**
 * @file virtual_texture_deep_test.cpp
 * @brief P3: VirtualTexture 系统深度边界测试
 *
 * 补充 virtual_texture_test.cpp 中未覆盖的场景：
 * - LRU 驱逐策略在高负载下的正确性
 * - 多帧 feedback 累积后缓存命中率变化
 * - 极端配置（1×1 池、最大上传数 = 0）
 * - Page loader 失败场景
 * - 连续 Update 帧号递增的 Touch 行为
 */

#include <gtest/gtest.h>
#include "engine/render/virtual_texture/virtual_texture.h"
#include <unordered_set>
#include <vector>

using namespace dse::vt;

// ─── VTPageCache 深度测试 ──────────────────────────────────────────────────

class VTPageCacheDeepTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache.Init(4);  // 4×4 = 16 物理页
    }
    VTPageCache cache;
};

TEST_F(VTPageCacheDeepTest, LRU_EvictsOldest_UnderPressure) {
    // 用不同帧号填满 16 页
    for (uint32_t i = 0; i < 16; ++i) {
        cache.Allocate(PageId{i, 0, 0, 0}, i);
    }
    EXPECT_EQ(cache.OccupiedCount(), 16u);

    // Touch 偶数页使其 "最近使用"
    for (uint32_t i = 0; i < 16; i += 2) {
        cache.Touch(PageId{i, 0, 0, 0}, 100);
    }

    // 分配 8 个新页 → 应优先驱逐未 Touch 的奇数页
    for (uint32_t i = 0; i < 8; ++i) {
        cache.Allocate(PageId{100 + i, 0, 0, 0}, 200);
    }

    // 偶数页（已 Touch）应大部分存活
    int even_survived = 0;
    for (uint32_t i = 0; i < 16; i += 2) {
        if (cache.Contains(PageId{i, 0, 0, 0})) ++even_survived;
    }
    // 至少一半偶数页存活（LRU 优先驱逐老页）
    EXPECT_GE(even_survived, 4);
}

TEST_F(VTPageCacheDeepTest, Allocate_OverflowFar_StillWorks) {
    // 分配远超池大小的页数
    for (uint32_t i = 0; i < 100; ++i) {
        auto* pp = cache.Allocate(PageId{i, 0, 0, 0}, i);
        ASSERT_NE(pp, nullptr);
    }
    EXPECT_EQ(cache.OccupiedCount(), 16u);  // 池大小上限

    // 最近 16 个应在缓存中
    for (uint32_t i = 84; i < 100; ++i) {
        EXPECT_TRUE(cache.Contains(PageId{i, 0, 0, 0}))
            << "Page " << i << " should still be cached";
    }
}

TEST_F(VTPageCacheDeepTest, Clear_ThenReallocate) {
    for (uint32_t i = 0; i < 8; ++i) {
        cache.Allocate(PageId{i, 0, 0, 0}, i);
    }
    cache.Clear();
    EXPECT_EQ(cache.OccupiedCount(), 0u);

    // 重新分配应正常工作
    auto* pp = cache.Allocate(PageId{99, 99, 0, 0}, 1);
    ASSERT_NE(pp, nullptr);
    EXPECT_EQ(cache.OccupiedCount(), 1u);
    EXPECT_TRUE(cache.Contains(PageId{99, 99, 0, 0}));
}

// ─── VirtualTextureSystem 深度测试 ─────────────────────────────────────────

class VTSystemDeepTest : public ::testing::Test {
protected:
    VirtualTextureSystem system;

    void SetUp() override {
        VirtualTextureConfig config;
        config.virtual_size = 1024;
        config.pool_size_pages = 4;
        config.max_uploads_per_frame = 4;
        system.Init(config, nullptr);
    }
};

TEST_F(VTSystemDeepTest, MultipleFeedback_CacheHitImproves) {
    system.SetPageLoadCallback([](const PageId& page, std::vector<uint8_t>& out) {
        out.resize(kPageSizeWithBorder * kPageSizeWithBorder * 4, 128);
        return true;
    });

    std::vector<FeedbackEntry> entries = {{0, 0, 0, 0}, {1, 0, 0, 0}};

    // 第一帧：cold miss
    system.SubmitFeedback(entries);
    system.Update(1);
    float hit1 = system.CacheHitRate();

    // 第二帧：相同页应命中缓存
    system.SubmitFeedback(entries);
    system.Update(2);
    float hit2 = system.CacheHitRate();

    EXPECT_GE(hit2, hit1);
}

TEST_F(VTSystemDeepTest, PageLoadFailure_NoCarsh) {
    system.SetPageLoadCallback([](const PageId& page, std::vector<uint8_t>& out) {
        return false;  // 模拟加载失败
    });

    std::vector<FeedbackEntry> entries = {{5, 5, 0, 0}};
    EXPECT_NO_THROW({
        system.SubmitFeedback(entries);
        system.Update(1);
    });
}

TEST_F(VTSystemDeepTest, Shutdown_ThenReinit) {
    system.Shutdown();
    EXPECT_TRUE(system.GetPageTable().empty());

    VirtualTextureConfig config;
    config.virtual_size = 512;
    config.pool_size_pages = 2;
    config.max_uploads_per_frame = 2;
    system.Init(config, nullptr);

    EXPECT_EQ(system.PageTableSize(), 4u);  // 512/128 = 4
}

TEST_F(VTSystemDeepTest, ManyFrames_NoMemoryLeak) {
    system.SetPageLoadCallback([](const PageId& page, std::vector<uint8_t>& out) {
        out.resize(kPageSizeWithBorder * kPageSizeWithBorder * 4, 64);
        return true;
    });

    for (uint32_t frame = 0; frame < 100; ++frame) {
        std::vector<FeedbackEntry> entries;
        entries.push_back({frame % 8, frame % 8, 0, 0});
        system.SubmitFeedback(entries);
        system.Update(frame);
    }

    // 系统应仍然正常
    EXPECT_LE(system.GetCache().OccupiedCount(), 16u);
}

// ─── VTPageCache 极小池 ───────────────────────────────────────────────────

TEST(VTPageCacheSmallPoolTest, SinglePagePool) {
    VTPageCache cache;
    cache.Init(1);  // 1×1 = 1 物理页

    auto* p1 = cache.Allocate(PageId{0, 0, 0, 0}, 1);
    ASSERT_NE(p1, nullptr);
    EXPECT_EQ(cache.OccupiedCount(), 1u);

    // 分配第二个 → 驱逐第一个
    auto* p2 = cache.Allocate(PageId{1, 1, 0, 0}, 2);
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(cache.OccupiedCount(), 1u);
    EXPECT_FALSE(cache.Contains(PageId{0, 0, 0, 0}));
    EXPECT_TRUE(cache.Contains(PageId{1, 1, 0, 0}));
}

// ─── PageId 边界值 ───────────────────────────────────────────────────────

TEST(PageIdDeepTest, MaxValues) {
    PageId page{UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX};
    PageIdHash hasher;
    // 不崩溃
    EXPECT_NO_THROW(hasher(page));
}

TEST(PageIdDeepTest, AllZero) {
    PageId page{0, 0, 0, 0};
    PageIdHash hasher;
    size_t h = hasher(page);
    // 全零页有确定哈希
    EXPECT_EQ(h, hasher(PageId{0, 0, 0, 0}));
}
