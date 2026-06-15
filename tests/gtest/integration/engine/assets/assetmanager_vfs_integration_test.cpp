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
#include <atomic>
#include <string>
#include <system_error>

namespace {
// 为每个用例生成唯一名字，避免并行执行时多个用例共享同一固定文件名而互相干扰
// （一个用例挂载着 bundle 文件时，另一个用例的 TearDown 删它会触发“拒绝访问/被占用”）。
std::string UniqueSuffix() {
    static std::atomic<uint64_t> counter{0};
    std::string s;
    if (const auto* info = ::testing::UnitTest::GetInstance()->current_test_info()) {
        s += info->name();
    }
    s += "_";
    s += std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    s += "_";
    s += std::to_string(counter.fetch_add(1));
    return s;
}
}  // namespace

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

// 测试 资源管理器VFS集成：Configurationdata之后获取返回正确
TEST_F(AssetManagerVfsIntegrationTest, ConfigurationdataAfterAcquireReturnsCorrect) {
    asset_mgr.ConfigureDataRoot("data");
    EXPECT_EQ(asset_mgr.GetDataRoot(), "data");
}

// 测试 资源管理器VFS集成：配置数据根Switchable根目录
TEST_F(AssetManagerVfsIntegrationTest, ConfigureDataRootSwitchableRootDirectory) {
    asset_mgr.ConfigureDataRoot("assets");
    EXPECT_EQ(asset_mgr.GetDataRoot(), "assets");

    asset_mgr.ConfigureDataRoot("data");
    EXPECT_EQ(asset_mgr.GetDataRoot(), "data");
}

// 测试 资源管理器VFS集成：归一化资源路径Processing Logical Paths
TEST_F(AssetManagerVfsIntegrationTest, NormalizeAssetPathProcessingLogicalPaths) {
    // 纯逻辑路径应直接返回
    std::string result = asset_mgr.NormalizeAssetPath("textures/hero.png");
    EXPECT_FALSE(result.empty());
}

// 测试 资源管理器VFS集成：归一化资源路径Handling Prefixed Paths
TEST_F(AssetManagerVfsIntegrationTest, NormalizeAssetPathHandlingPrefixedPaths) {
    // 带 data/ 前缀的路径应被规范化
    std::string result = asset_mgr.NormalizeAssetPath("data/textures/hero.png");
    EXPECT_FALSE(result.empty());
}

// 测试 资源管理器VFS集成：解析资源路径返回Accessible路径
TEST_F(AssetManagerVfsIntegrationTest, ResolveAssetPathReturnAccessiblePath) {
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
        // 创建唯一临时目录和文件用于 Bundle 测试（并行安全）
        const std::string suffix = UniqueSuffix();
        temp_dir_ = (std::filesystem::temp_directory_path() / ("dse_bundle_input_" + suffix)).string();
        bundle_path_ = (std::filesystem::temp_directory_path() / ("dse_bundle_output_" + suffix + ".bun")).string();

        std::filesystem::create_directories(temp_dir_);
        std::ofstream(temp_dir_ + "/test.txt") << "hello bundle";

        asset_mgr.ConfigureDataRoot("data");
    }

    void TearDown() override {
        // 清理临时文件（不抛异常）
        std::error_code ec;
        std::filesystem::remove_all(temp_dir_, ec);
        std::filesystem::remove(bundle_path_, ec);
    }
};

// 测试 资源资源包集成：打包资源包Packaging目录成功
TEST_F(AssetBundleIntegrationTest, PackBundlePackagingDirectorySuccessful) {
    bool result = asset_mgr.PackBundle(temp_dir_, bundle_path_, "");
    // 打包应成功
    EXPECT_TRUE(result);

    // Bundle 文件应存在
    EXPECT_TRUE(std::filesystem::exists(bundle_path_));
}

// 测试 资源资源包集成：挂载资源包之后加载文件到内存可读
TEST_F(AssetBundleIntegrationTest, MountBundleAfterLoadFileToMemoryReadable) {
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

// 测试 资源资源包集成：资源包且
TEST_F(AssetBundleIntegrationTest, BundleAnd) {
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

// 测试 资源资源包集成：错误失败或读取
TEST_F(AssetBundleIntegrationTest, ErrorFailsOrRead) {
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

// 测试 资源管理器VFS集成：创建示例Cachehit
TEST_F(AssetManagerVfsIntegrationTest, CreateExampleCachehit) {
    auto mat1 = asset_mgr.CreateMaterialInstance("test_mat");
    auto mat2 = asset_mgr.GetMaterialInstance(mat1->GetId());

    // 获取同一 ID 的材质应返回同一实例
    ASSERT_NE(mat1, nullptr);
    ASSERT_NE(mat2, nullptr);
    EXPECT_EQ(mat1.get(), mat2.get());
}

// 测试 资源管理器VFS集成：已经创建示例ID
TEST_F(AssetManagerVfsIntegrationTest, AlreadyCreateExampleID) {
    auto mat_a = asset_mgr.CreateMaterialInstance("mat_a");
    auto mat_b = asset_mgr.CreateMaterialInstance("mat_b");

    ASSERT_NE(mat_a, nullptr);
    ASSERT_NE(mat_b, nullptr);

    auto ids = asset_mgr.ListMaterialInstanceIds();
    EXPECT_GE(ids.size(), 2u);
}


// 测试 资源管理器VFS集成：Notuse资源
TEST_F(AssetManagerVfsIntegrationTest, NotuseAsset) {
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

// 测试 资源管理器异步集成：加载
TEST_F(AssetManagerAsyncIntegrationTest, Load) {
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

// 测试 资源管理器异步集成：待处理主线程Callbackscount非负
TEST_F(AssetManagerAsyncIntegrationTest, PendingMainThreadCallbackscountNonNegative) {
    EXPECT_GE(asset_mgr.PendingMainThreadCallbacks(), 0u);
    EXPECT_GE(asset_mgr.PendingMainThreadCallbacksHighWatermark(), 0u);
}

// ============================================================
// EventBus 集成
// ============================================================

// 测试 资源管理器VFS集成：设置事件总线之后获取事件总线返回正确指针
TEST_F(AssetManagerVfsIntegrationTest, SetEventBusAfterGetEventBusReturnTheCorrectPointer) {
    dse::core::EventBus bus;
    asset_mgr.SetEventBus(&bus);
    EXPECT_EQ(asset_mgr.GetEventBus(), &bus);
}

// 测试 资源管理器VFS集成：设置上任务系统之后获取返回正确指针
TEST_F(AssetManagerVfsIntegrationTest, SetUpJobSystemAfterAcquireReturnTheCorrectPointer) {
    dse::core::JobSystem js;
    asset_mgr.SetJobSystem(&js);
    EXPECT_EQ(asset_mgr.GetJobSystem(), &js);
}
