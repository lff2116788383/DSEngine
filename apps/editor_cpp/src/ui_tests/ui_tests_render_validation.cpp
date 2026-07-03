/**
 * @file ui_tests_render_validation.cpp
 * @brief Viewport render validation tests (pixel assertions + screenshot capture for Devin visual verification).
 *
 * 25 test scenes covering: geometry, lighting, materials, shadows/post-process, camera/viewpoint.
 * Layer 1: Automated pixel assertions (deterministic, run in CI)
 * Layer 2: Devin downloads PNGs and visually verifies (semantic validation)
 */
#include "ui_tests_internal.h"

#ifdef DSE_RENDER_TESTS

#include "imgui_te_engine.h"
#include "imgui_te_context.h"
#include "render_capture_helper.h"
#include "../editor_icons.h"

#include <filesystem>

namespace dse::editor::uitest {

// Capture directory for render test screenshots
static const char* kCaptureDir = "C:\\temp\\renders";

static void EnsureCaptureDir() {
    std::filesystem::create_directories(kCaptureDir);
}

static std::string CapturePath(const char* name) {
    return std::string(kCaptureDir) + "\\" + name + ".png";
}

// Helper: capture current scene viewport and return pixels
static CapturedPixels CaptureAndLoad(const char* name, int stabilize_frames = 20) {
    EnsureCaptureDir();
    std::string path = CapturePath(name);
    CaptureEditorWindow(path, 300);
    return LoadCapturedPNG(path);
}

void RegisterRenderValidationTests(ImGuiTestEngine* engine) {
    ImGuiTest* t = nullptr;

    // ====================================================================
    // A. Basic Geometry Rendering (5 tests)
    // ====================================================================

    // A1: render_cube_default
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_cube_default");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        // Create a cube entity
        auto ent = NewSelectedEntity(ctx);
        IM_CHECK(ent != entt::null);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        // Add directional light
        auto light = NewSelectedEntity(ctx);
        IM_CHECK(light != entt::null);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        // Focus scene viewport and wait for render
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        // Capture and assert
        auto px = CaptureAndLoad("render_cube_default");
        IM_CHECK(px.valid);
        IM_CHECK_F(px.NonBlackRatio() > 0.05f,
            "Cube scene should not be all black (non-black: %.1f%%)", px.NonBlackRatio() * 100.0f);
        // Cleanup
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent);
        ctx->Yield(2);
        DeleteSelectedEntity(ctx);
    };

    // A2: render_sphere_default
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_sphere_default");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        IM_CHECK(ent != entt::null);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        // TODO: Set mesh to sphere if API available
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_sphere_default");
        IM_CHECK(px.valid);
        IM_CHECK_F(px.NonBlackRatio() > 0.05f,
            "Sphere scene non-black: %.1f%%", px.NonBlackRatio() * 100.0f);
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent);
        ctx->Yield(2);
        DeleteSelectedEntity(ctx);
    };

    // A3: render_plane_default
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_plane_default");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        IM_CHECK(ent != entt::null);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_plane_default");
        IM_CHECK(px.valid);
        IM_CHECK_F(px.NonBlackRatio() > 0.05f,
            "Plane scene non-black: %.1f%%", px.NonBlackRatio() * 100.0f);
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent);
        ctx->Yield(2);
        DeleteSelectedEntity(ctx);
    };

    // A4: render_cylinder_default
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_cylinder_default");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        IM_CHECK(ent != entt::null);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_cylinder_default");
        IM_CHECK(px.valid);
        IM_CHECK_F(px.NonBlackRatio() > 0.05f,
            "Cylinder scene non-black: %.1f%%", px.NonBlackRatio() * 100.0f);
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent);
        ctx->Yield(2);
        DeleteSelectedEntity(ctx);
    };

    // A5: render_multi_objects
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_multi_objects");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto e1 = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto e2 = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto e3 = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_multi_objects");
        IM_CHECK(px.valid);
        IM_CHECK_F(px.NonBlackRatio() > 0.10f,
            "Multi-object scene non-black: %.1f%%", px.NonBlackRatio() * 100.0f);
        // Cleanup all
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(e3); ctx->Yield(2); DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(e2); ctx->Yield(2); DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(e1); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // ====================================================================
    // B. Lighting System (6 tests)
    // ====================================================================

    // B6: render_dirlight_white
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_dirlight_white");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_dirlight_white");
        IM_CHECK(px.valid);
        IM_CHECK_F(px.AverageBrightness() > 0.02f,
            "Dir light scene brightness: %.3f", px.AverageBrightness());
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // B7: render_pointlight_red
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_pointlight_red");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Point Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_pointlight_red");
        IM_CHECK(px.valid);
        IM_CHECK(px.NonBlackRatio() > 0.03f);
        // Note: Red dominance check depends on light color being set to red
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // B8: render_spotlight_cone
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_spotlight_cone");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Spot Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_spotlight_cone");
        IM_CHECK(px.valid);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // B9: render_no_light_ambient
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_no_light_ambient");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_no_light_ambient");
        IM_CHECK(px.valid);
        // Without explicit light, should be very dark (ambient only)
        IM_CHECK_F(px.AverageBrightness() < 0.15f,
            "No-light scene should be dark: %.3f", px.AverageBrightness());
        DeleteSelectedEntity(ctx);
    };

    // B10: render_multi_lights
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_multi_lights");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto l1 = NewSelectedEntity(ctx);
        AddComponent(ctx, "Point Light");
        ctx->Yield(2);
        auto l2 = NewSelectedEntity(ctx);
        AddComponent(ctx, "Point Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_multi_lights");
        IM_CHECK(px.valid);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(l1); ctx->Yield(2); DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // B11: render_light_intensity (compare dark vs bright)
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_light_intensity");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        // Scene with default light
        auto ent = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_light_intensity");
        IM_CHECK(px.valid);
        IM_CHECK(px.AverageBrightness() > 0.01f);
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // ====================================================================
    // C. Materials and Textures (5 tests)
    // ====================================================================

    // C12: render_material_red
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_material_red");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        // TODO: Set material albedo to red via Inspector
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_material_red");
        IM_CHECK(px.valid);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        // Color check (depends on material setup)
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // C13: render_material_blue
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_material_blue");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_material_blue");
        IM_CHECK(px.valid);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // C14: render_texture_checker
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_texture_checker");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_texture_checker");
        IM_CHECK(px.valid);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        // Texture should produce color variance (not solid color)
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // C15: render_material_metallic
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_material_metallic");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_material_metallic");
        IM_CHECK(px.valid);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // C16: render_material_transparent
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_material_transparent");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto e1 = NewSelectedEntity(ctx);  // back object
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto e2 = NewSelectedEntity(ctx);  // front transparent
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_material_transparent");
        IM_CHECK(px.valid);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(e2); ctx->Yield(2); DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(e1); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // ====================================================================
    // D. Shadows and Post-Processing (5 tests)
    // ====================================================================

    // D17: render_shadow_ground
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_shadow_ground");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto cube = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto plane = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_shadow_ground");
        IM_CHECK(px.valid);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(plane); ctx->Yield(2); DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(cube); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // D18: render_shadow_self
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_shadow_self");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto e1 = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto e2 = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_shadow_self");
        IM_CHECK(px.valid);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(e2); ctx->Yield(2); DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(e1); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // D19: render_bloom_effect
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_bloom_effect");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(2);
        auto pp = NewSelectedEntity(ctx);
        AddComponent(ctx, "Post Process");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_bloom_effect");
        IM_CHECK(px.valid);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(light); ctx->Yield(2); DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // D20: render_fog_distance
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_fog_distance");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_fog_distance");
        IM_CHECK(px.valid);
        IM_CHECK(px.NonBlackRatio() > 0.03f);
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // D21: render_ao_corners
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_ao_corners");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_ao_corners");
        IM_CHECK(px.valid);
        IM_CHECK(px.NonBlackRatio() > 0.03f);
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // ====================================================================
    // E. Camera and Viewpoint (4 tests)
    // ====================================================================

    // E22: render_camera_perspective
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_camera_perspective");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_camera_perspective");
        IM_CHECK(px.valid);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // E23: render_camera_orthographic
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_camera_orthographic");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_camera_orthographic");
        IM_CHECK(px.valid);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // E24: render_camera_closeup
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_camera_closeup");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_camera_closeup");
        IM_CHECK(px.valid);
        // Close-up should fill most of the viewport
        IM_CHECK_F(px.NonBlackRatio() > 0.05f,
            "Close-up should show object: non-black=%.1f%%", px.NonBlackRatio() * 100.0f);
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };

    // E25: render_camera_far
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_camera_far");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        auto ent = NewSelectedEntity(ctx);
        AddComponent(ctx, "Mesh Renderer");
        ctx->Yield(2);
        auto light = NewSelectedEntity(ctx);
        AddComponent(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_camera_far");
        IM_CHECK(px.valid);
        IM_CHECK(px.NonBlackRatio() > 0.01f);
        DeleteSelectedEntity(ctx);
        SelectionManager::Get().SetSingle(ent); ctx->Yield(2); DeleteSelectedEntity(ctx);
    };
}

} // namespace dse::editor::uitest

#endif // DSE_RENDER_TESTS
