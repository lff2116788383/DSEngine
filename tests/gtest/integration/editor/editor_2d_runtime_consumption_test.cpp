/**
 * @file editor_2d_runtime_consumption_test.cpp
 * @brief P1-8 .dsprite / .datlas versioned-format closure across the
 *        editor -> versioned file -> runtime-consumer boundary.
 *
 * The 2D editor writes .dsprite/.datlas; the runtime SpriteSheetAsset /
 * AtlasAsset loaders consume them at play time. This suite proves the full
 * loop now shares the engine version envelope (ReadVersionEnvelope /
 * WriteVersionEnvelope) and AssetDiagnostics:
 *   - editor save embeds the schema "version" and writes the canonical
 *     pixel_rect/uv_rect layout the runtime loader expects
 *   - the runtime consumer loads editor output directly (no migration)
 *   - a pre-v1 flat-rect file (x/y/w/h) migrates to pixel_rect on load
 *     (AssetDiagnostics.migrated) in both the editor and runtime loaders
 *   - a forward-version file loads leniently with a warning
 *   - the editor save/load round-trips its own edited state
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "editor_2d_tools.h"

#include "engine/assets/atlas_asset.h"
#include "engine/assets/sprite_sheet_asset.h"
#include "engine/core/asset_diagnostics.h"

namespace t2d = dse::editor::tools2d;
namespace fs = std::filesystem;

namespace {

fs::path TempPath(const std::string& name, const std::string& ext) {
    auto p = fs::temp_directory_path() /
             ("dse_2d_rt_" + name + "_" +
              std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
              ext);
    fs::remove(p);
    return p;
}

void WriteText(const fs::path& p, const std::string& text) {
    std::ofstream f(p, std::ios::binary);
    f << text;
}

t2d::SpriteSheetAsset MakeEditorSheet() {
    t2d::SpriteSheetAsset sheet;
    sheet.name = "hero";
    sheet.source_texture_path = "textures/hero.png";
    sheet.texture_width = 128;
    sheet.texture_height = 64;
    t2d::SliceGrid(sheet, 32, 32, 0, 0, 0);  // 4x2 = 8 frames
    return sheet;
}

}  // namespace

// ── .dsprite: editor save -> runtime consumption (canonical, no migration) ──
TEST(Sprite2DRuntimeConsumption, EditorSaveIsRuntimeLoadable) {
    t2d::SpriteSheetAsset sheet = MakeEditorSheet();
    ASSERT_FALSE(sheet.frames.empty());

    auto path = TempPath("sprite_canonical", ".dsprite");
    ASSERT_TRUE(t2d::SaveSpriteSheet(sheet, path.string()));

    // Editor output carries the shared version envelope.
    std::string content;
    {
        std::ifstream f(path);
        content.assign((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
    }
    EXPECT_NE(content.find("\"version\""), std::string::npos) << content;
    EXPECT_NE(content.find("pixel_rect"), std::string::npos);

    // Runtime consumer loads it directly, no migration needed.
    ::SpriteSheetAsset runtime;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(runtime.LoadFromFile(path.string(), diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, kSpriteSheetSchemaVersion);
    EXPECT_FALSE(diag.migrated);
    EXPECT_TRUE(diag.warnings.empty());

    ASSERT_EQ(runtime.frames.size(), sheet.frames.size());
    EXPECT_EQ(runtime.texture_path, "textures/hero.png");
    EXPECT_EQ(runtime.frames[0].pixel_rect.x, sheet.frames[0].x);
    EXPECT_EQ(runtime.frames[0].pixel_rect.y, sheet.frames[0].y);
    EXPECT_EQ(runtime.frames[0].pixel_rect.z, sheet.frames[0].w);
    EXPECT_EQ(runtime.frames[0].pixel_rect.w, sheet.frames[0].h);
    EXPECT_EQ(runtime.frames[0].name, sheet.frames[0].name);
    // uv_rect derived from the 128x64 sheet.
    EXPECT_FLOAT_EQ(runtime.frames[0].uv_rect.z, 32.0f / 128.0f);
    EXPECT_FLOAT_EQ(runtime.frames[0].uv_rect.w, 32.0f / 64.0f);

    fs::remove(path);
}

// ── .dsprite: pre-v1 flat rect file migrates in the runtime loader ──
TEST(Sprite2DRuntimeConsumption, LegacyFlatMigratesInRuntime) {
    const std::string legacy = R"({
        "name": "old",
        "texture": "textures/old.png",
        "width": 64, "height": 64,
        "frames": [
            { "name": "f0", "x": 0, "y": 0, "w": 32, "h": 16,
              "pivot_x": 0.25, "pivot_y": 0.75 }
        ]
    })";
    auto path = TempPath("sprite_legacy", ".dsprite");
    WriteText(path, legacy);

    ::SpriteSheetAsset runtime;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(runtime.LoadFromFile(path.string(), diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, dse::assets::kSchemaVersionLegacy);
    EXPECT_TRUE(diag.migrated);
    EXPECT_FALSE(diag.warnings.empty());

    ASSERT_EQ(runtime.frames.size(), 1u);
    EXPECT_EQ(runtime.frames[0].pixel_rect, glm::ivec4(0, 0, 32, 16));
    EXPECT_FLOAT_EQ(runtime.frames[0].uv_rect.z, 32.0f / 64.0f);
    EXPECT_FLOAT_EQ(runtime.frames[0].uv_rect.w, 16.0f / 64.0f);

    fs::remove(path);
}

// ── .dsprite: forward-version file loads leniently with a warning ──
TEST(Sprite2DRuntimeConsumption, ForwardVersionLoadsLeniently) {
    const std::string future =
        "{\"version\":999,\"texture\":\"t.png\",\"width\":8,\"height\":8,"
        "\"frames\":[{\"name\":\"f\",\"pixel_rect\":{\"x\":0,\"y\":0,\"w\":8,\"h\":8}}]}";
    auto path = TempPath("sprite_future", ".dsprite");
    WriteText(path, future);

    ::SpriteSheetAsset runtime;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(runtime.LoadFromFile(path.string(), diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, 999);
    EXPECT_FALSE(diag.migrated);
    EXPECT_FALSE(diag.warnings.empty());

    fs::remove(path);
}

// ── .dsprite: editor save/load round-trips edited state ──
TEST(Sprite2DRuntimeConsumption, EditorRoundTrip) {
    t2d::SpriteSheetAsset sheet = MakeEditorSheet();
    auto path = TempPath("sprite_rt", ".dsprite");
    ASSERT_TRUE(t2d::SaveSpriteSheet(sheet, path.string()));

    t2d::SpriteSheetAsset loaded;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(t2d::LoadSpriteSheet(loaded, path.string(), diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, t2d::kSpriteSheetSchemaVersion);
    EXPECT_FALSE(diag.migrated);

    ASSERT_EQ(loaded.frames.size(), sheet.frames.size());
    EXPECT_EQ(loaded.name, "hero");
    EXPECT_EQ(loaded.frames[1].x, sheet.frames[1].x);
    EXPECT_EQ(loaded.frames[1].w, sheet.frames[1].w);
    EXPECT_EQ(loaded.frames[1].name, sheet.frames[1].name);

    fs::remove(path);
}

// ── .datlas: editor save -> runtime consumption (canonical) ──
TEST(Atlas2DRuntimeConsumption, EditorSaveIsRuntimeLoadable) {
    t2d::AtlasAsset atlas;
    atlas.name = "ui_atlas";
    atlas.width = 256;
    atlas.height = 256;
    atlas.entries.push_back({"sprites/a.png", "a", 0, 0, 64, 64});
    atlas.entries.push_back({"sprites/b.png", "b", 64, 0, 32, 32});

    auto path = TempPath("atlas_canonical", ".datlas");
    ASSERT_TRUE(t2d::SaveAtlas(atlas, path.string()));

    ::AtlasAsset runtime;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(runtime.LoadFromFile(path.string(), diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, kAtlasSchemaVersion);
    EXPECT_FALSE(diag.migrated);

    ASSERT_EQ(runtime.entries.size(), 2u);
    const AtlasEntry* a = runtime.FindEntry("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->pixel_rect, glm::ivec4(0, 0, 64, 64));
    const AtlasEntry* b = runtime.FindEntry("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->pixel_rect, glm::ivec4(64, 0, 32, 32));
    EXPECT_FLOAT_EQ(b->uv_rect.x, 64.0f / 256.0f);

    fs::remove(path);
}

// ── .datlas: pre-v1 flat entry file migrates in the runtime loader ──
TEST(Atlas2DRuntimeConsumption, LegacyFlatMigratesInRuntime) {
    const std::string legacy = R"({
        "name": "old_atlas",
        "width": 128, "height": 128,
        "entries": [
            { "name": "e0", "src": "sprites/e0.png", "x": 10, "y": 20, "w": 30, "h": 40 }
        ]
    })";
    auto path = TempPath("atlas_legacy", ".datlas");
    WriteText(path, legacy);

    ::AtlasAsset runtime;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(runtime.LoadFromFile(path.string(), diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, dse::assets::kSchemaVersionLegacy);
    EXPECT_TRUE(diag.migrated);
    EXPECT_FALSE(diag.warnings.empty());

    ASSERT_EQ(runtime.entries.size(), 1u);
    EXPECT_EQ(runtime.entries[0].pixel_rect, glm::ivec4(10, 20, 30, 40));
    EXPECT_FLOAT_EQ(runtime.entries[0].uv_rect.x, 10.0f / 128.0f);

    fs::remove(path);
}
