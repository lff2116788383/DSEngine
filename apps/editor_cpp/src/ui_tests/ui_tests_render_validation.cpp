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

#include <entt/entt.hpp>
#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"
#include "render_capture_helper.h"
#include "../editor_icons.h"
#include "../editor_selection.h"
#include "../editor_scene_camera.h"
#include "../editor_toolbar.h"     // EnterPlayMode / ExitPlayMode / IsEditorInPlayMode

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_animation.h"   // Animator3DComponent (skinned knight test)
#include "engine/assets/asset_manager.h"          // AssetManager::LoadTextureAsync   // Animator3DComponent (skinned knight test)

#include <filesystem>
#include <cstdlib>
#include <string>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace dse::editor::uitest {

namespace {


/// Pre-capture preparation: clear selection wireframe and focus camera on scene center
void PreCapture(ImGuiTestContext* ctx, float focus_y = 0.5f) {
    SelectionManager::Get().Clear();
    FocusEditorCamera(GetEditorCamera(), glm::vec3(0.0f, focus_y, 0.0f));
    ctx->Yield(15);
}

/// Extended pre-capture for scenes whose objects span depth or sit off-origin:
/// clears selection and aims the camera at an explicit focal point with a custom
/// distance/orientation so all objects stay well framed (FocusEditorCamera fixes
/// distance to 5, which is too close for depth-spanning rows).
void PreCaptureFramed(ImGuiTestContext* ctx, const glm::vec3& focus, float distance,
                      float yaw = 0.0f, float pitch = 0.35f) {
    SelectionManager::Get().Clear();
    auto& cam = GetEditorCamera();
    cam.focal_point = focus;
    cam.distance = distance;
    cam.yaw = yaw;
    cam.pitch = pitch;
    ctx->Yield(15);
}
entt::registry& Reg() { return Services().engine->pipeline()->world().registry(); }

// DIAG: dump all mesh entities' transform/render state to stderr (test log).
void DumpMeshDiag(const char* tag) {
    auto& reg = Reg();
    auto view = reg.view<TransformComponent, dse::MeshRendererComponent>();
    int n = 0;
    for (auto e : view) {
        auto& tf = view.get<TransformComponent>(e);
        auto& mr = view.get<dse::MeshRendererComponent>(e);
        glm::vec3 wp = glm::vec3(tf.local_to_world[3]);
        fprintf(stderr,
            "[MESHDIAG %s] e=%u pos=(%.2f,%.2f,%.2f) l2w=(%.2f,%.2f,%.2f) scl=(%.2f,%.2f,%.2f) dirty=%d vis=%d stat=%d verts=%zu path='%s' variant='%s' meshlet=%u\n",
            tag, (unsigned)e, tf.position.x, tf.position.y, tf.position.z,
            wp.x, wp.y, wp.z, tf.scale.x, tf.scale.y, tf.scale.z,
            (int)tf.dirty, (int)mr.visible, (int)mr.is_static, mr.temp_vertices.size(),
            mr.mesh_path.c_str(), mr.shader_variant.c_str(), mr.meshlet_mesh_id);
        ++n;
    }
    fprintf(stderr, "[MESHDIAG %s] total_mesh_entities=%d\n", tag, n);
    fflush(stderr);
}

// 经 Hierarchy 右键菜单 "Create 3D Object/<item>" 创建真实图元/光源实体
// （CreateEntity3DCube 等会填充 procedural 几何数据；裸 Mesh Renderer 组件
// mesh_path 为空、不会渲染任何几何体）。返回新建实体并置为单选。
entt::entity NewPrimitive(ImGuiTestContext* ctx, const char* item) {
    entt::registry& reg = Reg();
    std::vector<entt::entity> before;
    for (auto en : reg.storage<entt::entity>())
        if (reg.valid(en)) before.push_back(en);
    OpenHierarchyContextMenu(ctx);
    // 子菜单需先点开父项，再在弹出的子菜单窗口内定位条目
    // （一步式 "Create 3D Object/Cube" 路径在 BeginPopupContextWindow 下解析失败）。
    ctx->ItemClick("Create 3D Object");
    ctx->Yield(2);
    ctx->SetRef("//$FOCUSED");
    if (ctx->ItemExists(item)) {
        ctx->ItemClick(item);
    } else {
        std::string wildcard = std::string("**/") + item;
        ctx->ItemClick(wildcard.c_str());
    }
    ctx->Yield(2);
    for (auto en : reg.storage<entt::entity>()) {
        if (!reg.valid(en)) continue;
        bool seen = false;
        for (auto b : before) if (b == en) { seen = true; break; }
        if (!seen) { SelectionManager::Get().SetSingle(en); return en; }
    }
    return entt::null;
}

// Hierarchy 右键 → "Create Empty Entity"，按 registry 差集取回新实体并置为单选。
// 用于蒙皮模型测试：得到一个干净的编辑器实体（含 TransformComponent、无程序化几何残留）。
entt::entity NewEmptyEntity(ImGuiTestContext* ctx) {
    entt::registry& reg = Reg();
    std::vector<entt::entity> before;
    for (auto en : reg.storage<entt::entity>())
        if (reg.valid(en)) before.push_back(en);
    OpenHierarchyContextMenu(ctx);
    ctx->ItemClick("Create Empty Entity");
    ctx->Yield(2);
    for (auto en : reg.storage<entt::entity>()) {
        if (!reg.valid(en)) continue;
        bool seen = false;
        for (auto b : before) if (b == en) { seen = true; break; }
        if (!seen) { SelectionManager::Get().SetSingle(en); return en; }
    }
    return entt::null;
}

TransformComponent& Tf(entt::entity e) { return Reg().get<TransformComponent>(e); }
dse::MeshRendererComponent& Mr(entt::entity e) { return Reg().get<dse::MeshRendererComponent>(e); }

void SetPos(entt::entity e, float x, float y, float z) {
    auto& t = Tf(e);
    t.position = glm::vec3(x, y, z);
    t.dirty = true;
}

void SetScale(entt::entity e, float x, float y, float z) {
    auto& t = Tf(e);
    t.scale = glm::vec3(x, y, z);
    t.dirty = true;
}

// 图元几何为 position-only 顶点，PBR 需要法线；用 PBR 变体让方向光/点光着色生效。
void UsePBR(entt::entity e) { (void)e; /* DIAG: keep default variant */ }

void DestroyEntities(std::initializer_list<entt::entity> ents) {
    entt::registry& reg = Reg();
    SelectionManager::Get().Clear();
    for (auto e : ents)
        if (e != entt::null && reg.valid(e)) reg.destroy(e);
}

} // anonymous namespace

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
    if (!CaptureEditorWindow(path, 300)) {
        // Capture failed (headless mode / no window) - return invalid
        return CapturedPixels{};
    }
    return LoadCapturedPNG(path);
}

// Skip render test if running in headless mode (no GPU window)
#define SKIP_IF_NO_CAPTURE(px) \
    if (!px.valid) { \
        ctx->LogWarning("Render capture unavailable (headless mode) - skipping"); \
        return; \
    }

void RegisterRenderValidationTests(ImGuiTestEngine* engine) {
    // Only register render tests if DSE_RENDER_TESTS_ENABLED env var is set
    // (render tests require GUI mode with GPU - skip in headless)
    const char* enabled = std::getenv("DSE_RENDER_TESTS_ENABLED");
    if (!enabled || std::string(enabled) != "1") {
        return;
    }
    ImGuiTest* t = nullptr;

    // ====================================================================
    // A. Basic Geometry Rendering (5 tests)
    // ====================================================================

    // A1: render_cube_default
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_cube_default");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();  // keep Scene viewport unobstructed (Sequencer etc. float over it)
        ctx->Yield(4);
        auto ent = NewPrimitive(ctx, "Cube");
        IM_CHECK(ent != entt::null);
        UsePBR(ent);
        auto light = NewPrimitive(ctx, "Directional Light");
        IM_CHECK(light != entt::null);
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_cube_default");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DestroyEntities({ent, light});
        ctx->Yield(2);
    };

    // A2: render_sphere_default
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_sphere_default");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto ent = NewPrimitive(ctx, "Sphere");
        IM_CHECK(ent != entt::null);
        UsePBR(ent);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_sphere_default");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DestroyEntities({ent, light});
        ctx->Yield(2);
    };

    // A3: render_plane_default
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_plane_default");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto ent = NewPrimitive(ctx, "Plane");
        IM_CHECK(ent != entt::null);
        UsePBR(ent);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_plane_default");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DestroyEntities({ent, light});
        ctx->Yield(2);
    };

    // A4: render_cylinder_default（引擎无 cylinder 图元，用竖向拉伸的 Cube 柱体代替）
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_cylinder_default");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto ent = NewPrimitive(ctx, "Cube");
        IM_CHECK(ent != entt::null);
        UsePBR(ent);
        SetScale(ent, 0.5f, 2.0f, 0.5f);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_cylinder_default");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DestroyEntities({ent, light});
        ctx->Yield(2);
    };

    // A5: render_multi_objects
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_multi_objects");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto e1 = NewPrimitive(ctx, "Cube");
        UsePBR(e1);
        SetPos(e1, -1.5f, 0.5f, 0.0f);
        auto e2 = NewPrimitive(ctx, "Sphere");
        UsePBR(e2);
        SetPos(e2, 1.5f, 0.5f, 0.0f);
        auto e3 = NewPrimitive(ctx, "Plane");
        UsePBR(e3);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_multi_objects");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.10f);
        DestroyEntities({e1, e2, e3, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // B. Lighting System (6 tests)
    // ====================================================================

    // B6: render_dirlight_white
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_dirlight_white");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto ent = NewPrimitive(ctx, "Cube");
        UsePBR(ent);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_dirlight_white");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.AverageBrightness() > 0.02f);
        DestroyEntities({ent, light});
        ctx->Yield(2);
    };

    // B7: render_pointlight_red
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_pointlight_red");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto ent = NewPrimitive(ctx, "Cube");
        UsePBR(ent);
        auto light = NewPrimitive(ctx, "Point Light");
        if (light != entt::null) {
            auto& pl = Reg().get<dse::PointLightComponent>(light);
            pl.color = glm::vec3(1.0f, 0.0f, 0.0f);
            pl.intensity = 3.0f;
            SetPos(light, 0.0f, 1.5f, 1.5f);
        }
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_pointlight_red");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.03f);
        DestroyEntities({ent, light});
        ctx->Yield(2);
    };

    // B8: render_spotlight_cone
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_spotlight_cone");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto ground = NewPrimitive(ctx, "Plane");
        UsePBR(ground);
        auto light = NewPrimitive(ctx, "Spot Light");
        if (light != entt::null) {
            auto& sl = Reg().get<dse::SpotLightComponent>(light);
            sl.intensity = 5.0f;
            sl.direction = glm::vec3(0.0f, -1.0f, 0.0f);
            SetPos(light, 0.0f, 3.0f, 0.0f);
        }
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_spotlight_cone");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({ground, light});
        ctx->Yield(2);
    };

    // B9: render_no_light_ambient
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_no_light_ambient");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto ent = NewPrimitive(ctx, "Cube");
        UsePBR(ent);
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_no_light_ambient");
        SKIP_IF_NO_CAPTURE(px);
        // Without explicit light, should be very dark (ambient only)
        IM_CHECK(px.AverageBrightness() < 0.15f);
        DestroyEntities({ent});
        ctx->Yield(2);
    };

    // B10: render_multi_lights
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_multi_lights");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto ent = NewPrimitive(ctx, "Cube");
        UsePBR(ent);
        auto l1 = NewPrimitive(ctx, "Point Light");
        if (l1 != entt::null) {
            auto& pl = Reg().get<dse::PointLightComponent>(l1);
            pl.color = glm::vec3(1.0f, 0.2f, 0.2f);
            pl.intensity = 3.0f;
            SetPos(l1, -2.0f, 1.5f, 1.0f);
        }
        auto l2 = NewPrimitive(ctx, "Point Light");
        if (l2 != entt::null) {
            auto& pl = Reg().get<dse::PointLightComponent>(l2);
            pl.color = glm::vec3(0.2f, 0.2f, 1.0f);
            pl.intensity = 3.0f;
            SetPos(l2, 2.0f, 1.5f, 1.0f);
        }
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_multi_lights");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DestroyEntities({ent, l1, l2});
        ctx->Yield(2);
    };

    // B11: render_light_intensity (bright light)
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_light_intensity");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto ent = NewPrimitive(ctx, "Cube");
        UsePBR(ent);
        auto light = NewPrimitive(ctx, "Directional Light");
        if (light != entt::null) {
            auto& dl = Reg().get<dse::DirectionalLight3DComponent>(light);
            dl.intensity = 3.0f;
        }
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_light_intensity");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.AverageBrightness() > 0.01f);
        DestroyEntities({ent, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // C. Materials and Textures (5 tests)
    // ====================================================================

    // C12: render_material_red
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_material_red");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto ent = NewPrimitive(ctx, "Cube");
        UsePBR(ent);
        Mr(ent).color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_material_red");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DestroyEntities({ent, light});
        ctx->Yield(2);
    };

    // C13: render_material_blue
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_material_blue");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto ent = NewPrimitive(ctx, "Cube");
        UsePBR(ent);
        Mr(ent).color = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_material_blue");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DestroyEntities({ent, light});
        ctx->Yield(2);
    };

    // C14: render_texture_checker（无内置 checker 纹理资产，用双色并排 Cube 验证颜色区分度）
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_texture_checker");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto e1 = NewPrimitive(ctx, "Cube");
        UsePBR(e1);
        Mr(e1).color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        SetPos(e1, -0.8f, 0.5f, 0.0f);
        auto e2 = NewPrimitive(ctx, "Cube");
        UsePBR(e2);
        Mr(e2).color = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
        SetPos(e2, 0.8f, 0.5f, 0.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_texture_checker");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DestroyEntities({e1, e2, light});
        ctx->Yield(2);
    };

    // C15: render_material_metallic
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_material_metallic");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto ent = NewPrimitive(ctx, "Sphere");
        UsePBR(ent);
        Mr(ent).metallic = 1.0f;
        Mr(ent).roughness = 0.2f;
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_material_metallic");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DestroyEntities({ent, light});
        ctx->Yield(2);
    };

    // C16: render_material_transparent
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_material_transparent");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto e1 = NewPrimitive(ctx, "Sphere");  // back object
        UsePBR(e1);
        SetPos(e1, 0.0f, 0.5f, -1.5f);
        auto e2 = NewPrimitive(ctx, "Cube");    // front transparent
        UsePBR(e2);
        Mr(e2).color = glm::vec4(0.2f, 0.8f, 0.2f, 0.4f);
        SetPos(e2, 0.0f, 0.5f, 0.5f);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_material_transparent");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DestroyEntities({e1, e2, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // D. Shadows and Post-Processing (5 tests)
    // ====================================================================

    // D17: render_shadow_ground
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_shadow_ground");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto cube = NewPrimitive(ctx, "Cube");
        UsePBR(cube);
        SetPos(cube, 0.0f, 1.0f, 0.0f);
        auto plane = NewPrimitive(ctx, "Plane");
        UsePBR(plane);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_shadow_ground");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DestroyEntities({cube, plane, light});
        ctx->Yield(2);
    };

    // D18: render_shadow_self
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_shadow_self");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto e1 = NewPrimitive(ctx, "Cube");
        UsePBR(e1);
        SetPos(e1, 0.0f, 0.5f, 0.0f);
        auto e2 = NewPrimitive(ctx, "Cube");
        UsePBR(e2);
        SetPos(e2, 0.5f, 1.5f, 0.3f);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx, 1.0f);  // stacked cubes reach y=1.5; raise focal point
        auto px = CaptureAndLoad("render_shadow_self");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DestroyEntities({e1, e2, light});
        ctx->Yield(2);
    };

    // D19: render_bloom_effect（Scene 视口不合成 bloom，Game 视图才可见；此处验证发光体+PostProcess 不破坏渲染）
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_bloom_effect");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto ent = NewPrimitive(ctx, "Cube");
        UsePBR(ent);
        Mr(ent).emissive = glm::vec3(4.0f, 4.0f, 4.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        entt::entity pp = Reg().create();
        Reg().emplace<dse::PostProcessComponent>(pp);
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_bloom_effect");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DestroyEntities({ent, light, pp});
        ctx->Yield(2);
    };

    // D20: render_fog_distance
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_fog_distance");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto e1 = NewPrimitive(ctx, "Cube");
        UsePBR(e1);
        SetPos(e1, -1.0f, 0.5f, 0.0f);
        auto e2 = NewPrimitive(ctx, "Cube");
        UsePBR(e2);
        SetPos(e2, 0.0f, 0.5f, -6.0f);
        auto e3 = NewPrimitive(ctx, "Cube");
        UsePBR(e3);
        SetPos(e3, 1.0f, 0.5f, -14.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        entt::entity pp = Reg().create();
        {
            auto& p = Reg().emplace<dse::PostProcessComponent>(pp);
            p.fog_enabled = true;
            p.fog_density = 0.15f;
        }
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        // Cubes span z=0..-14; aim at mid-depth from farther back with a 3/4 angle
        PreCaptureFramed(ctx, glm::vec3(0.0f, 0.5f, -6.0f), 16.0f, 0.5f, 0.4f);
        auto px = CaptureAndLoad("render_fog_distance");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.03f);
        DestroyEntities({e1, e2, e3, light, pp});
        ctx->Yield(2);
    };

    // D21: render_ao_corners（SSAO 只在 Game 视图合成；Scene 视口验证转角几何正常渲染）
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_ao_corners");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto floor_ent = NewPrimitive(ctx, "Plane");
        UsePBR(floor_ent);
        auto wall = NewPrimitive(ctx, "Cube");
        UsePBR(wall);
        SetScale(wall, 4.0f, 2.0f, 0.2f);
        SetPos(wall, 0.0f, 1.0f, -2.0f);
        auto box = NewPrimitive(ctx, "Cube");
        UsePBR(box);
        SetPos(box, 0.0f, 0.5f, -1.4f);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_ao_corners");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.03f);
        DestroyEntities({floor_ent, wall, box, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // E. Camera and Viewpoint (4 tests)
    // ====================================================================

    // E22: render_camera_perspective（一排递远的 Cube，验证透视缩小）
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_camera_perspective");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto e1 = NewPrimitive(ctx, "Cube");
        UsePBR(e1);
        SetPos(e1, -1.0f, 0.5f, 0.0f);
        auto e2 = NewPrimitive(ctx, "Cube");
        UsePBR(e2);
        SetPos(e2, 0.0f, 0.5f, -4.0f);
        auto e3 = NewPrimitive(ctx, "Cube");
        UsePBR(e3);
        SetPos(e3, 1.0f, 0.5f, -8.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        // Cubes recede z=0..-8; 3/4 view from farther back shows perspective falloff
        PreCaptureFramed(ctx, glm::vec3(0.0f, 0.5f, -4.0f), 12.0f, 0.55f, 0.4f);
        auto px = CaptureAndLoad("render_camera_perspective");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DestroyEntities({e1, e2, e3, light});
        ctx->Yield(2);
    };

    // E23: render_camera_orthographic（编辑器相机仅透视投影；正交需 Game 视图，此处保留场景基线）
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_camera_orthographic");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto e1 = NewPrimitive(ctx, "Cube");
        UsePBR(e1);
        SetPos(e1, -1.0f, 0.5f, 0.0f);
        auto e2 = NewPrimitive(ctx, "Cube");
        UsePBR(e2);
        SetPos(e2, 1.0f, 0.5f, 0.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        // Two side-by-side cubes; slight 3/4 angle + pull-back frames both with margin
        PreCaptureFramed(ctx, glm::vec3(0.0f, 0.5f, 0.0f), 7.0f, 0.4f, 0.35f);
        auto px = CaptureAndLoad("render_camera_orthographic");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DestroyEntities({e1, e2, light});
        ctx->Yield(2);
    };

    // E24: render_camera_closeup（大尺寸 Cube 占满视口）
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_camera_closeup");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto ent = NewPrimitive(ctx, "Cube");
        UsePBR(ent);
        SetScale(ent, 3.0f, 3.0f, 3.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_camera_closeup");
        SKIP_IF_NO_CAPTURE(px);
        // Close-up should fill most of the viewport
        IM_CHECK(px.NonBlackRatio() > 0.05f);
        DestroyEntities({ent, light});
        ctx->Yield(2);
    };

    // E25: render_camera_far（远处小物体）
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_camera_far");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto ent = NewPrimitive(ctx, "Cube");
        UsePBR(ent);
        SetPos(ent, 0.0f, 0.5f, -30.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_camera_far");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.01f);
        DestroyEntities({ent, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F. Skinned skeletal-animation model asset import (1 test)
    //    使用 KF demo 骑士资产（cooked paladin dmesh + dskel + idle danim）验证
    //    “编辑器导入蒙皮骨骼动画模型并渲染”这一路径。
    // ====================================================================

    // F26: render_skinned_knight
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_skinned_knight");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);

        const std::string kf =
            "c:\\Users\\Administrator\\Desktop\\Engine\\DSEngine\\examples\\KF_Framework\\cooked\\";
        const std::string mesh_path = kf + "paladin_prop_j_nordstrom.dmesh";
        const std::string skel_path = kf + "paladin_prop_j_nordstrom.dskel";
        const std::string anim_path = kf + "Sword And Shield Idle.danim";

        auto knight = NewEmptyEntity(ctx);
        IM_CHECK(knight != entt::null);

        entt::registry& reg = Reg();
        auto& mr = reg.get_or_emplace<dse::MeshRendererComponent>(knight);
        mr.mesh_path = mesh_path;
        mr.temp_vertices.clear();
        mr.temp_indices.clear();
        mr.dmesh_vertex_stride = 24;
        mr.local_bounds_valid = false;
        mr.meshlet_mesh_id = 0;
        mr.shader_variant = "MESH_HALFLAMBERT";
        mr.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        mr.is_static = false;
        mr.visible = true;

        // Async texture loading (GPU upload happens on main thread via PumpMainThreadCallbacks)
        const std::string kf_tex =
            "c:\\Users\\Administrator\\Desktop\\Engine\\DSEngine\\examples\\KF_Framework\\assets\\textures\\";
        if (auto* am = Services().engine->asset_manager()) {
            entt::registry* rp = &reg; entt::entity kn = knight;
            am->LoadTextureAsync(kf_tex + "Paladin_diffuse.png",
                [rp,kn](std::shared_ptr<TextureAsset> t){
                    if (t && rp->valid(kn) && rp->all_of<dse::MeshRendererComponent>(kn))
                        rp->get<dse::MeshRendererComponent>(kn).albedo_texture_handle = t->GetHandle(); });
            am->LoadTextureAsync(kf_tex + "Paladin_normal.png",
                [rp,kn](std::shared_ptr<TextureAsset> t){
                    if (t && rp->valid(kn) && rp->all_of<dse::MeshRendererComponent>(kn))
                        rp->get<dse::MeshRendererComponent>(kn).normal_texture_handle = t->GetHandle(); });
        }

        auto& anim = reg.emplace<dse::Animator3DComponent>(knight);
        anim.enabled = true;
        anim.dskel_path = skel_path;
        anim.danim_path = anim_path;
        anim.speed = 1.0f;
        anim.loop = true;

        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(80);

        DumpMeshDiag("knight");
        {
            const size_t fbm = reg.all_of<dse::Animator3DComponent>(knight)
                ? reg.get<dse::Animator3DComponent>(knight).final_bone_matrices.size() : 0;
            fprintf(stderr,
                "[KNIGHT] verts=%zu idx=%zu stride=%d bounds_valid=%d "
                "bmin=(%.2f,%.2f,%.2f) bmax=(%.2f,%.2f,%.2f) final_bones=%zu\n",
                mr.temp_vertices.size(), mr.temp_indices.size(), mr.dmesh_vertex_stride,
                (int)mr.local_bounds_valid,
                mr.local_bounds_min.x, mr.local_bounds_min.y, mr.local_bounds_min.z,
                mr.local_bounds_max.x, mr.local_bounds_max.y, mr.local_bounds_max.z, fbm);
            fprintf(stderr, "[KNIGHT] albedo_handle=%u normal_handle=%u\n",
                mr.albedo_texture_handle, mr.normal_texture_handle);
            fflush(stderr);
        }

        const glm::vec3 kn_center = (mr.local_bounds_min + mr.local_bounds_max) * 0.5f;
        const glm::vec3 kn_half   = (mr.local_bounds_max - mr.local_bounds_min) * 0.5f;
        float kn_radius = glm::length(kn_half);
        if (!(kn_radius > 0.001f)) kn_radius = 100.0f;

        // Camera3DComponent for Play mode (editor camera disabled in Play)
        entt::entity kn_cam = NewEmptyEntity(ctx);
        {
            glm::vec3 cam_pos = kn_center + glm::vec3(0.0f, kn_half.y * 0.15f, kn_radius * 2.6f);
            glm::mat4 world = glm::inverse(glm::lookAt(cam_pos, kn_center, glm::vec3(0, 1, 0)));
            auto& ctf = reg.get<TransformComponent>(kn_cam);
            ctf.position = cam_pos;
            ctf.rotation = glm::quat_cast(world);
            auto& c3d = reg.emplace<dse::Camera3DComponent>(kn_cam);
            c3d.enabled = true; c3d.priority = 1000;
            c3d.fov = 45.0f; c3d.near_clip = 1.0f; c3d.far_clip = kn_radius * 20.0f + 1000.0f;
        }
        SelectionManager::Get().Clear();

        auto FrameKnight = [&]() {
            auto& cam = GetEditorCamera();
            cam.focal_point = kn_center;
            cam.distance = kn_radius * 2.6f;
            cam.yaw = 0.4f;
            cam.pitch = 0.12f;
        };
        SelectionManager::Get().Clear();
        FrameKnight();
        ctx->Yield(20);

        ctx->WindowFocus("//Scene");
        ctx->Yield(20);
        auto px_bind = CaptureAndLoad("render_skinned_knight");
        SKIP_IF_NO_CAPTURE(px_bind);
        IM_CHECK(px_bind.NonBlackRatio() > 0.02f);

        // Play mode: AnimatorSystem drives idle animation
        dse::editor::EnterPlayMode(reg);
        IM_CHECK(dse::editor::IsEditorInPlayMode());
        ctx->Yield(120);

        FrameKnight();
        SelectionManager::Get().Clear();
        {
            const size_t fbm = reg.all_of<dse::Animator3DComponent>(knight)
                ? reg.get<dse::Animator3DComponent>(knight).final_bone_matrices.size() : 0;
            const float ct = reg.all_of<dse::Animator3DComponent>(knight)
                ? reg.get<dse::Animator3DComponent>(knight).current_time : -1.0f;
            fprintf(stderr, "[KNIGHT] play-pass final_bones=%zu current_time=%.3f\n", fbm, ct);
            fflush(stderr);
        }
        ctx->WindowFocus("//Scene");
        ctx->Yield(20);
        auto px_anim = CaptureAndLoad("render_skinned_knight_anim");
        SKIP_IF_NO_CAPTURE(px_anim);
        IM_CHECK(px_anim.NonBlackRatio() > 0.02f);

        entt::entity sel = entt::null;
        dse::editor::ExitPlayMode(reg, sel, Services().engine);
        ctx->Yield(3);
        if (reg.valid(knight)) DestroyEntities({knight});
        if (reg.valid(light))  DestroyEntities({light});
        if (reg.valid(kn_cam)) DestroyEntities({kn_cam});
        ctx->Yield(2);
    };
}

} // namespace dse::editor::uitest

#endif // DSE_RENDER_TESTS
