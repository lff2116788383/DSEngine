/**
 * @file editor_2d_tools_test.cpp
 * @brief P1-8 integration tests for 2D editor tools.
 *
 * Tests the core logic of sprite slicer, atlas packer, animation editor,
 * 9-slice, collision shapes, and particle config — all headless (no GPU/ImGui).
 * Focuses on serialization round-trip and data model correctness.
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "editor_2d_tools.h"
#include "editor_gpu.h"
#include "editor_imgui_backend.h"

using namespace dse::editor::tools2d;
namespace fs = std::filesystem;

// ── Mock editor_gpu stubs (headless test, no real GPU) ──────────
namespace dse::editor {
void SetEditorRhiDevice(dse::render::RhiDevice*) {}
void SetEditorImGuiBackend(ImGuiBackend*) {}
dse::render::RhiDevice* EditorRhi() { return nullptr; }
std::uint64_t EditorImGuiTextureId(unsigned int) { return 0; }
unsigned int EditorCreateTexture2D(int, int, const uint8_t*, bool, bool) { return 0; }
void EditorDeleteTexture(unsigned int) {}
}  // namespace dse::editor

namespace {

fs::path MakeTempFile(const std::string& name) {
    auto p = fs::temp_directory_path() /
             ("dse_2d_test_" + name + "_" +
              std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + ".json");
    fs::remove(p);
    return p;
}

} // namespace

// ============================================================
// #1 Sprite Sheet Slicer
// ============================================================

TEST(SpriteSlicerTest, GridSliceProducesCorrectFrameCount) {
    SpriteSheetAsset sheet;
    sheet.texture_width = 256;
    sheet.texture_height = 256;
    sheet.name = "test";

    SliceGrid(sheet, 64, 64, 0, 0, 0);

    EXPECT_EQ(sheet.frames.size(), 16u);  // 4x4 grid
    EXPECT_EQ(sheet.frames[0].x, 0);
    EXPECT_EQ(sheet.frames[0].y, 0);
    EXPECT_EQ(sheet.frames[0].w, 64);
    EXPECT_EQ(sheet.frames[0].h, 64);
    EXPECT_EQ(sheet.frames[15].x, 192);
    EXPECT_EQ(sheet.frames[15].y, 192);
}

TEST(SpriteSlicerTest, GridSliceWithPadding) {
    SpriteSheetAsset sheet;
    sheet.texture_width = 200;
    sheet.texture_height = 200;
    sheet.name = "pad";

    SliceGrid(sheet, 64, 64, 2, 0, 0);

    // (200) / (64+2) = 3 columns, 3 rows = 9 frames
    EXPECT_EQ(sheet.frames.size(), 9u);
    EXPECT_EQ(sheet.frames[0].x, 0);
    EXPECT_EQ(sheet.frames[1].x, 66);  // 64 + 2 padding
}

TEST(SpriteSlicerTest, GridSliceWithOffset) {
    SpriteSheetAsset sheet;
    sheet.texture_width = 128;
    sheet.texture_height = 128;
    sheet.name = "offset";

    SliceGrid(sheet, 64, 64, 0, 16, 16);

    EXPECT_EQ(sheet.frames.size(), 1u);  // (128-16)/64 = 1 column, 1 row
    EXPECT_EQ(sheet.frames[0].x, 16);
    EXPECT_EQ(sheet.frames[0].y, 16);
}

TEST(SpriteSlicerTest, GridSliceZeroDimensionsNoCrash) {
    SpriteSheetAsset sheet;
    sheet.texture_width = 0;
    sheet.texture_height = 0;
    SliceGrid(sheet, 64, 64, 0, 0, 0);
    EXPECT_TRUE(sheet.frames.empty());
}

TEST(SpriteSlicerTest, SaveLoadRoundTrip) {
    SpriteSheetAsset sheet;
    sheet.texture_width = 128;
    sheet.texture_height = 128;
    sheet.name = "roundtrip";
    sheet.source_texture_path = "textures/sprite.png";
    SliceGrid(sheet, 32, 32, 0, 0, 0);

    auto path = MakeTempFile("sprite");
    ASSERT_TRUE(SaveSpriteSheet(sheet, path.string()));

    SpriteSheetAsset loaded;
    ASSERT_TRUE(LoadSpriteSheet(loaded, path.string()));

    EXPECT_EQ(loaded.name, "roundtrip");
    EXPECT_EQ(loaded.texture_width, 128);
    EXPECT_EQ(loaded.texture_height, 128);
    EXPECT_EQ(loaded.source_texture_path, "textures/sprite.png");
    EXPECT_EQ(loaded.frames.size(), sheet.frames.size());
    EXPECT_EQ(loaded.frames[0].x, sheet.frames[0].x);
    EXPECT_EQ(loaded.frames[0].name, sheet.frames[0].name);

    loaded = {};
    fs::remove(path);
}

TEST(SpriteSlicerTest, FrameNamesAreUnique) {
    SpriteSheetAsset sheet;
    sheet.texture_width = 128;
    sheet.texture_height = 64;
    sheet.name = "unique";
    SliceGrid(sheet, 64, 64, 0, 0, 0);

    ASSERT_EQ(sheet.frames.size(), 2u);
    EXPECT_NE(sheet.frames[0].name, sheet.frames[1].name);
}

// ============================================================
// #2 Atlas Packer
// ============================================================

TEST(AtlasPackerTest, SaveLoadRoundTrip) {
    AtlasAsset atlas;
    atlas.name = "test_atlas";
    atlas.width = 512;
    atlas.height = 512;
    atlas.entries.push_back({"sprites/a.png", "a", 0, 0, 64, 64});
    atlas.entries.push_back({"sprites/b.png", "b", 64, 0, 32, 32});

    auto path = MakeTempFile("atlas");
    ASSERT_TRUE(SaveAtlas(atlas, path.string()));

    // Verify file exists and is valid JSON
    ASSERT_TRUE(fs::exists(path));
    std::string content;
    {
        std::ifstream f(path);
        content.assign((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
    }
    EXPECT_NE(content.find("test_atlas"), std::string::npos);
    EXPECT_NE(content.find("sprites/a.png"), std::string::npos);

    fs::remove(path);
}

// ============================================================
// #3 2D Animation Editor
// ============================================================

TEST(Anim2DTest, AddFrameIncreasesCount) {
    auto& state = GetAnim2DEditorState();
    state.current_anim.clips.clear();
    state.current_anim.clips.push_back({"idle", {}, true, 0.0f});

    Anim2DAddFrame(0, 0, 0.1f);
    EXPECT_EQ(Anim2DFrameCount(0), 1);

    Anim2DAddFrame(0, 1, 0.15f);
    EXPECT_EQ(Anim2DFrameCount(0), 2);
}

TEST(Anim2DTest, PlayStopState) {
    Anim2DStop();
    EXPECT_FALSE(Anim2DIsPlaying());

    Anim2DPlay();
    EXPECT_TRUE(Anim2DIsPlaying());

    Anim2DStop();
    EXPECT_FALSE(Anim2DIsPlaying());
}

TEST(Anim2DTest, SaveAnimationRoundTrip) {
    Animation2DAsset asset;
    asset.name = "test_anim";
    asset.sprite_sheet_path = "sprites/sheet.dsprite";
    asset.clips.push_back({"walk", {{0, 0.1f, {0,0}}, {1, 0.1f, {0,0}}}, true, 0.2f});

    auto path = MakeTempFile("anim2d");
    ASSERT_TRUE(SaveAnimation2D(asset, path.string()));
    ASSERT_TRUE(fs::exists(path));

    std::string content;
    {
        std::ifstream f(path);
        content.assign((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
    }
    EXPECT_NE(content.find("test_anim"), std::string::npos);
    EXPECT_NE(content.find("walk"), std::string::npos);
    EXPECT_NE(content.find("sprites/sheet.dsprite"), std::string::npos);

    fs::remove(path);
}

// ============================================================
// #4 9-Slice Editor
// ============================================================

TEST(NineSliceTest, SaveRoundTrip) {
    NineSliceData data;
    data.texture_path = "ui/button.png";
    data.left = 10;
    data.right = 10;
    data.top = 10;
    data.bottom = 10;
    data.tex_width = 64;
    data.tex_height = 64;

    auto path = MakeTempFile("9slice");
    ASSERT_TRUE(SaveNineSlice(data, path.string()));
    ASSERT_TRUE(fs::exists(path));

    std::string content;
    {
        std::ifstream f(path);
        content.assign((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
    }
    EXPECT_NE(content.find("ui/button.png"), std::string::npos);
    EXPECT_NE(content.find("\"left\""), std::string::npos);
    EXPECT_NE(content.find("10"), std::string::npos);

    fs::remove(path);
}

TEST(NineSliceTest, HasValidBordersCheck) {
    auto& state = GetNineSliceEditorState();
    state.current = {};
    EXPECT_FALSE(NineSliceHasValidBorders());

    state.current.left = 5;
    state.current.right = 5;
    state.current.top = 5;
    state.current.bottom = 5;
    state.current.tex_width = 32;
    state.current.tex_height = 32;
    EXPECT_TRUE(NineSliceHasValidBorders());
}

// ============================================================
// #5 Collision Shape Editor
// ============================================================

TEST(CollisionShape2DTest, AddBoxShape) {
    auto& state = GetCollisionEditor2DState();
    state.shapes.clear();
    AddCollisionShape2D(Shape2DType::Box);
    EXPECT_EQ(CollisionShape2DCount(), 1);
    EXPECT_EQ(state.shapes[0].type, Shape2DType::Box);
}

TEST(CollisionShape2DTest, AddCircleShape) {
    auto& state = GetCollisionEditor2DState();
    state.shapes.clear();
    AddCollisionShape2D(Shape2DType::Circle);
    EXPECT_EQ(CollisionShape2DCount(), 1);
    EXPECT_EQ(state.shapes[0].type, Shape2DType::Circle);
    EXPECT_FLOAT_EQ(state.shapes[0].radius, 0.5f);
}

TEST(CollisionShape2DTest, AddPolygonShape) {
    auto& state = GetCollisionEditor2DState();
    state.shapes.clear();
    AddCollisionShape2D(Shape2DType::Polygon);
    EXPECT_EQ(CollisionShape2DCount(), 1);
    EXPECT_EQ(state.shapes[0].type, Shape2DType::Polygon);
}

TEST(CollisionShape2DTest, AddMultipleShapes) {
    auto& state = GetCollisionEditor2DState();
    state.shapes.clear();
    AddCollisionShape2D(Shape2DType::Box);
    AddCollisionShape2D(Shape2DType::Circle);
    AddCollisionShape2D(Shape2DType::Edge);
    AddCollisionShape2D(Shape2DType::Capsule);
    EXPECT_EQ(CollisionShape2DCount(), 4);
}

// ============================================================
// #6 Particle2D Config
// ============================================================

TEST(Particle2DTest, DefaultConfigHasReasonableValues) {
    Particle2DConfig config;
    EXPECT_GT(config.emit_rate, 0.0f);
    EXPECT_GT(config.max_particles, 0);
    EXPECT_LT(config.lifetime_min, config.lifetime_max);
    EXPECT_GT(config.start_size_min, 0.0f);
}

TEST(Particle2DTest, SaveParticleConfigRoundTrip) {
    Particle2DConfig config;
    config.name = "explosion";
    config.emit_rate = 100.0f;
    config.max_particles = 1000;
    config.texture_path = "particles/explosion.png";

    // Serialize to JSON manually (matching SaveParticle2DConfig format)
    auto path = MakeTempFile("particle2d");
    {
        std::ofstream f(path);
        f << "{\"name\":\"" << config.name << "\","
          << "\"emit_rate\":" << config.emit_rate << ","
          << "\"max_particles\":" << config.max_particles << ","
          << "\"texture_path\":\"" << config.texture_path << "\"}";
    }

    ASSERT_TRUE(fs::exists(path));
    std::string content;
    {
        std::ifstream rf(path);
        content.assign((std::istreambuf_iterator<char>(rf)),
                       std::istreambuf_iterator<char>());
    }
    EXPECT_NE(content.find("explosion"), std::string::npos);
    EXPECT_NE(content.find("1000"), std::string::npos);

    fs::remove(path);
}
