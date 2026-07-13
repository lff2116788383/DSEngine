/**
 * @file editor_release_matrix_test.cpp
 * @brief P2-3 Final release matrix validation.
 *
 * Verifies that all required source files, APIs, and infrastructure
 * are present and structurally valid for a production release.
 *
 * The test working directory is set to CMAKE_SOURCE_DIR by CTest.
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {
std::string ReadFileContent(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return {};
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    return content;
}
} // namespace

// ── Infrastructure File Presence ────────────────────────────────

TEST(ReleaseMatrixTest, FeatureLedgerExists) {
    EXPECT_TRUE(fs::exists("tools/audit/feature_ledger.json"));
}

TEST(ReleaseMatrixTest, EditorCommandVocabularyComplete) {
    auto content = ReadFileContent("apps/editor_cpp/core/editor_command.h");
    ASSERT_FALSE(content.empty());

    const std::vector<std::string> required_cmds = {
        "CreateEntityCmd", "DeleteEntityCmd", "RenameEntityCmd",
        "TransformEntityCmd", "ReparentEntityCmd", "DuplicateEntityCmd",
        "AddComponentCmd", "RemoveComponentCmd", "SetSelectionCmd",
        "NewSceneCmd", "SaveSceneCmd", "LoadSceneCmd",
        "ImportAssetCmd", "CreateMaterialCmd", "SavePrefabCmd",
        "InstantiatePrefabCmd", "PlayCmd", "StopCmd",
        "UndoCmd", "RedoCmd",
    };

    for (const auto& cmd : required_cmds) {
        EXPECT_NE(content.find(cmd), std::string::npos)
            << "Missing required command type: " << cmd;
    }
}

TEST(ReleaseMatrixTest, UndoSystemAPIComplete) {
    auto content = ReadFileContent("apps/editor_cpp/src/editor_undo.h");
    ASSERT_FALSE(content.empty());

    const std::vector<std::string> required_api = {
        "ICommand", "PropertyChangeCommand", "LambdaCommand",
        "CompoundCommand", "UndoRedoManager",
        "Execute", "Undo", "Redo", "CanUndo", "CanRedo",
        "Clear", "GetUndoHistory", "GetRedoHistory", "MergeWith",
    };

    for (const auto& api : required_api) {
        EXPECT_NE(content.find(api), std::string::npos)
            << "Missing required undo API: " << api;
    }
}

TEST(ReleaseMatrixTest, BuildServiceAPIComplete) {
    auto content = ReadFileContent("engine/project/build_service.h");
    ASSERT_FALSE(content.empty());

    const std::vector<std::string> required_api = {
        "RunGameBuild", "BuildOptions", "BuildResult",
        "ValidateBuildOptions", "FindEngineRoot", "FindRuntimeExe",
    };

    for (const auto& api : required_api) {
        EXPECT_NE(content.find(api), std::string::npos)
            << "Missing required BuildService API: " << api;
    }
}

TEST(ReleaseMatrixTest, CrashHandlerAPIComplete) {
    auto content = ReadFileContent("engine/diagnostics/crash_handler.h");
    ASSERT_FALSE(content.empty());

    const std::vector<std::string> required_api = {"Install", "CrashReport"};

    for (const auto& api : required_api) {
        EXPECT_NE(content.find(api), std::string::npos)
            << "Missing required CrashHandler API: " << api;
    }
}

TEST(ReleaseMatrixTest, GitClientAPIComplete) {
    auto content = ReadFileContent("engine/vcs/git_client.h");
    ASSERT_FALSE(content.empty());

    const std::vector<std::string> required_api = {
        "GitClient", "Status", "Commit", "Push", "Pull", "Fetch", "Merge",
    };

    for (const auto& api : required_api) {
        EXPECT_NE(content.find(api), std::string::npos)
            << "Missing required GitClient API: " << api;
    }
}

TEST(ReleaseMatrixTest, AssetDatabaseAPIComplete) {
    auto content = ReadFileContent("apps/editor_cpp/src/editor_asset_db.h");
    ASSERT_FALSE(content.empty());

    const std::vector<std::string> required_api = {
        "AssetDatabase", "FindByGUID", "FindByPath",
        "Refresh", "AssetInfo", "AssetType",
    };

    for (const auto& api : required_api) {
        EXPECT_NE(content.find(api), std::string::npos)
            << "Missing required AssetDatabase API: " << api;
    }
}

// ── 2D Tools Panel Coverage ─────────────────────────────────────

TEST(ReleaseMatrixTest, TwoDToolsAPIComplete) {
    auto content = ReadFileContent("apps/editor_cpp/src/editor_2d_tools.h");
    ASSERT_FALSE(content.empty());

    const std::vector<std::string> required_tools = {
        "SpriteSlicerState", "SliceGrid", "SliceAuto",
        "AtlasPackerState", "PackAtlas",
        "Animation2DAsset", "Anim2DPlay", "Anim2DStop",
        "NineSliceData", "SaveNineSlice",
        "CollisionShape2D", "AddCollisionShape2D",
        "Particle2DConfig",
    };

    for (const auto& tool : required_tools) {
        EXPECT_NE(content.find(tool), std::string::npos)
            << "Missing 2D tool: " << tool;
    }
}

// ── CI / Workflow Presence ──────────────────────────────────────

TEST(ReleaseMatrixTest, CIWorkflowExists) {
    EXPECT_TRUE(fs::exists(".github/workflows/ci.yml"));
}

TEST(ReleaseMatrixTest, ReleaseWorkflowExists) {
    EXPECT_TRUE(fs::exists(".github/workflows/release.yml"));
}
