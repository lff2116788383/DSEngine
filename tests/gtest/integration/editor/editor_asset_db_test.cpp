/**
 * @file editor_asset_db_test.cpp
 * @brief 资产类型分类纯逻辑（editor_asset_db_core）的无头测试。
 *
 * 验证 AssetTypeFromExtension（扩展名 → AssetType，含多别名贴图/音频）与
 * AssetTypeToString（枚举 → 稳定字符串）的双向一致性。AssetDatabase 的磁盘
 * 扫描 / GUID / .meta 缓存依赖项目目录与文件系统，不在此纯逻辑覆盖。
 */

#include <gtest/gtest.h>

#include <string>

#include "editor_asset_db.h"

using namespace dse::editor;

// ── AssetTypeFromExtension：引擎自有扩展名 ───────────────────────────────────

TEST(AssetDb, EngineExtensionsMapToType) {
    EXPECT_EQ(AssetTypeFromExtension(".dmesh"), AssetType::Mesh);
    EXPECT_EQ(AssetTypeFromExtension(".dmat"), AssetType::Material);
    EXPECT_EQ(AssetTypeFromExtension(".danim"), AssetType::Animation);
    EXPECT_EQ(AssetTypeFromExtension(".dskel"), AssetType::Skeleton);
    EXPECT_EQ(AssetTypeFromExtension(".dscene"), AssetType::Scene);
    EXPECT_EQ(AssetTypeFromExtension(".dprefab"), AssetType::Prefab);
    EXPECT_EQ(AssetTypeFromExtension(".lua"), AssetType::Script);
    EXPECT_EQ(AssetTypeFromExtension(".dpak"), AssetType::Pak);
}

// ── AssetTypeFromExtension：贴图别名 ─────────────────────────────────────────

TEST(AssetDb, TextureAliasesAllMapToTexture) {
    for (const char* ext : {".png", ".jpg", ".jpeg", ".hdr", ".tga",
                            ".bmp", ".ktx", ".dds", ".exr"}) {
        EXPECT_EQ(AssetTypeFromExtension(ext), AssetType::Texture) << "ext=" << ext;
    }
}

// ── AssetTypeFromExtension：音频别名 ─────────────────────────────────────────

TEST(AssetDb, AudioAliasesAllMapToAudio) {
    for (const char* ext : {".wav", ".ogg", ".mp3", ".flac", ".aac"}) {
        EXPECT_EQ(AssetTypeFromExtension(ext), AssetType::Audio) << "ext=" << ext;
    }
}

// ── AssetTypeFromExtension：未知 / 边界 ──────────────────────────────────────

TEST(AssetDb, UnknownExtensionsMapToUnknown) {
    EXPECT_EQ(AssetTypeFromExtension(""), AssetType::Unknown);
    EXPECT_EQ(AssetTypeFromExtension(".txt"), AssetType::Unknown);
    EXPECT_EQ(AssetTypeFromExtension(".cpp"), AssetType::Unknown);
    EXPECT_EQ(AssetTypeFromExtension(".meta"), AssetType::Unknown);
    EXPECT_EQ(AssetTypeFromExtension("dmesh"), AssetType::Unknown);  // 无点号
}

TEST(AssetDb, ExtensionMatchIsCaseSensitive) {
    // 调用方负责小写化；核心函数本身大小写敏感。
    EXPECT_EQ(AssetTypeFromExtension(".PNG"), AssetType::Unknown);
    EXPECT_EQ(AssetTypeFromExtension(".DMESH"), AssetType::Unknown);
}

// ── AssetTypeToString ────────────────────────────────────────────────────────

TEST(AssetDb, TypeToStringCoversAllValues) {
    EXPECT_STREQ(AssetTypeToString(AssetType::Unknown), "Unknown");
    EXPECT_STREQ(AssetTypeToString(AssetType::Mesh), "Mesh");
    EXPECT_STREQ(AssetTypeToString(AssetType::Material), "Material");
    EXPECT_STREQ(AssetTypeToString(AssetType::Animation), "Animation");
    EXPECT_STREQ(AssetTypeToString(AssetType::Skeleton), "Skeleton");
    EXPECT_STREQ(AssetTypeToString(AssetType::Texture), "Texture");
    EXPECT_STREQ(AssetTypeToString(AssetType::Audio), "Audio");
    EXPECT_STREQ(AssetTypeToString(AssetType::Scene), "Scene");
    EXPECT_STREQ(AssetTypeToString(AssetType::Prefab), "Prefab");
    EXPECT_STREQ(AssetTypeToString(AssetType::Script), "Script");
    EXPECT_STREQ(AssetTypeToString(AssetType::Pak), "Pak");
}

// ── 往返：扩展名 → 类型 → 字符串 一致 ───────────────────────────────────────

TEST(AssetDb, RoundTripExtensionToTypeToString) {
    EXPECT_STREQ(AssetTypeToString(AssetTypeFromExtension(".dmesh")), "Mesh");
    EXPECT_STREQ(AssetTypeToString(AssetTypeFromExtension(".png")), "Texture");
    EXPECT_STREQ(AssetTypeToString(AssetTypeFromExtension(".wav")), "Audio");
    EXPECT_STREQ(AssetTypeToString(AssetTypeFromExtension(".xyz")), "Unknown");
}
