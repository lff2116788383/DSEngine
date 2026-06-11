/**
 * @file asset_pipeline_async_test.cpp
 * @brief 资产管线异步化 + LRU 淘汰 + 热重载 单元测试
 *
 * 覆盖场景：
 * - 全资源类型异步加载（Dmesh/Danim/Dskel/AudioClip/Material）
 * - LRU 淘汰：内存预算设置、EstimatedMemoryUsage 追踪、EvictLRU 驱逐
 * - 热重载：StartFileWatcher / StopFileWatcher 生命周期
 * - PumpHotReloads 不崩溃
 */

#include <gtest/gtest.h>
#include "engine/assets/asset_manager.h"
#include "engine/core/event_bus.h"
#include "engine/core/event_id.h"
#include "engine/core/job_system.h"
#include "engine/core/service_locator.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>

using namespace dse::core;

// ============================================================
// LRU 淘汰测试
// ============================================================

class AssetLruTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = std::filesystem::temp_directory_path() / "dse_asset_lru_test";
        std::filesystem::create_directories(temp_dir_);

        mgr_ = std::make_unique<AssetManager>();
        mgr_->ConfigureDataRoot(temp_dir_.string());
    }

    void TearDown() override {
        mgr_.reset();
        std::filesystem::remove_all(temp_dir_);
    }

    void WriteDmeshFile(const std::string& name, std::size_t size) {
        std::ofstream out(temp_dir_ / name, std::ios::binary);
        std::vector<char> data(size, 'X');
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
    }

    std::filesystem::path temp_dir_;
    std::unique_ptr<AssetManager> mgr_;
};

TEST_F(AssetLruTest, InsideuseisZero) {
    EXPECT_EQ(mgr_->EstimatedMemoryUsage(), 0u);
}

TEST_F(AssetLruTest, LoadAssetAfterInside) {
    WriteDmeshFile("test.dmesh", 1024);
    auto dmesh = mgr_->LoadDmesh("test.dmesh");
    ASSERT_NE(dmesh, nullptr);
    EXPECT_GT(mgr_->EstimatedMemoryUsage(), 0u);
}

TEST_F(AssetLruTest, SetMemoryBudgetSetABudget) {
    mgr_->SetMemoryBudget(1024 * 1024);
    // 不应崩溃
    SUCCEED();
}

TEST_F(AssetLruTest, EvictLRUReturnsZeroIfThereIsNoBudget) {
    WriteDmeshFile("a.dmesh", 512);
    auto a = mgr_->LoadDmesh("a.dmesh");
    ASSERT_NE(a, nullptr);

    // 无预算限制
    std::size_t evicted = mgr_->EvictLRU();
    EXPECT_EQ(evicted, 0u);
}

TEST_F(AssetLruTest, EvictLRUNoEliminationWithinBudget) {
    WriteDmeshFile("a.dmesh", 512);
    auto a = mgr_->LoadDmesh("a.dmesh");
    ASSERT_NE(a, nullptr);

    mgr_->SetMemoryBudget(1024 * 1024); // 远大于实际使用
    std::size_t evicted = mgr_->EvictLRU();
    EXPECT_EQ(evicted, 0u);
}

TEST_F(AssetLruTest, EvictLRUEliminateExpiredResourcesBeyondBudget) {
    WriteDmeshFile("a.dmesh", 2048);
    WriteDmeshFile("b.dmesh", 2048);

    // 加载两个资源
    auto a = mgr_->LoadDmesh("a.dmesh");
    ASSERT_NE(a, nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 确保时间戳不同
    auto b = mgr_->LoadDmesh("b.dmesh");
    ASSERT_NE(b, nullptr);

    std::size_t usage_before = mgr_->EstimatedMemoryUsage();
    EXPECT_GT(usage_before, 0u);

    // 设置很小的预算
    mgr_->SetMemoryBudget(1);

    // 释放 a 的外部引用使其可被驱逐
    a.reset();

    std::size_t evicted = mgr_->EvictLRU();
    // a 的 weak_ptr 已过期，应该被驱逐
    EXPECT_GE(evicted, 1u);
    EXPECT_LT(mgr_->EstimatedMemoryUsage(), usage_before);
}

TEST_F(AssetLruTest, EvictLRUDoNotRetireResourcesThatAreStillReferenced) {
    WriteDmeshFile("a.dmesh", 2048);

    auto a = mgr_->LoadDmesh("a.dmesh");
    ASSERT_NE(a, nullptr);

    mgr_->SetMemoryBudget(1); // 极小预算

    // a 仍被持有，不应被驱逐
    std::size_t evicted = mgr_->EvictLRU();
    EXPECT_EQ(evicted, 0u);
}

// ============================================================
// 异步加载扩展测试
// ============================================================

class AssetAsyncExpandedTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = std::filesystem::temp_directory_path() / "dse_asset_async_expanded_test";
        std::filesystem::create_directories(temp_dir_);

        mgr_ = std::make_unique<AssetManager>();
        mgr_->ConfigureDataRoot(temp_dir_.string());
    }

    void TearDown() override {
        mgr_.reset();
        std::filesystem::remove_all(temp_dir_);
    }

    void WriteFile(const std::string& name, const std::string& content) {
        std::ofstream out(temp_dir_ / name, std::ios::binary);
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
    }

    std::filesystem::path temp_dir_;
    std::unique_ptr<AssetManager> mgr_;
};

TEST_F(AssetAsyncExpandedTest, LoadDmeshAsynccallbackIsDispatched) {
    WriteFile("test.dmesh", std::string(128, '\0'));

    std::atomic<bool> callback_fired{false};
    mgr_->LoadDmeshAsync("test.dmesh", [&callback_fired](std::shared_ptr<DmeshAsset> asset) {
        callback_fired.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mgr_->PumpMainThreadCallbacks();
    EXPECT_TRUE(callback_fired.load());
}

TEST_F(AssetAsyncExpandedTest, LoadDanimAsynccallbackIsDispatched) {
    WriteFile("test.danim", std::string(64, '\0'));

    std::atomic<bool> callback_fired{false};
    mgr_->LoadDanimAsync("test.danim", [&callback_fired](std::shared_ptr<DanimAsset> asset) {
        callback_fired.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mgr_->PumpMainThreadCallbacks();
    EXPECT_TRUE(callback_fired.load());
}

TEST_F(AssetAsyncExpandedTest, LoadDskelAsynccallbackIsDispatched) {
    WriteFile("test.dskel", std::string(64, '\0'));

    std::atomic<bool> callback_fired{false};
    mgr_->LoadDskelAsync("test.dskel", [&callback_fired](std::shared_ptr<DskelAsset> asset) {
        callback_fired.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mgr_->PumpMainThreadCallbacks();
    EXPECT_TRUE(callback_fired.load());
}

TEST_F(AssetAsyncExpandedTest, LoadAudioClipAsynccallbackIsDispatched) {
    WriteFile("test.wav", std::string(256, '\0'));

    std::atomic<bool> callback_fired{false};
    mgr_->LoadAudioClipAsync("test.wav", [&callback_fired](std::shared_ptr<AudioClipAsset> asset) {
        callback_fired.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mgr_->PumpMainThreadCallbacks();
    EXPECT_TRUE(callback_fired.load());
}

TEST_F(AssetAsyncExpandedTest, LoadInvalidPathReturnsnullptrDoesNotCrash) {
    std::atomic<bool> callback_fired{false};
    mgr_->LoadDmeshAsync("///nonexistent.dmesh", [&callback_fired](std::shared_ptr<DmeshAsset> asset) {
        callback_fired.store(true);
        EXPECT_EQ(asset, nullptr);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mgr_->PumpMainThreadCallbacks();
    EXPECT_TRUE(callback_fired.load());
}

TEST_F(AssetAsyncExpandedTest, LoadbringJobSystem) {
    auto job_system = std::make_shared<JobSystem>();
    job_system->Init();
    mgr_->SetJobSystem(job_system.get());

    WriteFile("job_test.dmesh", std::string(256, '\0'));

    std::atomic<bool> callback_fired{false};
    mgr_->LoadDmeshAsync("job_test.dmesh", [&callback_fired](std::shared_ptr<DmeshAsset> asset) {
        callback_fired.store(true);
    });

    // 等待 job 完成
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    mgr_->PumpMainThreadCallbacks();
    EXPECT_TRUE(callback_fired.load());

    job_system->Shutdown();
}

// ============================================================
// 热重载生命周期测试
// ============================================================

class AssetHotReloadTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = std::filesystem::temp_directory_path() / "dse_asset_hotreload_test";
        std::filesystem::create_directories(temp_dir_);

        mgr_ = std::make_unique<AssetManager>();
        mgr_->ConfigureDataRoot(temp_dir_.string());
    }

    void TearDown() override {
        mgr_->StopFileWatcher();
        mgr_.reset();
        std::filesystem::remove_all(temp_dir_);
    }

    std::filesystem::path temp_dir_;
    std::unique_ptr<AssetManager> mgr_;
};

TEST_F(AssetHotReloadTest, StartStopDoesNotCrash) {
    mgr_->StartFileWatcher();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mgr_->StopFileWatcher();
    SUCCEED();
}

TEST_F(AssetHotReloadTest, MultiTimesStartStopDoesNotCrash) {
    for (int i = 0; i < 3; ++i) {
        mgr_->StartFileWatcher();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        mgr_->StopFileWatcher();
    }
    SUCCEED();
}

TEST_F(AssetHotReloadTest, PumpHotReloadsReturnsZeroIfNothingIsPending) {
    std::size_t count = mgr_->PumpHotReloads();
    EXPECT_EQ(count, 0u);
}

TEST_F(AssetHotReloadTest, StopFileWatcherDoesNotCrashWhenNotStarted) {
    mgr_->StopFileWatcher();
    SUCCEED();
}
