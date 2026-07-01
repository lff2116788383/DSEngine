/**
 * @file asset_distribution_test.cpp
 * @brief AssetDistribution + BinaryPatch + SHA256 单元测试
 */

#include <gtest/gtest.h>
#include "engine/assets/asset_distribution.h"
#include "engine/assets/binary_patch.h"
#include "engine/assets/sha256.h"
#include <filesystem>
#include <fstream>
#include <cstring>

namespace fs = std::filesystem;
using namespace dse::assets;

// ── SHA256 测试 ──────────────────────────────────────────────────────────────

TEST(SHA256Test, EmptyString) {
    std::string hash = SHA256::HashToHex("", 0);
    EXPECT_EQ(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(SHA256Test, KnownVector_abc) {
    const char* data = "abc";
    std::string hash = SHA256::HashToHex(data, 3);
    EXPECT_EQ(hash, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(SHA256Test, LongerInput) {
    const char* data = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    std::string hash = SHA256::HashToHex(data, std::strlen(data));
    EXPECT_EQ(hash, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST(SHA256Test, HashFile) {
    fs::path tmp = fs::temp_directory_path() / "dse_sha256_test.bin";
    {
        std::ofstream f(tmp, std::ios::binary);
        f << "hello world";
    }
    std::string hash = SHA256::HashFile(tmp.string());
    EXPECT_FALSE(hash.empty());
    // sha256("hello world") = b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9
    EXPECT_EQ(hash, "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
    fs::remove(tmp);
}

// ── BinaryPatch 测试 ─────────────────────────────────────────────────────────

class BinaryPatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_dir_ = fs::temp_directory_path() / "dse_patch_test";
        fs::create_directories(tmp_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tmp_dir_, ec);
    }

    fs::path tmp_dir_;
};

TEST_F(BinaryPatchTest, IdenticalFiles_NoPatchBlocks) {
    std::vector<uint8_t> data(1024, 0xAB);
    std::vector<uint8_t> patch_out;
    ASSERT_TRUE(BinaryPatch::GenerateFromMemory(
        data.data(), data.size(), data.data(), data.size(), patch_out));

    PatchData pd;
    ASSERT_TRUE(BinaryPatch::Deserialize(patch_out.data(), patch_out.size(), pd));
    EXPECT_EQ(pd.blocks.size(), 0u); // 相同文件无变化块
}

TEST_F(BinaryPatchTest, SingleBlockChange) {
    std::vector<uint8_t> old_data(4096, 0x00);
    std::vector<uint8_t> new_data(4096, 0x00);
    new_data[100] = 0xFF; // 修改一个字节

    std::vector<uint8_t> patch_out;
    ASSERT_TRUE(BinaryPatch::GenerateFromMemory(
        old_data.data(), old_data.size(), new_data.data(), new_data.size(), patch_out));

    PatchData pd;
    ASSERT_TRUE(BinaryPatch::Deserialize(patch_out.data(), patch_out.size(), pd));
    EXPECT_EQ(pd.blocks.size(), 1u);
    EXPECT_EQ(pd.blocks[0].offset, 0u);
}

TEST_F(BinaryPatchTest, ApplyRoundtrip) {
    std::vector<uint8_t> old_data(8192, 0x42);
    std::vector<uint8_t> new_data(8192, 0x42);
    // 修改多个块
    new_data[0] = 0x01;
    new_data[4096] = 0x02;

    std::vector<uint8_t> patch_out;
    ASSERT_TRUE(BinaryPatch::GenerateFromMemory(
        old_data.data(), old_data.size(), new_data.data(), new_data.size(), patch_out));

    std::vector<uint8_t> applied;
    ASSERT_TRUE(BinaryPatch::ApplyFromMemory(
        old_data.data(), old_data.size(), patch_out.data(), patch_out.size(), applied));

    EXPECT_EQ(applied.size(), new_data.size());
    EXPECT_EQ(applied, new_data);
}

TEST_F(BinaryPatchTest, FileRoundtrip) {
    fs::path old_file = tmp_dir_ / "old.bin";
    fs::path new_file = tmp_dir_ / "new.bin";
    fs::path patch_file = tmp_dir_ / "diff.dpatch";
    fs::path output_file = tmp_dir_ / "out.bin";

    std::vector<uint8_t> old_data(16384, 0x33);
    std::vector<uint8_t> new_data(16384, 0x33);
    new_data[1000] = 0xAA;
    new_data[9000] = 0xBB;

    {
        std::ofstream f(old_file, std::ios::binary);
        f.write(reinterpret_cast<const char*>(old_data.data()), old_data.size());
    }
    {
        std::ofstream f(new_file, std::ios::binary);
        f.write(reinterpret_cast<const char*>(new_data.data()), new_data.size());
    }

    ASSERT_TRUE(BinaryPatch::Generate(old_file.string(), new_file.string(), patch_file.string()));
    ASSERT_TRUE(BinaryPatch::Apply(old_file.string(), patch_file.string(), output_file.string()));

    std::ifstream result(output_file, std::ios::binary);
    std::vector<uint8_t> result_data(
        (std::istreambuf_iterator<char>(result)), std::istreambuf_iterator<char>());
    EXPECT_EQ(result_data, new_data);
}

TEST_F(BinaryPatchTest, SizeChange_NewFileLarger) {
    std::vector<uint8_t> old_data(4096, 0x11);
    std::vector<uint8_t> new_data(8192, 0x22);

    std::vector<uint8_t> patch_out;
    ASSERT_TRUE(BinaryPatch::GenerateFromMemory(
        old_data.data(), old_data.size(), new_data.data(), new_data.size(), patch_out));

    std::vector<uint8_t> applied;
    ASSERT_TRUE(BinaryPatch::ApplyFromMemory(
        old_data.data(), old_data.size(), patch_out.data(), patch_out.size(), applied));
    EXPECT_EQ(applied, new_data);
}

// ── AssetDistribution Manifest 测试 ──────────────────────────────────────────

class AssetDistributionPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_dir_ = fs::temp_directory_path() / "dse_dist_test";
        fs::create_directories(tmp_dir_);

        DistributionConfig cfg;
        cfg.cache_path = (tmp_dir_ / "cache").string();
        cfg.cdn_base_url = "https://cdn.example.com/assets";
        cfg.cell_size = 256.0f;
        cfg.max_concurrent_downloads = 4;
        dist_.Init(cfg);
    }

    void TearDown() override {
        dist_.Shutdown();
        std::error_code ec;
        fs::remove_all(tmp_dir_, ec);
    }

    fs::path tmp_dir_;
    AssetDistribution dist_;
};

TEST_F(AssetDistributionPipelineTest, ManifestSaveLoad) {
    // 创建一些测试资产文件
    fs::path asset1 = tmp_dir_ / "texture.dds";
    {
        std::ofstream f(asset1, std::ios::binary);
        std::vector<uint8_t> data(1024, 0xAA);
        f.write(reinterpret_cast<const char*>(data.data()), data.size());
    }

    std::vector<std::string> assets = { asset1.string() };
    dist_.PackageCell(0, 0, 0, assets);
    dist_.PackageCell(1, 0, 0, assets);

    fs::path manifest_path = tmp_dir_ / "manifest.json";
    ASSERT_TRUE(dist_.SaveManifest(manifest_path.string()));
    EXPECT_TRUE(fs::exists(manifest_path));

    // 重新加载
    AssetDistribution dist2;
    DistributionConfig cfg2;
    cfg2.cache_path = (tmp_dir_ / "cache2").string();
    cfg2.cdn_base_url = "https://cdn.example.com/assets";
    dist2.Init(cfg2);

    ASSERT_TRUE(dist2.LoadManifest(manifest_path.string()));
    EXPECT_EQ(dist2.GetManifest().packages.size(), 2u);
    EXPECT_EQ(dist2.GetManifest().packages[0].package_id, "cell_0_0_lod_0");
    EXPECT_EQ(dist2.GetManifest().packages[1].package_id, "cell_1_0_lod_0");
    dist2.Shutdown();
}

TEST_F(AssetDistributionPipelineTest, VerifyPackage_NotInstalled) {
    EXPECT_FALSE(dist_.VerifyPackage("nonexistent"));
}

TEST_F(AssetDistributionPipelineTest, GetRequiredPackages) {
    fs::path asset = tmp_dir_ / "mesh.dse";
    {
        std::ofstream f(asset, std::ios::binary);
        f << "mesh data";
    }
    std::vector<std::string> assets = { asset.string() };

    dist_.PackageCell(0, 0, 0, assets);
    dist_.PackageCell(1, 0, 0, assets);
    dist_.PackageCell(2, 2, 0, assets);

    auto pkgs = dist_.GetRequiredPackages(glm::vec3(128, 0, 128), 256.0f);
    EXPECT_GE(pkgs.size(), 1u);
}

TEST_F(AssetDistributionPipelineTest, PriorityCloserCellsHigher) {
    fs::path asset = tmp_dir_ / "data.bin";
    {
        std::ofstream f(asset, std::ios::binary);
        f << "test data";
    }
    std::vector<std::string> assets = { asset.string() };

    dist_.PackageCell(0, 0, 0, assets);  // center at (128, 128)
    dist_.PackageCell(10, 10, 0, assets); // center at (2688, 2688)

    dist_.UpdatePriorities(glm::vec3(128, 0, 128));
    auto queue = dist_.GetDownloadQueue();
    ASSERT_GE(queue.size(), 2u);
    EXPECT_GT(queue[0].priority, queue[1].priority); // 近的优先级高
}
