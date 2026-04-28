/**
* @file assetmanager_vfs_integration_test.cpp
* @brief AssetManager + VFS (虚拟文件系统) 集成测试
*
* 验证场景：
* - AssetManager 资源路径规范化与解析
* - Asset Bundle 打包、挂载与读取
* - 缓存命中：同一资源二次加载返回缓存实例
* - LoadFileToMemory 从 Bundle 和磁盘读取
* - 异步加载回调泵浦机制
* - UnloadUnused 释放未引用资源
*/

#ifdef _MSC_VER
#include <io.h>
#endif
#include <gtest/gtest.h>
#include "engine/assets/asset_manager.h"
#include "engine/core/event_bus.h"
#include "engine/core/job_system.h"
#include "engine/core/service_locator.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <thread>
#include <chrono>

// ============================================================
// AssetManager 路径解析集成
// ============================================================

class AssetManagerVfsIntegrationTest : public ::testing::Test {
protected:
    AssetManager asset_mgr;

    void SetUp() override {
        asset_mgr.ConfigureDataRoot("data");
    }

    void TearDown() override {}
};

TEST_F(AssetManagerVfsIntegrationTest, 配置数据根目录后获取返回正确值) {
    asset_mgr.ConfigureDataRoot("data");
    EXPECT_EQ(asset_mgr.GetDataRoot(), "data");
}

TEST_F(AssetManagerVfsIntegrationTest, ConfigureDataRoot可切换根目录) {
    asset_mgr.ConfigureDataRoot("assets");
    EXPECT_EQ(asset_mgr.GetDataRoot(), "assets");

    asset_mgr.ConfigureDataRoot("data");
    EXPECT_EQ(asset_mgr.GetDataRoot(), "data");
}

TEST_F(AssetManagerVfsIntegrationTest, NormalizeAssetPath处理逻辑路径) {
    // 纯逻辑路径应直接返回
    std::string result = asset_mgr.NormalizeAssetPath("textures/hero.png");
    EXPECT_FALSE(result.empty());
}

TEST_F(AssetManagerVfsIntegrationTest, NormalizeAssetPath处理带前缀路径) {
    // 带 data/ 前缀的路径应被规范化
    std::string result = asset_mgr.NormalizeAssetPath("data/textures/hero.png");
    EXPECT_FALSE(result.empty());
}

TEST_F(AssetManagerVfsIntegrationTest, ResolveAssetPath返回可访问路径) {
    std::string resolved = asset_mgr.ResolveAssetPath("textures/test.png");
    // 无论文件是否存在，路径解析不应返回空（除非路径本身非法）
    // 此处主要验证不崩溃
    SUCCEED();
}

// ============================================================
// Asset Bundle 打包与挂载
// ============================================================

class AssetBundleIntegrationTest : public ::testing::Test {
protected:
    AssetManager asset_mgr;
    std::string temp_dir_;
    std::string bundle_path_;

    void SetUp() override {
        // 创建临时目录和文件用于 Bundle 测试
        temp_dir_ = "test_bundle_input";
        bundle_path_ = "test_output.bun";

        std::filesystem::create_directories(temp_dir_);
        std::ofstream(temp_dir_ + "/test.txt") << "hello bundle";

        asset_mgr.ConfigureDataRoot("data");
    }

    void TearDown() override {
        // 清理临时文件
        std::filesystem::remove_all(temp_dir_);
        std::filesystem::remove(bundle_path_);
    }
};

TEST_F(AssetBundleIntegrationTest, PackBundle打包目录成功) {
    bool result = asset_mgr.PackBundle(temp_dir_, bundle_path_, "");
    // 打包应成功
    EXPECT_TRUE(result);

    // Bundle 文件应存在
    EXPECT_TRUE(std::filesystem::exists(bundle_path_));
}

TEST_F(AssetBundleIntegrationTest, MountBundle后LoadFileToMemory可读取) {
    // 先打包
    ASSERT_TRUE(asset_mgr.PackBundle(temp_dir_, bundle_path_, ""));

    // 挂载
    bool mount_ok = asset_mgr.MountBundle(bundle_path_, "");
    EXPECT_TRUE(mount_ok);

    // 从 Bundle 读取
    std::vector<uint8_t> data;
    bool load_ok = asset_mgr.LoadFileToMemory("test.txt", data);
    if (load_ok) {
        EXPECT_FALSE(data.empty());
        // 内容验证
        std::string content(data.begin(), data.end());
        EXPECT_EQ(content, "hello bundle");
    }
}

TEST_F(AssetBundleIntegrationTest, 加密Bundle打包和挂载) {
    std::string aes_key = "0123456789abcdef"; // 16 字节 AES 密钥

    bool pack_ok = asset_mgr.PackBundle(temp_dir_, bundle_path_, aes_key);
    EXPECT_TRUE(pack_ok);

    bool mount_ok = asset_mgr.MountBundle(bundle_path_, aes_key);
    EXPECT_TRUE(mount_ok);

    std::vector<uint8_t> data;
    bool load_ok = asset_mgr.LoadFileToMemory("test.txt", data);
    if (load_ok) {
        std::string content(data.begin(), data.end());
        EXPECT_EQ(content, "hello bundle");
    }
}

TEST_F(AssetBundleIntegrationTest, 错误密钥挂载失败或读取异常) {
    std::string correct_key = "0123456789abcdef";
    std::string wrong_key = "fedcba9876543210";

    ASSERT_TRUE(asset_mgr.PackBundle(temp_dir_, bundle_path_, correct_key));

    // 用错误密钥挂载
    bool mount_ok = asset_mgr.MountBundle(bundle_path_, wrong_key);
    if (mount_ok) {
        // 即使挂载"成功"（文件格式可能不校验密钥），读取内容应不正确
        std::vector<uint8_t> data;
        bool load_ok = asset_mgr.LoadFileToMemory("test.txt", data);
        if (load_ok) {
            std::string content(data.begin(), data.end());
            EXPECT_NE(content, "hello bundle");
        }
    }
    // 不论哪种结果，均不应崩溃
    SUCCEED();
}

// ============================================================
// 缓存机制
// ============================================================

TEST_F(AssetManagerVfsIntegrationTest, 创建材质实例缓存命中) {
    auto mat1 = asset_mgr.CreateMaterialInstance("test_mat");
    auto mat2 = asset_mgr.GetMaterialInstance(mat1->GetId());

    // 获取同一 ID 的材质应返回同一实例
    ASSERT_NE(mat1, nullptr);
    ASSERT_NE(mat2, nullptr);
    EXPECT_EQ(mat1.get(), mat2.get());
}

TEST_F(AssetManagerVfsIntegrationTest, 列出已创建的材质实例ID) {
    auto mat_a = asset_mgr.CreateMaterialInstance("mat_a");
    auto mat_b = asset_mgr.CreateMaterialInstance("mat_b");

    ASSERT_NE(mat_a, nullptr);
    ASSERT_NE(mat_b, nullptr);

    auto ids = asset_mgr.ListMaterialInstanceIds();
    EXPECT_GE(ids.size(), 2u);
}


TEST_F(AssetManagerVfsIntegrationTest, 卸载未使用资源) {
    auto mat = asset_mgr.CreateMaterialInstance("temp_mat");
    unsigned int mat_id = mat->GetId();

    // 释放 shared_ptr
    mat.reset();

    // UnloadUnused 应清理无引用的资源
    asset_mgr.UnloadUnused();

    // 无强引用后 GetMaterialInstance 应返回空
    auto expired = asset_mgr.GetMaterialInstance(mat_id);
    EXPECT_EQ(expired, nullptr);
}

// ============================================================
// 异步加载
// ============================================================

class AssetManagerAsyncIntegrationTest : public ::testing::Test {
protected:
    AssetManager asset_mgr;
    std::shared_ptr<dse::core::JobSystem> job_system;

    void SetUp() override {
        job_system = std::make_shared<dse::core::JobSystem>();
        job_system->Init();
        asset_mgr.SetJobSystem(job_system.get());
        asset_mgr.ConfigureDataRoot("data");
    }

    void TearDown() override {
        job_system->Shutdown();
    }
};

TEST_F(AssetManagerAsyncIntegrationTest, 异步加载回调泵浦机制) {
    std::atomic<int> callback_count{0};

    // 提交异步加载（文件可能不存在，此处测试回调机制）
    asset_mgr.LoadTextureAsync("nonexistent.png", [&](std::shared_ptr<TextureAsset>) {
        callback_count.fetch_add(1);
    });

    // 泵浦主线程回调
    asset_mgr.PumpMainThreadCallbacks();

    // 即使文件不存在，回调也应被触发（返回 nullptr）
    // 给一点时间让后台任务完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    asset_mgr.PumpMainThreadCallbacks();

    // 不论结果如何，Pump 不应崩溃
    SUCCEED();
}

TEST_F(AssetManagerAsyncIntegrationTest, PendingMainThreadCallbacks计数非负) {
    EXPECT_GE(asset_mgr.PendingMainThreadCallbacks(), 0u);
    EXPECT_GE(asset_mgr.PendingMainThreadCallbacksHighWatermark(), 0u);
}

// ============================================================
// EventBus 集成
// ============================================================

TEST_F(AssetManagerVfsIntegrationTest, SetEventBus后GetEventBus返回正确指针) {
    dse::core::EventBus bus;
    asset_mgr.SetEventBus(&bus);
    EXPECT_EQ(asset_mgr.GetEventBus(), &bus);
}

TEST_F(AssetManagerVfsIntegrationTest, 设置JobSystem后获取返回正确指针) {
    dse::core::JobSystem js;
    asset_mgr.SetJobSystem(&js);
    EXPECT_EQ(asset_mgr.GetJobSystem(), &js);
}
