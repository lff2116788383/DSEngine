/**
 * @file ui_tests_2d_tools.cpp
 * @brief UI tests for 2D editor tools: Sprite Slicer, Atlas Packer, Frame Animation,
 *        9-Slice, Collision Shape, 2D Particles, Parallax, 2D Lighting
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cmath>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_2d_tools.h"

namespace dse::editor::uitest {

void Register2DToolsTests(ImGuiTestEngine* e) {

    // ═══════════════════════════════════════════════════════════════════════
    // #1 — Sprite Sheet Slicer
    // ═══════════════════════════════════════════════════════════════════════

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "sprite_slicer_grid");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            auto& st = GetSpriteSlicerState();
            st.current_sheet.source_texture_path = "test_sheet.png";
            st.current_sheet.texture_width = 256;
            st.current_sheet.texture_height = 256;
            st.current_sheet.name = "TestSheet";

            SliceGrid(st.current_sheet, 64, 64, 0, 0, 0);
            IM_CHECK(SpriteSlicerFrameCount() == 16);  // 4x4 grid

            // Verify frame positions
            IM_CHECK(st.current_sheet.frames[0].x == 0);
            IM_CHECK(st.current_sheet.frames[0].y == 0);
            IM_CHECK(st.current_sheet.frames[0].w == 64);
            IM_CHECK(st.current_sheet.frames[0].h == 64);
            IM_CHECK(st.current_sheet.frames[1].x == 64);

            ctx->Yield(2);
        };
    }

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "sprite_slicer_grid_padding");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            auto& st = GetSpriteSlicerState();
            st.current_sheet.texture_width = 256;
            st.current_sheet.texture_height = 128;
            st.current_sheet.name = "PaddedSheet";

            SliceGrid(st.current_sheet, 32, 32, 2, 0, 0);
            // 256 / (32+2) = 7 cols, 128 / (32+2) = 3 rows = 21 frames
            IM_CHECK(SpriteSlicerFrameCount() == 21);

            ctx->Yield(2);
        };
    }

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "sprite_slicer_auto");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            auto& st = GetSpriteSlicerState();
            st.current_sheet.texture_width = 128;
            st.current_sheet.texture_height = 128;
            st.current_sheet.name = "AutoSheet";

            SliceAuto(st.current_sheet, 0.1f);
            IM_CHECK(SpriteSlicerFrameCount() > 0);

            // Names should contain "auto"
            IM_CHECK(st.current_sheet.frames[0].name.find("auto") != std::string::npos);

            ctx->Yield(2);
        };
    }

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "sprite_slicer_mode_name");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            auto& st = GetSpriteSlicerState();
            st.mode = SliceMode::Grid;
            IM_CHECK(strcmp(SpriteSlicerModeName(), "Grid") == 0);
            st.mode = SliceMode::Auto;
            IM_CHECK(strcmp(SpriteSlicerModeName(), "Auto") == 0);
            st.mode = SliceMode::Manual;
            IM_CHECK(strcmp(SpriteSlicerModeName(), "Manual") == 0);

            ctx->Yield(2);
        };
    }

    // ═══════════════════════════════════════════════════════════════════════
    // #2 — Sprite Atlas Packer
    // ═══════════════════════════════════════════════════════════════════════

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "atlas_packer_basic");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            auto& st = GetAtlasPackerState();
            st.current_atlas.max_size = 512;
            st.current_atlas.padding = 2;
            st.current_atlas.power_of_two = true;

            std::vector<std::string> inputs = {
                "sprites/hero.png", "sprites/enemy.png",
                "sprites/bullet.png", "sprites/powerup.png"
            };

            bool ok = PackAtlas(st.current_atlas, inputs);
            IM_CHECK(ok);
            IM_CHECK(AtlasEntryCount() == 4);
            IM_CHECK(st.current_atlas.width > 0);
            IM_CHECK(st.current_atlas.height > 0);

            // Power of two check
            IM_CHECK((st.current_atlas.width & (st.current_atlas.width - 1)) == 0);
            IM_CHECK((st.current_atlas.height & (st.current_atlas.height - 1)) == 0);

            // Names should be extracted from paths
            IM_CHECK(st.current_atlas.entries[0].name == "hero");
            IM_CHECK(st.current_atlas.entries[1].name == "enemy");

            ctx->Yield(2);
        };
    }

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "atlas_packer_empty");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            AtlasAsset atlas;
            std::vector<std::string> empty;
            bool ok = PackAtlas(atlas, empty);
            IM_CHECK(!ok);  // Should fail with empty input
            IM_CHECK(atlas.entries.empty());

            ctx->Yield(2);
        };
    }

    // ═══════════════════════════════════════════════════════════════════════
    // #3 — 2D Frame Animation Editor
    // ═══════════════════════════════════════════════════════════════════════

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "anim2d_clip_management");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            auto& st = GetAnim2DEditorState();
            st.current_anim.clips.clear();

            Animation2DClip idle;
            idle.name = "Idle";
            idle.loop = true;
            st.current_anim.clips.push_back(idle);

            Animation2DClip run;
            run.name = "Run";
            run.loop = true;
            st.current_anim.clips.push_back(run);

            IM_CHECK(Anim2DClipCount() == 2);
            IM_CHECK(st.current_anim.clips[0].name == "Idle");
            IM_CHECK(st.current_anim.clips[1].name == "Run");

            ctx->Yield(2);
        };
    }

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "anim2d_add_frames");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            auto& st = GetAnim2DEditorState();
            st.current_anim.clips.clear();
            Animation2DClip clip;
            clip.name = "Walk";
            st.current_anim.clips.push_back(clip);

            Anim2DAddFrame(0, 0, 0.1f);
            Anim2DAddFrame(0, 1, 0.1f);
            Anim2DAddFrame(0, 2, 0.15f);
            Anim2DAddFrame(0, 3, 0.1f);

            IM_CHECK(Anim2DFrameCount(0) == 4);
            IM_CHECK(std::abs(st.current_anim.clips[0].total_duration - 0.45f) < 0.001f);

            ctx->Yield(2);
        };
    }

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "anim2d_playback");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            IM_CHECK(!Anim2DIsPlaying());
            Anim2DPlay();
            IM_CHECK(Anim2DIsPlaying());
            Anim2DStop();
            IM_CHECK(!Anim2DIsPlaying());

            ctx->Yield(2);
        };
    }

    // ═══════════════════════════════════════════════════════════════════════
    // #4 — 9-Slice / 9-Patch Editor
    // ═══════════════════════════════════════════════════════════════════════

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "nineslice_valid_borders");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            auto& st = GetNineSliceEditorState();
            st.current.tex_width = 128;
            st.current.tex_height = 128;
            st.current.left = 16;
            st.current.right = 16;
            st.current.top = 16;
            st.current.bottom = 16;

            IM_CHECK(NineSliceHasValidBorders());

            // Invalid: borders exceed texture size
            st.current.left = 70;
            st.current.right = 70;
            IM_CHECK(!NineSliceHasValidBorders());  // 70+70 = 140 > 128

            // Reset to valid
            st.current.left = 10;
            st.current.right = 10;
            IM_CHECK(NineSliceHasValidBorders());

            ctx->Yield(2);
        };
    }

    // ═══════════════════════════════════════════════════════════════════════
    // #5 — 2D Collision Shape Editor
    // ═══════════════════════════════════════════════════════════════════════

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "collision2d_add_shapes");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            auto& st = GetCollisionEditor2DState();
            st.shapes.clear();

            AddCollisionShape2D(Shape2DType::Box);
            IM_CHECK(CollisionShape2DCount() == 1);
            IM_CHECK(st.shapes[0].type == Shape2DType::Box);
            IM_CHECK(st.shapes[0].half_extents.x == 0.5f);

            AddCollisionShape2D(Shape2DType::Circle);
            IM_CHECK(CollisionShape2DCount() == 2);
            IM_CHECK(st.shapes[1].type == Shape2DType::Circle);
            IM_CHECK(st.shapes[1].radius == 0.5f);

            AddCollisionShape2D(Shape2DType::Polygon);
            IM_CHECK(CollisionShape2DCount() == 3);
            IM_CHECK(st.shapes[2].vertices.size() == 4);  // default quad

            AddCollisionShape2D(Shape2DType::Edge);
            IM_CHECK(CollisionShape2DCount() == 4);
            IM_CHECK(st.shapes[3].vertices.size() == 2);

            AddCollisionShape2D(Shape2DType::Capsule);
            IM_CHECK(CollisionShape2DCount() == 5);
            IM_CHECK(st.shapes[4].capsule_height == 1.0f);

            ctx->Yield(2);
        };
    }

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "collision2d_selection");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            auto& st = GetCollisionEditor2DState();
            st.shapes.clear();
            st.selected_shape = -1;

            AddCollisionShape2D(Shape2DType::Box);
            // AddCollisionShape2D auto-selects the new shape
            IM_CHECK(st.selected_shape == 0);

            AddCollisionShape2D(Shape2DType::Circle);
            IM_CHECK(st.selected_shape == 1);

            ctx->Yield(2);
        };
    }

    // ═══════════════════════════════════════════════════════════════════════
    // #6 — 2D Particle Editor
    // ═══════════════════════════════════════════════════════════════════════

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "particle2d_simulation");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            auto& st = GetParticle2DEditorState();
            st.simulating = false;
            st.sim_time = 0;
            st.active_particles = 0;

            IM_CHECK(!Particle2DSimulating());
            IM_CHECK(Particle2DActiveCount() == 0);

            st.simulating = true;
            IM_CHECK(Particle2DSimulating());

            st.simulating = false;
            ctx->Yield(2);
        };
    }

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "particle2d_config_defaults");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            auto& st = GetParticle2DEditorState();
            Particle2DConfig cfg;

            IM_CHECK(cfg.emit_rate == 50.0f);
            IM_CHECK(cfg.max_particles == 500);
            IM_CHECK(cfg.emit_shape == Particle2DEmitShape::Point);
            IM_CHECK(cfg.blend_mode == Particle2DBlendMode::Additive);
            IM_CHECK(!cfg.trail_enabled);

            ctx->Yield(2);
        };
    }

    // ═══════════════════════════════════════════════════════════════════════
    // #7 — Parallax Layer Editor
    // ═══════════════════════════════════════════════════════════════════════

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "parallax_add_layers");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            auto& st = GetParallaxEditorState();
            st.config.layers.clear();
            st.selected_layer = -1;

            AddParallaxLayer("Sky");
            IM_CHECK(ParallaxLayerCount() == 1);
            IM_CHECK(st.config.layers[0].name == "Sky");
            IM_CHECK(st.selected_layer == 0);

            AddParallaxLayer("Mountains");
            AddParallaxLayer("Trees");
            IM_CHECK(ParallaxLayerCount() == 3);
            IM_CHECK(st.selected_layer == 2);

            // Sort order assigned incrementally
            IM_CHECK(st.config.layers[0].sort_order == 0);
            IM_CHECK(st.config.layers[1].sort_order == 1);
            IM_CHECK(st.config.layers[2].sort_order == 2);

            ctx->Yield(2);
        };
    }

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "parallax_layer_properties");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            auto& st = GetParallaxEditorState();
            st.config.layers.clear();
            AddParallaxLayer("Background");

            auto& layer = st.config.layers[0];
            IM_CHECK(layer.scroll_factor_x == 1.0f);
            IM_CHECK(layer.scroll_factor_y == 1.0f);
            IM_CHECK(layer.repeat_x == true);
            IM_CHECK(layer.repeat_y == false);
            IM_CHECK(layer.opacity == 1.0f);

            layer.scroll_factor_x = 0.5f;
            IM_CHECK(layer.scroll_factor_x == 0.5f);

            ctx->Yield(2);
        };
    }

    // ═══════════════════════════════════════════════════════════════════════
    // #8 — 2D Lighting Editor
    // ═══════════════════════════════════════════════════════════════════════

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "light2d_add_lights");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            auto& st = GetLight2DEditorState();
            st.lights.clear();
            st.selected_light = -1;

            AddLight2D(Light2DType::Point);
            IM_CHECK(Light2DCount() == 1);
            IM_CHECK(st.lights[0].type == Light2DType::Point);
            IM_CHECK(st.selected_light == 0);

            AddLight2D(Light2DType::Spot);
            IM_CHECK(Light2DCount() == 2);
            IM_CHECK(st.lights[1].type == Light2DType::Spot);

            AddLight2D(Light2DType::Directional);
            IM_CHECK(Light2DCount() == 3);
            IM_CHECK(st.lights[2].type == Light2DType::Directional);

            ctx->Yield(2);
        };
    }

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "light2d_properties");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            auto& st = GetLight2DEditorState();
            st.lights.clear();
            AddLight2D(Light2DType::Point);

            auto& light = st.lights[0];
            IM_CHECK(light.intensity == 1.0f);
            IM_CHECK(light.range == 200.0f);
            IM_CHECK(light.falloff == 2.0f);
            IM_CHECK(light.shadow_mode == Light2DShadowMode::None);
            IM_CHECK(!light.use_normal_map);

            // Modify and verify
            light.intensity = 2.5f;
            light.shadow_mode = Light2DShadowMode::Soft;
            light.use_normal_map = true;
            IM_CHECK(light.intensity == 2.5f);
            IM_CHECK(light.shadow_mode == Light2DShadowMode::Soft);
            IM_CHECK(light.use_normal_map);

            ctx->Yield(2);
        };
    }

    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-2d-tools", "light2d_ambient");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            using namespace dse::editor::tools2d;
            ctx->Yield(2);

            auto& st = GetLight2DEditorState();
            // Default ambient
            IM_CHECK(st.ambient_intensity == 0.3f);
            IM_CHECK(st.ambient_color.x > 0.0f);

            st.ambient_intensity = 0.5f;
            st.ambient_color = {0.2f, 0.2f, 0.3f};
            IM_CHECK(st.ambient_intensity == 0.5f);

            ctx->Yield(2);
        };
    }
}

}  // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
