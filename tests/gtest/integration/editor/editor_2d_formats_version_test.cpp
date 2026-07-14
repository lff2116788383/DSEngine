/**
 * @file editor_2d_formats_version_test.cpp
 * @brief P1-8 versioned-format closure for the remaining editor-local 2D
 *        formats: .d9slice / .dparticle2d / .dparallax / .dlight2d.
 *
 * Each 2D authoring tool now writes a schema "version" via the shared engine
 * version envelope and loads back through an AssetDiagnostics-aware path that
 * migrates pre-v1 files. This suite proves, headlessly:
 *   - canonical save embeds the version and round-trips its edited state
 *   - a pre-v1 file (no "version", legacy/short field set) loads and reports
 *     AssetDiagnostics.migrated with a descriptive warning
 *   - a forward-version file loads leniently with a warning
 *   - malformed input is rejected with a recorded diagnostic error
 *   - the loaded editor data maps losslessly onto the runtime ECS components
 *     the 2D systems consume (ParallaxComponent, Light2DComponent /
 *     Ambient2DComponent, and the UI renderer's nine-slice fields)
 */

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "editor_2d_tools.h"

#include "engine/core/asset_diagnostics.h"
#include "engine/ecs/light_2d.h"
#include "engine/ecs/parallax_2d.h"
#include "engine/ecs/ui.h"

namespace t2d = dse::editor::tools2d;
namespace fs = std::filesystem;

namespace {

fs::path TempPath(const std::string& name, const std::string& ext) {
    auto p = fs::temp_directory_path() /
             ("dse_2d_fmt_" + name + "_" +
              std::to_string(::testing::UnitTest::GetInstance()->random_seed()) +
              ext);
    fs::remove(p);
    return p;
}

void WriteText(const fs::path& p, const std::string& text) {
    std::ofstream f(p, std::ios::binary);
    f << text;
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// .d9slice
// ─────────────────────────────────────────────────────────────────────────────

TEST(NineSliceVersioned, CanonicalRoundTrip) {
    t2d::NineSliceData data;
    data.texture_path = "ui/button.png";
    data.left = 12;
    data.right = 14;
    data.top = 10;
    data.bottom = 8;
    data.tex_width = 128;
    data.tex_height = 64;

    auto path = TempPath("nineslice_canonical", ".d9slice");
    ASSERT_TRUE(t2d::SaveNineSlice(data, path.string()));

    t2d::NineSliceData loaded;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(t2d::LoadNineSlice(loaded, path.string(), diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, t2d::kNineSliceSchemaVersion);
    EXPECT_FALSE(diag.migrated);
    EXPECT_TRUE(diag.warnings.empty());

    EXPECT_EQ(loaded.texture_path, data.texture_path);
    EXPECT_EQ(loaded.left, data.left);
    EXPECT_EQ(loaded.right, data.right);
    EXPECT_EQ(loaded.top, data.top);
    EXPECT_EQ(loaded.bottom, data.bottom);
    EXPECT_EQ(loaded.tex_width, data.tex_width);
    EXPECT_EQ(loaded.tex_height, data.tex_height);
}

TEST(NineSliceVersioned, LegacyNoVersionLoads) {
    auto path = TempPath("nineslice_legacy", ".d9slice");
    WriteText(path,
              "{\n"
              "  \"texture\": \"ui/panel.png\",\n"
              "  \"left\": 4, \"right\": 6,\n"
              "  \"top\": 5, \"bottom\": 7,\n"
              "  \"width\": 64, \"height\": 32\n"
              "}\n");

    t2d::NineSliceData loaded;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(t2d::LoadNineSlice(loaded, path.string(), diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, dse::assets::kSchemaVersionLegacy);
    EXPECT_EQ(loaded.left, 4);
    EXPECT_EQ(loaded.tex_width, 64);
}

TEST(NineSliceVersioned, MalformedRejected) {
    auto path = TempPath("nineslice_bad", ".d9slice");
    WriteText(path, "{ this is not json ]");

    t2d::NineSliceData loaded;
    dse::assets::AssetDiagnostics diag;
    EXPECT_FALSE(t2d::LoadNineSlice(loaded, path.string(), diag));
    EXPECT_FALSE(diag.ok);
    EXPECT_FALSE(diag.errors.empty());
}

TEST(NineSliceVersioned, RuntimeUiConsumption) {
    t2d::NineSliceData data;
    data.texture_path = "ui/frame.png";
    data.left = 16;
    data.right = 16;
    data.top = 8;
    data.bottom = 8;
    data.tex_width = 128;
    data.tex_height = 64;

    auto path = TempPath("nineslice_rt", ".d9slice");
    ASSERT_TRUE(t2d::SaveNineSlice(data, path.string()));

    t2d::NineSliceData loaded;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(t2d::LoadNineSlice(loaded, path.string(), diag));

    // Map editor pixel borders onto the UI renderer's nine-slice fields
    // (border stored as UV fractions: left, bottom, right, top).
    UIRendererComponent ui;
    ui.nine_slice_enabled = true;
    ui.nine_slice_src_size =
        glm::vec2(static_cast<float>(loaded.tex_width), static_cast<float>(loaded.tex_height));
    ui.nine_slice_border = glm::vec4(
        static_cast<float>(loaded.left) / loaded.tex_width,
        static_cast<float>(loaded.bottom) / loaded.tex_height,
        static_cast<float>(loaded.right) / loaded.tex_width,
        static_cast<float>(loaded.top) / loaded.tex_height);

    EXPECT_TRUE(ui.nine_slice_enabled);
    EXPECT_FLOAT_EQ(ui.nine_slice_src_size.x, 128.0f);
    EXPECT_FLOAT_EQ(ui.nine_slice_src_size.y, 64.0f);
    EXPECT_FLOAT_EQ(ui.nine_slice_border.x, 16.0f / 128.0f);
    EXPECT_FLOAT_EQ(ui.nine_slice_border.y, 8.0f / 64.0f);
    EXPECT_FLOAT_EQ(ui.nine_slice_border.z, 16.0f / 128.0f);
    EXPECT_FLOAT_EQ(ui.nine_slice_border.w, 8.0f / 64.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// .dparticle2d
// ─────────────────────────────────────────────────────────────────────────────

TEST(Particle2DVersioned, CanonicalRoundTrip) {
    t2d::Particle2DConfig cfg;
    cfg.name = "sparks";
    cfg.emit_rate = 120.0f;
    cfg.max_particles = 800;
    cfg.emit_shape = t2d::Particle2DEmitShape::Circle;
    cfg.emit_radius = 24.0f;
    cfg.emit_rect = {3, 5};
    cfg.lifetime_min = 0.25f;
    cfg.lifetime_max = 1.5f;
    cfg.velocity_min = {-30, -120};
    cfg.velocity_max = {30, -220};
    cfg.gravity = {0, 180};
    cfg.angular_velocity_min = -45.0f;
    cfg.angular_velocity_max = 45.0f;
    cfg.damping = 0.1f;
    cfg.start_size_min = 6.0f;
    cfg.start_size_max = 12.0f;
    cfg.end_size = 1.0f;
    cfg.start_color = {1, 0.8f, 0.2f, 1};
    cfg.end_color = {1, 0.2f, 0, 0};
    cfg.blend_mode = t2d::Particle2DBlendMode::Additive;
    cfg.texture_path = "fx/spark.png";
    cfg.trail_enabled = true;
    cfg.trail_length = 8;
    cfg.trail_width = 3.0f;

    auto path = TempPath("particle_canonical", ".dparticle2d");
    ASSERT_TRUE(t2d::SaveParticle2DConfig(cfg, path.string()));

    t2d::Particle2DConfig loaded;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(t2d::LoadParticle2DConfig(loaded, path.string(), diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, t2d::kParticle2DSchemaVersion);
    EXPECT_FALSE(diag.migrated);

    EXPECT_EQ(loaded.name, cfg.name);
    EXPECT_FLOAT_EQ(loaded.emit_rate, cfg.emit_rate);
    EXPECT_EQ(loaded.max_particles, cfg.max_particles);
    EXPECT_EQ(static_cast<int>(loaded.emit_shape), static_cast<int>(cfg.emit_shape));
    EXPECT_FLOAT_EQ(loaded.emit_radius, cfg.emit_radius);
    EXPECT_FLOAT_EQ(loaded.emit_rect.y, cfg.emit_rect.y);
    EXPECT_FLOAT_EQ(loaded.velocity_max.y, cfg.velocity_max.y);
    EXPECT_FLOAT_EQ(loaded.angular_velocity_min, cfg.angular_velocity_min);
    EXPECT_FLOAT_EQ(loaded.damping, cfg.damping);
    EXPECT_FLOAT_EQ(loaded.start_color.y, cfg.start_color.y);
    EXPECT_FLOAT_EQ(loaded.end_color.w, cfg.end_color.w);
    EXPECT_EQ(static_cast<int>(loaded.blend_mode), static_cast<int>(cfg.blend_mode));
    EXPECT_EQ(loaded.texture_path, cfg.texture_path);
    EXPECT_TRUE(loaded.trail_enabled);
    EXPECT_EQ(loaded.trail_length, cfg.trail_length);
    EXPECT_FLOAT_EQ(loaded.trail_width, cfg.trail_width);
}

TEST(Particle2DVersioned, LegacyShortFormMigrates) {
    // Pre-v1 file: no version, only the small field set the early save emitted.
    auto path = TempPath("particle_legacy", ".dparticle2d");
    WriteText(path,
              "{\n"
              "  \"name\": \"smoke\",\n"
              "  \"emit_rate\": 40,\n"
              "  \"max_particles\": 300,\n"
              "  \"emit_shape\": 0,\n"
              "  \"lifetime_min\": 1.0,\n"
              "  \"lifetime_max\": 3.0,\n"
              "  \"velocity_min\": [-10, -20],\n"
              "  \"velocity_max\": [10, -40],\n"
              "  \"gravity\": [0, 50],\n"
              "  \"start_size_min\": 4,\n"
              "  \"start_size_max\": 8,\n"
              "  \"end_size\": 1,\n"
              "  \"blend_mode\": 0,\n"
              "  \"trail_enabled\": false\n"
              "}\n");

    t2d::Particle2DConfig loaded;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(t2d::LoadParticle2DConfig(loaded, path.string(), diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, dse::assets::kSchemaVersionLegacy);
    EXPECT_TRUE(diag.migrated);
    EXPECT_FALSE(diag.warnings.empty());

    EXPECT_EQ(loaded.name, "smoke");
    EXPECT_FLOAT_EQ(loaded.emit_rate, 40.0f);
    // Fields absent in the legacy file keep the current-schema defaults.
    t2d::Particle2DConfig defaults;
    EXPECT_FLOAT_EQ(loaded.damping, defaults.damping);
    EXPECT_EQ(loaded.trail_length, defaults.trail_length);
}

TEST(Particle2DVersioned, MalformedRejected) {
    auto path = TempPath("particle_bad", ".dparticle2d");
    WriteText(path, "not json at all");

    t2d::Particle2DConfig loaded;
    dse::assets::AssetDiagnostics diag;
    EXPECT_FALSE(t2d::LoadParticle2DConfig(loaded, path.string(), diag));
    EXPECT_FALSE(diag.ok);
    EXPECT_FALSE(diag.errors.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// .dparallax
// ─────────────────────────────────────────────────────────────────────────────

static t2d::ParallaxConfig MakeParallax() {
    t2d::ParallaxConfig cfg;
    cfg.name = "forest";
    cfg.base_speed = 1.5f;
    t2d::ParallaxLayer sky;
    sky.name = "sky";
    sky.texture_path = "bg/sky.png";
    sky.scroll_factor_x = 0.1f;
    sky.scroll_factor_y = 0.05f;
    sky.offset_y = 10.0f;
    sky.repeat_x = true;
    sky.repeat_y = false;
    sky.sort_order = 0;
    sky.opacity = 1.0f;
    sky.tint = {0.9f, 0.95f, 1.0f, 1.0f};
    cfg.layers.push_back(sky);
    t2d::ParallaxLayer trees;
    trees.name = "trees";
    trees.texture_path = "bg/trees.png";
    trees.scroll_factor_x = 0.6f;
    trees.scroll_factor_y = 0.6f;
    trees.sort_order = 2;
    trees.opacity = 0.85f;
    cfg.layers.push_back(trees);
    return cfg;
}

TEST(ParallaxVersioned, CanonicalRoundTrip) {
    t2d::ParallaxConfig cfg = MakeParallax();

    auto path = TempPath("parallax_canonical", ".dparallax");
    ASSERT_TRUE(t2d::SaveParallaxConfig(cfg, path.string()));

    t2d::ParallaxConfig loaded;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(t2d::LoadParallaxConfig(loaded, path.string(), diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, t2d::kParallaxSchemaVersion);
    EXPECT_FALSE(diag.migrated);

    ASSERT_EQ(loaded.layers.size(), cfg.layers.size());
    EXPECT_EQ(loaded.name, cfg.name);
    EXPECT_FLOAT_EQ(loaded.base_speed, cfg.base_speed);
    EXPECT_EQ(loaded.layers[0].name, "sky");
    EXPECT_FLOAT_EQ(loaded.layers[0].scroll_factor_x, 0.1f);
    EXPECT_FLOAT_EQ(loaded.layers[0].tint.x, 0.9f);
    EXPECT_EQ(loaded.layers[1].sort_order, 2);
    EXPECT_FLOAT_EQ(loaded.layers[1].opacity, 0.85f);
}

TEST(ParallaxVersioned, LegacyScrollKeysMigrate) {
    auto path = TempPath("parallax_legacy", ".dparallax");
    WriteText(path,
              "{\n"
              "  \"name\": \"old\",\n"
              "  \"base_speed\": 1.0,\n"
              "  \"layers\": [\n"
              "    { \"name\": \"l0\", \"texture\": \"bg/a.png\", \"scroll_x\": 0.3, "
              "\"scroll_y\": 0.4, \"offset_y\": 0, \"repeat_x\": true, "
              "\"sort_order\": 1, \"opacity\": 1.0 }\n"
              "  ]\n"
              "}\n");

    t2d::ParallaxConfig loaded;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(t2d::LoadParallaxConfig(loaded, path.string(), diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, dse::assets::kSchemaVersionLegacy);
    EXPECT_TRUE(diag.migrated);
    EXPECT_FALSE(diag.warnings.empty());

    ASSERT_EQ(loaded.layers.size(), 1u);
    EXPECT_FLOAT_EQ(loaded.layers[0].scroll_factor_x, 0.3f);
    EXPECT_FLOAT_EQ(loaded.layers[0].scroll_factor_y, 0.4f);
}

TEST(ParallaxVersioned, ForwardVersionLoadsWithWarning) {
    auto path = TempPath("parallax_forward", ".dparallax");
    WriteText(path,
              "{\n"
              "  \"version\": 999,\n"
              "  \"name\": \"future\",\n"
              "  \"base_speed\": 1.0,\n"
              "  \"layers\": [\n"
              "    { \"name\": \"l0\", \"texture\": \"bg/a.png\", "
              "\"scroll_factor_x\": 0.5, \"scroll_factor_y\": 0.5 }\n"
              "  ]\n"
              "}\n");

    t2d::ParallaxConfig loaded;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(t2d::LoadParallaxConfig(loaded, path.string(), diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, 999);
    EXPECT_FALSE(diag.warnings.empty());
    ASSERT_EQ(loaded.layers.size(), 1u);
    EXPECT_FLOAT_EQ(loaded.layers[0].scroll_factor_x, 0.5f);
}

TEST(ParallaxVersioned, RuntimeComponentConsumption) {
    t2d::ParallaxConfig cfg = MakeParallax();
    auto path = TempPath("parallax_rt", ".dparallax");
    ASSERT_TRUE(t2d::SaveParallaxConfig(cfg, path.string()));

    t2d::ParallaxConfig loaded;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(t2d::LoadParallaxConfig(loaded, path.string(), diag));

    // Build the runtime ParallaxComponent the ParallaxSystem consumes each frame.
    ParallaxComponent comp;
    comp.enabled = true;
    for (const auto& el : loaded.layers) {
        ParallaxLayer rl;
        rl.name = el.name;
        rl.scroll_factor_x = el.scroll_factor_x;
        rl.scroll_factor_y = el.scroll_factor_y;
        rl.offset_y = el.offset_y;
        rl.repeat_x = el.repeat_x;
        rl.repeat_y = el.repeat_y;
        rl.sorting_order = el.sort_order;
        rl.opacity = el.opacity;
        rl.tint = el.tint;
        comp.layers.push_back(rl);
    }

    ASSERT_EQ(comp.layers.size(), 2u);
    EXPECT_TRUE(comp.enabled);
    EXPECT_EQ(comp.layers[0].name, "sky");
    EXPECT_FLOAT_EQ(comp.layers[0].scroll_factor_x, 0.1f);
    EXPECT_EQ(comp.layers[1].sorting_order, 2);
    EXPECT_FLOAT_EQ(comp.layers[1].opacity, 0.85f);
}

// ─────────────────────────────────────────────────────────────────────────────
// .dlight2d
// ─────────────────────────────────────────────────────────────────────────────

static t2d::Light2DEditorState MakeLightScene() {
    t2d::Light2DEditorState st;
    st.ambient_color = {0.1f, 0.12f, 0.2f};
    st.ambient_intensity = 0.4f;

    t2d::Light2DConfig point;
    point.name = "torch";
    point.type = t2d::Light2DType::Point;
    point.position = {100, 50};
    point.color = {1.0f, 0.6f, 0.3f};
    point.intensity = 1.5f;
    point.range = 180.0f;
    point.falloff = 2.5f;
    point.shadow_mode = t2d::Light2DShadowMode::Soft;
    point.shadow_softness = 0.7f;
    point.shadow_rays = 96;
    st.lights.push_back(point);

    t2d::Light2DConfig spot;
    spot.name = "lamp";
    spot.type = t2d::Light2DType::Spot;
    spot.position = {-40, 120};
    spot.color = {0.8f, 0.9f, 1.0f};
    spot.intensity = 2.0f;
    spot.range = 240.0f;
    spot.spot_angle = 30.0f;
    spot.spot_direction = 90.0f;
    st.lights.push_back(spot);
    return st;
}

TEST(Light2DVersioned, CanonicalRoundTrip) {
    t2d::Light2DEditorState st = MakeLightScene();

    auto path = TempPath("light_canonical", ".dlight2d");
    ASSERT_TRUE(t2d::SaveLight2DScene(st, path.string()));

    t2d::Light2DEditorState loaded;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(t2d::LoadLight2DScene(loaded, path.string(), diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, t2d::kLight2DSchemaVersion);
    EXPECT_FALSE(diag.migrated);

    EXPECT_FLOAT_EQ(loaded.ambient_intensity, 0.4f);
    EXPECT_FLOAT_EQ(loaded.ambient_color.z, 0.2f);
    ASSERT_EQ(loaded.lights.size(), 2u);
    EXPECT_EQ(loaded.lights[0].name, "torch");
    EXPECT_EQ(static_cast<int>(loaded.lights[0].shadow_mode),
              static_cast<int>(t2d::Light2DShadowMode::Soft));
    EXPECT_EQ(loaded.lights[0].shadow_rays, 96);
    EXPECT_FLOAT_EQ(loaded.lights[1].spot_angle, 30.0f);
    EXPECT_FLOAT_EQ(loaded.lights[1].spot_direction, 90.0f);
}

TEST(Light2DVersioned, LegacyShortFormMigrates) {
    auto path = TempPath("light_legacy", ".dlight2d");
    WriteText(path,
              "{\n"
              "  \"ambient_color\": [0.2, 0.2, 0.3],\n"
              "  \"ambient_intensity\": 0.5,\n"
              "  \"lights\": [\n"
              "    { \"name\": \"l0\", \"type\": 0, \"position\": [0, 0], "
              "\"color\": [1, 1, 1], \"intensity\": 1.0, \"range\": 100, "
              "\"falloff\": 2.0, \"shadow_mode\": 0 }\n"
              "  ]\n"
              "}\n");

    t2d::Light2DEditorState loaded;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(t2d::LoadLight2DScene(loaded, path.string(), diag));
    EXPECT_TRUE(diag.ok);
    EXPECT_EQ(diag.source_version, dse::assets::kSchemaVersionLegacy);
    EXPECT_TRUE(diag.migrated);
    EXPECT_FALSE(diag.warnings.empty());

    ASSERT_EQ(loaded.lights.size(), 1u);
    // Spot/shadow fields absent in the legacy file keep current defaults.
    t2d::Light2DConfig defaults;
    EXPECT_FLOAT_EQ(loaded.lights[0].spot_angle, defaults.spot_angle);
    EXPECT_EQ(loaded.lights[0].shadow_rays, defaults.shadow_rays);
}

TEST(Light2DVersioned, RuntimeComponentConsumption) {
    t2d::Light2DEditorState st = MakeLightScene();
    auto path = TempPath("light_rt", ".dlight2d");
    ASSERT_TRUE(t2d::SaveLight2DScene(st, path.string()));

    t2d::Light2DEditorState loaded;
    dse::assets::AssetDiagnostics diag;
    ASSERT_TRUE(t2d::LoadLight2DScene(loaded, path.string(), diag));

    // Global ambient maps onto the runtime Ambient2DComponent singleton.
    Ambient2DComponent ambient;
    ambient.color = loaded.ambient_color;
    ambient.intensity = loaded.ambient_intensity;
    EXPECT_FLOAT_EQ(ambient.intensity, 0.4f);

    // Each editor light maps onto a runtime Light2DComponent the Light2DSystem
    // reads (enum values line up between the editor-local and runtime enums).
    ASSERT_FALSE(loaded.lights.empty());
    const auto& el = loaded.lights[0];
    Light2DComponent lc;
    lc.type = static_cast<Light2DType>(static_cast<int>(el.type));
    lc.color = el.color;
    lc.intensity = el.intensity;
    lc.range = el.range;
    lc.falloff = el.falloff;
    lc.shadow_mode = static_cast<Shadow2DMode>(static_cast<int>(el.shadow_mode));
    lc.shadow_strength = el.shadow_softness;
    lc.shadow_ray_count = el.shadow_rays;

    EXPECT_EQ(static_cast<int>(lc.type), static_cast<int>(Light2DType::Point));
    EXPECT_FLOAT_EQ(lc.intensity, 1.5f);
    EXPECT_FLOAT_EQ(lc.range, 180.0f);
    EXPECT_EQ(static_cast<int>(lc.shadow_mode), static_cast<int>(Shadow2DMode::Soft));
    EXPECT_EQ(lc.shadow_ray_count, 96);
}
