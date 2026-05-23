/**
 * @file asset_scanner_test.cpp
 * @brief AssetScanner 场景文件资产路径扫描测试
 */

#include <gtest/gtest.h>
#include "engine/assets/asset_scanner.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class AssetScannerTest : public ::testing::Test {
protected:
    void SetUp() override {
        tmp_dir_ = fs::temp_directory_path() / "dse_scanner_test";
        fs::create_directories(tmp_dir_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(tmp_dir_, ec);
    }

    std::string WriteScene(const std::string& filename, const std::string& json) {
        auto path = tmp_dir_ / filename;
        std::ofstream out(path.string());
        out << json;
        return path.string();
    }

    fs::path tmp_dir_;
};

TEST_F(AssetScannerTest, ScanEntitiesWithComponents) {
    std::string scene = WriteScene("test.dscene", R"({
        "entities": [
            {
                "components": {
                    "ScriptComponent": { "script_path": "scripts/player.lua" },
                    "MeshRendererComponent": { "mesh_path": "models/cube.dmesh" }
                }
            },
            {
                "components": {
                    "AudioSourceComponent": { "clip_path": "audio/bgm.wav" }
                }
            }
        ]
    })");

    auto paths = dse::pak::ScanSceneAssetPaths(scene);
    EXPECT_GE(paths.size(), 3u);

    auto contains = [&](const std::string& p) {
        return std::find(paths.begin(), paths.end(), p) != paths.end();
    };
    EXPECT_TRUE(contains("scripts/player.lua"));
    EXPECT_TRUE(contains("models/cube.dmesh"));
    EXPECT_TRUE(contains("audio/bgm.wav"));
    EXPECT_TRUE(contains("test.dscene"));
}

TEST_F(AssetScannerTest, ScanEmptyScene) {
    std::string scene = WriteScene("empty.dscene", R"({"entities": []})");
    auto paths = dse::pak::ScanSceneAssetPaths(scene);
    EXPECT_EQ(paths.size(), 1u);
    EXPECT_EQ(paths[0], "empty.dscene");
}

TEST_F(AssetScannerTest, ScanAnimator3DBlendNodes) {
    std::string scene = WriteScene("anim.dscene", R"({
        "entities": [
            {
                "components": {
                    "Animator3DComponent": {
                        "dskel_path": "models/char.dskel",
                        "danim_path": "anims/idle.danim",
                        "blend_nodes": [
                            { "danim_path": "anims/walk.danim" },
                            { "danim_path": "anims/run.danim" }
                        ]
                    }
                }
            }
        ]
    })");

    auto paths = dse::pak::ScanSceneAssetPaths(scene);
    auto contains = [&](const std::string& p) {
        return std::find(paths.begin(), paths.end(), p) != paths.end();
    };
    EXPECT_TRUE(contains("models/char.dskel"));
    EXPECT_TRUE(contains("anims/idle.danim"));
    EXPECT_TRUE(contains("anims/walk.danim"));
    EXPECT_TRUE(contains("anims/run.danim"));
}

TEST_F(AssetScannerTest, InvalidFile) {
    auto paths = dse::pak::ScanSceneAssetPaths("nonexistent.dscene");
    EXPECT_TRUE(paths.empty());
}

TEST_F(AssetScannerTest, MalformedJSON) {
    std::string scene = WriteScene("bad.dscene", "not json at all {{{");
    auto paths = dse::pak::ScanSceneAssetPaths(scene);
    EXPECT_TRUE(paths.empty());
}

TEST_F(AssetScannerTest, CollectDirectoryFiles) {
    fs::create_directories(tmp_dir_ / "sub");
    {
        std::ofstream(tmp_dir_ / "a.txt") << "a";
        std::ofstream(tmp_dir_ / "sub" / "b.txt") << "b";
    }

    auto files = dse::pak::CollectDirectoryFiles(tmp_dir_.string());
    EXPECT_GE(files.size(), 2u);
}
