/**
 * @file editor_asset_refs_test.cpp
 * @brief P2-1 资产引用修正（rename/move reference fixup）的无头测试。
 *
 * 用确定性临时项目验证：重命名/移动一个被引用资产时，(1) 文件与其 .meta 一并
 * 移动且 GUID 不变；(2) 场景/预制体等 JSON 里对旧相对路径的引用被精确改写为
 * 新路径；(3) 前缀相同的其它路径（a.dmesh vs a.dmesh2）不被误伤。
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "editor_asset_refs_core.h"

using namespace dse::editor;
namespace fs = std::filesystem;

namespace {

fs::path MakeTempRoot() {
    fs::path root = fs::temp_directory_path() /
        ("dse_asset_refs_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
         "_" + std::to_string(reinterpret_cast<uintptr_t>(&root)));
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    return root;
}

void WriteFile(const fs::path& p, const std::string& content) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
    ofs << content;
}

std::string ReadFile(const fs::path& p) {
    std::ifstream ifs(p, std::ios::binary);
    std::stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

}  // namespace

// ── 纯 JSON 路径重写：精确整条匹配，不做子串替换 ────────────────────────────

TEST(AssetRefs, RewriteJsonRefsExactMatchOnly) {
    rapidjson::Document doc;
    doc.Parse(R"({
        "mesh_path": "meshes/a.dmesh",
        "other_path": "meshes/a.dmesh2",
        "nested": { "danim_path": "meshes/a.dmesh" },
        "list": ["meshes/a.dmesh", "keep/me.dmat"]
    })");
    ASSERT_FALSE(doc.HasParseError());

    int n = RewriteJsonPathRefs(doc, "meshes/a.dmesh", "meshes/b.dmesh", doc.GetAllocator());
    EXPECT_EQ(n, 3);  // mesh_path + nested.danim_path + list[0]
    EXPECT_STREQ(doc["mesh_path"].GetString(), "meshes/b.dmesh");
    EXPECT_STREQ(doc["other_path"].GetString(), "meshes/a.dmesh2");  // 前缀相同，未误伤
    EXPECT_STREQ(doc["nested"]["danim_path"].GetString(), "meshes/b.dmesh");
    EXPECT_STREQ(doc["list"][0].GetString(), "meshes/b.dmesh");
    EXPECT_STREQ(doc["list"][1].GetString(), "keep/me.dmat");
}

TEST(AssetRefs, NormalizeCollapsesSeparatorsAndDotSlash) {
    EXPECT_EQ(NormalizeAssetRefPath("meshes\\a.dmesh"), "meshes/a.dmesh");
    EXPECT_EQ(NormalizeAssetRefPath("./meshes/a.dmesh"), "meshes/a.dmesh");
    EXPECT_EQ(NormalizeAssetRefPath("meshes/a.dmesh"), "meshes/a.dmesh");
}

// ── 端到端：移动资产文件 + .meta + 跨文件引用修正 ───────────────────────────

TEST(AssetRefs, MoveAssetPreservesGuidAndFixesReferences) {
    const fs::path root = MakeTempRoot();

    // 被引用的资产 + 其 .meta（GUID 必须保留）
    WriteFile(root / "meshes" / "a.dmesh", "BINARY-MESH-DATA");
    const std::string guid = "0123456789abcdef0123456789abcdef";
    WriteFile(root / "meshes" / "a.dmesh.meta",
              R"({"guid":")" + guid + R"(","type":"Mesh"})");

    // 引用该资产的场景（JSON），含一条前缀相同的干扰路径
    WriteFile(root / "scenes" / "level.dscene", R"({
        "entities": [
            { "mesh_path": "meshes/a.dmesh" },
            { "mesh_path": "meshes/a.dmesh2" }
        ]
    })");

    MoveAssetResult r = MoveAssetWithFixup(root, "meshes/a.dmesh", "meshes/b.dmesh");

    ASSERT_TRUE(r.ok) << (r.errors.empty() ? "" : r.errors[0]);
    EXPECT_EQ(r.guid, guid);
    EXPECT_EQ(r.references_rewritten, 1);

    // 文件已移动，.meta 随之移动且 GUID 不变
    EXPECT_FALSE(fs::exists(root / "meshes" / "a.dmesh"));
    EXPECT_FALSE(fs::exists(root / "meshes" / "a.dmesh.meta"));
    EXPECT_TRUE(fs::exists(root / "meshes" / "b.dmesh"));
    ASSERT_TRUE(fs::exists(root / "meshes" / "b.dmesh.meta"));
    EXPECT_NE(ReadFile(root / "meshes" / "b.dmesh.meta").find(guid), std::string::npos);

    // 场景引用已改写，干扰路径未动
    const std::string scene = ReadFile(root / "scenes" / "level.dscene");
    EXPECT_NE(scene.find("meshes/b.dmesh\""), std::string::npos);
    EXPECT_NE(scene.find("meshes/a.dmesh2"), std::string::npos);
    EXPECT_EQ(scene.find("\"meshes/a.dmesh\""), std::string::npos);

    std::error_code ec;
    fs::remove_all(root, ec);
}

TEST(AssetRefs, MoveMissingSourceFails) {
    const fs::path root = MakeTempRoot();
    MoveAssetResult r = MoveAssetWithFixup(root, "meshes/nope.dmesh", "meshes/x.dmesh");
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.errors.empty());
    std::error_code ec;
    fs::remove_all(root, ec);
}

TEST(AssetRefs, MoveRefusesExistingDestination) {
    const fs::path root = MakeTempRoot();
    WriteFile(root / "a.dmesh", "A");
    WriteFile(root / "b.dmesh", "B");
    MoveAssetResult r = MoveAssetWithFixup(root, "a.dmesh", "b.dmesh");
    EXPECT_FALSE(r.ok);
    EXPECT_TRUE(fs::exists(root / "a.dmesh"));  // 未被覆盖
    std::error_code ec;
    fs::remove_all(root, ec);
}
