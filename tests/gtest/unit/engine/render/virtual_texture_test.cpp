/**
 * @file virtual_texture_test.cpp
 * @brief 虚拟纹理系统单元测试
 */

#include <gtest/gtest.h>
#include "engine/render/virtual_texture/virtual_texture.h"
#include <unordered_set>

using namespace dse::vt;

// ─── VTPageCache 测试 ──────────────────────────────────────────────────────

class VTPageCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache.Init(4);  // 4×4 = 16 物理页
    }
    VTPageCache cache;
};

TEST_F(VTPageCacheTest, Init_Empty) {
    EXPECT_EQ(cache.OccupiedCount(), 0u);
    EXPECT_EQ(cache.PoolSizePages(), 4u);
}

TEST_F(VTPageCacheTest, Allocate_Single) {
    PageId page{0, 0, 0, 0};
    auto* pp = cache.Allocate(page, 1);
    ASSERT_NE(pp, nullptr);
    EXPECT_TRUE(pp->occupied);
    EXPECT_EQ(pp->resident_page, page);
    EXPECT_EQ(cache.OccupiedCount(), 1u);
}

TEST_F(VTPageCacheTest, Contains_AfterAllocate) {
    PageId page{1, 2, 0, 0};
    EXPECT_FALSE(cache.Contains(page));
    cache.Allocate(page, 1);
    EXPECT_TRUE(cache.Contains(page));
}

TEST_F(VTPageCacheTest, Allocate_DuplicateReturnsExisting) {
    PageId page{3, 4, 1, 0};
    auto* first = cache.Allocate(page, 1);
    auto* second = cache.Allocate(page, 2);
    EXPECT_EQ(first, second);
    EXPECT_EQ(cache.OccupiedCount(), 1u);
    EXPECT_EQ(second->last_used_frame, 2u);  // Frame updated
}

TEST_F(VTPageCacheTest, Allocate_FillPool) {
    // 池大小 4×4 = 16 页
    for (uint32_t i = 0; i < 16; ++i) {
        PageId page{i, 0, 0, 0};
        auto* pp = cache.Allocate(page, i);
        EXPECT_NE(pp, nullptr);
    }
    EXPECT_EQ(cache.OccupiedCount(), 16u);
}

TEST_F(VTPageCacheTest, Allocate_LRU_Eviction) {
    // 填满 16 页
    for (uint32_t i = 0; i < 16; ++i) {
        PageId page{i, 0, 0, 0};
        cache.Allocate(page, i);
    }

    // 分配第 17 个应驱逐 frame=0 的页
    PageId new_page{99, 99, 0, 0};
    auto* pp = cache.Allocate(new_page, 100);
    EXPECT_NE(pp, nullptr);
    EXPECT_EQ(pp->resident_page, new_page);
    EXPECT_EQ(cache.OccupiedCount(), 16u);  // 仍然是满的

    // 被驱逐的页不再存在
    PageId evicted{0, 0, 0, 0};
    EXPECT_FALSE(cache.Contains(evicted));
    EXPECT_TRUE(cache.Contains(new_page));
}

TEST_F(VTPageCacheTest, Touch_UpdatesFrame) {
    PageId page{5, 5, 0, 0};
    cache.Allocate(page, 1);
    cache.Touch(page, 99);

    const auto* pp = cache.GetPhysicalPage(page);
    ASSERT_NE(pp, nullptr);
    EXPECT_EQ(pp->last_used_frame, 99u);
}

TEST_F(VTPageCacheTest, Clear_ResetsAll) {
    for (uint32_t i = 0; i < 8; ++i) {
        cache.Allocate(PageId{i, 0, 0, 0}, i);
    }
    EXPECT_EQ(cache.OccupiedCount(), 8u);

    cache.Clear();
    EXPECT_EQ(cache.OccupiedCount(), 0u);
}

TEST_F(VTPageCacheTest, GetPhysicalPage_NotFound) {
    PageId page{42, 42, 0, 0};
    EXPECT_EQ(cache.GetPhysicalPage(page), nullptr);
}

// ─── PageId 哈希和比较 ──────────────────────────────────────────────────────

TEST(PageIdTest, Equality) {
    PageId a{1, 2, 3, 0};
    PageId b{1, 2, 3, 0};
    PageId c{1, 2, 4, 0};
    EXPECT_EQ(a, b);
    EXPECT_FALSE(a == c);
}

TEST(PageIdTest, Hash_Distinct) {
    PageIdHash hasher;
    std::unordered_set<size_t> hashes;
    // 确保不同页的哈希大多不同
    for (uint32_t x = 0; x < 8; ++x) {
        for (uint32_t y = 0; y < 8; ++y) {
            hashes.insert(hasher(PageId{x, y, 0, 0}));
        }
    }
    EXPECT_GT(hashes.size(), 50u);  // 64 个页至少 50 个不同哈希
}

// ─── VirtualTextureSystem 测试 ──────────────────────────────────────────────

class VTSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        VirtualTextureConfig config;
        config.virtual_size = 1024;  // 小尺寸便于测试
        config.pool_size_pages = 4;  // 4×4 = 16 页
        config.max_uploads_per_frame = 4;
        system.Init(config, nullptr);
    }

    VirtualTextureSystem system;
};

TEST_F(VTSystemTest, Init_PageTableSize) {
    // 1024 / 128 = 8
    EXPECT_EQ(system.PageTableSize(), 8u);
}

TEST_F(VTSystemTest, Init_PhysicalAtlasSize) {
    // 4 * 136 = 544
    EXPECT_EQ(system.PhysicalAtlasSize(), 544u);
}

TEST_F(VTSystemTest, CacheHitRate_Initially100) {
    // 无请求时默认 100%
    EXPECT_FLOAT_EQ(system.CacheHitRate(), 1.0f);
}

TEST_F(VTSystemTest, SubmitFeedback_ProcessesMisses) {
    std::vector<FeedbackEntry> entries;
    entries.push_back({0, 0, 0, 0});
    entries.push_back({1, 0, 0, 0});
    entries.push_back({0, 1, 0, 0});

    system.SubmitFeedback(entries);
    system.Update(1);

    // 所有三个是 misses（不在缓存中）
    EXPECT_LT(system.CacheHitRate(), 1.0f);
}

TEST_F(VTSystemTest, SubmitFeedback_WithLoader_PagesGetCached) {
    // 注册一个简单的 page loader
    system.SetPageLoadCallback([](const PageId& page, std::vector<uint8_t>& out_pixels) {
        out_pixels.resize(kPageSizeWithBorder * kPageSizeWithBorder * 4, 128);
        return true;
    });

    std::vector<FeedbackEntry> entries;
    entries.push_back({2, 3, 0, 0});
    system.SubmitFeedback(entries);
    system.Update(1);

    // 页面应该被缓存了
    EXPECT_TRUE(system.GetCache().Contains(PageId{2, 3, 0, 0}));
}

TEST_F(VTSystemTest, Shutdown_ClearsState) {
    system.Shutdown();
    EXPECT_TRUE(system.GetPageTable().empty());
}

// ─── 配置默认值测试 ──────────────────────────────────────────────────────

TEST(VirtualTextureConfigTest, DefaultValues) {
    VirtualTextureConfig config;
    EXPECT_EQ(config.virtual_size, 16384u);
    EXPECT_EQ(config.pool_size_pages, 64u);
    EXPECT_EQ(config.feedback_scale, 8u);
    EXPECT_EQ(config.max_uploads_per_frame, 8u);
}

TEST(VirtualTextureComponentTest, DefaultValues) {
    VirtualTextureComponent comp;
    EXPECT_TRUE(comp.enabled);
    EXPECT_EQ(comp.vt_id, 0u);
    EXPECT_EQ(comp.virtual_width, 16384u);
    EXPECT_EQ(comp.virtual_height, 16384u);
    EXPECT_FLOAT_EQ(comp.mip_bias, 0.0f);
}

// ─── 常量验证 ──────────────────────────────────────────────────────────────

TEST(VTConstantsTest, PageSizeWithBorder) {
    EXPECT_EQ(kPageSizeWithBorder, kPageSize + kPageBorder * 2);
    EXPECT_EQ(kPageSizeWithBorder, 136u);
}

TEST(VTConstantsTest, PageSize) {
    EXPECT_EQ(kPageSize, 128u);
}
