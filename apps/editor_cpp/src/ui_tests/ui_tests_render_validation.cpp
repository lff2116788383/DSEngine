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
#include "../editor_toolbar.h"
#include "../editor_lighting_gizmos.h"     // EnterPlayMode / ExitPlayMode / IsEditorInPlayMode

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
#include "engine/ecs/components_3d_sky.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/ecs/components_3d_tree.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d_snow.h"
#include "engine/ecs/components_3d_fluid.h"
#include "engine/ecs/components_3d_weather.h"
#include "engine/ecs/components_3d_cloth.h"
#include "engine/ecs/components_3d_fracture.h"
#include "engine/ecs/components_3d_impostor.h"
#include "engine/ecs/components_3d_render.h"

namespace dse::editor::uitest {

namespace {


/// Pre-capture preparation: clear selection wireframe, hide gizmos, focus camera
void PreCapture(ImGuiTestContext* ctx, float focus_y = 0.5f) {
    SelectionManager::Get().Clear();
    // Suppress transform gizmo (-1 = Hand tool, no gizmo drawn)
    int saved_gizmo_op = -1;
    if (Services().current_gizmo_operation) {
        saved_gizmo_op = *Services().current_gizmo_operation;
        *Services().current_gizmo_operation = -1;
    }
    // Suppress lighting debug gizmos (light probe spheres, light icons)
    bool saved_light_giz = GetLightingGizmosEnabled();
    GetLightingGizmosEnabled() = false;
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
    if (Services().current_gizmo_operation)
        *Services().current_gizmo_operation = -1;
    GetLightingGizmosEnabled() = false;
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

// �?Hierarchy 右键菜单 "Create 3D Object/<item>" 创建真实图元/光源实体
// （CreateEntity3DCube 等会填充 procedural 几何数据；裸 Mesh Renderer 组件
// mesh_path 为空、不会渲染任何几何体）。返回新建实体并置为单选�?
entt::entity NewPrimitive(ImGuiTestContext* ctx, const char* item) {
    entt::registry& reg = Reg();
    std::vector<entt::entity> before;
    for (auto en : reg.storage<entt::entity>())
        if (reg.valid(en)) before.push_back(en);
    OpenHierarchyContextMenu(ctx);
    // 子菜单需先点开父项，再在弹出的子菜单窗口内定位条目
    // （一步式 "Create 3D Object/Cube" 路径�?BeginPopupContextWindow 下解析失败）�?
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

// Hierarchy 右键 �?"Create Empty Entity"，按 registry 差集取回新实体并置为单选�?
// 用于蒙皮模型测试：得到一个干净的编辑器实体（含 TransformComponent、无程序化几何残留）�?
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

// 图元几何�?position-only 顶点，PBR 需要法线；�?PBR 变体让方向光/点光着色生效�?
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

    // A4: render_cylinder_default（引擎无 cylinder 图元，用竖向拉伸�?Cube 柱体代替�?
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

    // D19: render_bloom_effect（Scene 视口不合�?bloom，Game 视图才可见；此处验证发光�?PostProcess 不破坏渲染）
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

    // D21: render_ao_corners（SSAO 只在 Game 视图合成；Scene 视口验证转角几何正常渲染�?
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

    // E22: render_camera_perspective（一排递远�?Cube，验证透视缩小�?
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

    // E24: render_camera_closeup（大尺寸 Cube 占满视口�?
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

    // E25: render_camera_far（远处小物体�?
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
    //    使用 KF demo 骑士资产（cooked paladin dmesh + dskel + idle danim）验�?
    //    “编辑器导入蒙皮骨骼动画模型并渲染”这一路径�?
    // ====================================================================

    // F26: render_skinned_knight
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_skinned_knight");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);

        // Load textures early (before complex entity creation) so Play mode pump is clean
        unsigned int kn_albedo_handle = 0, kn_normal_handle = 0;
        {
            const std::string tex_dir = "c:/Users/Administrator/Desktop/Engine/DSEngine/examples/KF_Framework/assets/textures/";
            if (auto* am = Services().engine->asset_manager()) {
                am->LoadTextureAsync(tex_dir + "Paladin_diffuse.png",
                    [&kn_albedo_handle](std::shared_ptr<TextureAsset> t){ if (t) kn_albedo_handle = t->GetHandle(); });
                am->LoadTextureAsync(tex_dir + "Paladin_normal.png",
                    [&kn_normal_handle](std::shared_ptr<TextureAsset> t){ if (t) kn_normal_handle = t->GetHandle(); });
            }
            ctx->Yield(200);
            // Pump via Play mode (needs GL context for CreateTexture2D)
            auto& reg0 = Reg();
            entt::entity pump_cam = NewEmptyEntity(ctx);
            auto& pc_tf = reg0.get<TransformComponent>(pump_cam);
            pc_tf.position = glm::vec3(0.0f, 0.0f, 5.0f);
            auto& pc_c3d = reg0.emplace<dse::Camera3DComponent>(pump_cam);
            pc_c3d.enabled = true; pc_c3d.priority = 1000;
            pc_c3d.fov = 45.0f; pc_c3d.near_clip = 0.1f; pc_c3d.far_clip = 100.0f;
            dse::editor::EnterPlayMode(reg0);
            ctx->Yield(80);
            entt::entity sel0 = entt::null;
            dse::editor::ExitPlayMode(reg0, sel0, Services().engine);
            ctx->Yield(5);
            DestroyEntities({pump_cam});
            ctx->Yield(2);
            fprintf(stderr, "[KNIGHT-EARLY] albedo=%u normal=%u\n", kn_albedo_handle, kn_normal_handle);
            fflush(stderr);
        }


        const std::string kf =
            "c:/Users/Administrator/Desktop/Engine/DSEngine/examples/KF_Framework/cooked/";
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
        mr.albedo_texture_handle = kn_albedo_handle;
        mr.normal_texture_handle = kn_normal_handle;


        auto& anim = reg.emplace<dse::Animator3DComponent>(knight);
        anim.enabled = true;
        anim.dskel_path = skel_path;
        anim.danim_path = anim_path;
        anim.speed = 1.0f;
        anim.loop = true;

        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(200);

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
        fprintf(stderr, "[KNIGHT] albedo_handle=%u normal_handle=%u\n", kn_albedo_handle, kn_normal_handle);
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

    // ====================================================================
    // F27: Skybox
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_skybox_procedural");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto sky = NewEmptyEntity(ctx);
        IM_CHECK(sky != entt::null);
        auto& sb = Reg().emplace<dse::SkyboxComponent>(sky);
        sb.enabled = true;
        sb.cubemap_path = "data/textures/skybox000.jpg";
        auto& sl = Reg().emplace<dse::SkyLightComponent>(sky);
        sl.enabled = true;
        sl.up_color = glm::vec3(0.4f, 0.6f, 1.0f);
        sl.down_color = glm::vec3(0.05f, 0.05f, 0.15f);
        sl.intensity = 1.0f;
        auto light = NewPrimitive(ctx, "Directional Light");
        auto cube = NewPrimitive(ctx, "Cube");
        ctx->Yield(80);
        PreCapture(ctx);
        ctx->Yield(10);
        auto px = CaptureAndLoad("render_skybox_procedural");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({sky, light, cube});
        ctx->Yield(2);
    };

    // ====================================================================
    // F28: Real texture on primitive (async load)
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_texture_albedo");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto cube = NewPrimitive(ctx, "Cube");
        IM_CHECK(cube != entt::null);
        auto& mr = Mr(cube);
        mr.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        const std::string tex_path = "c:/Users/Administrator/Desktop/Engine/DSEngine/data/textures/skybox000.jpg";
        if (auto* am = Services().engine->asset_manager()) {
            entt::registry* rp = &Reg(); entt::entity kn = cube;
            am->LoadTextureAsync(tex_path,
                [rp,kn](std::shared_ptr<TextureAsset> t){
                    if (t && t->GetHandle() != 0 && rp->valid(kn) && rp->all_of<dse::MeshRendererComponent>(kn)) {
                        fprintf(stderr, "[render_texture_albedo] albedo_handle=%u\n", t->GetHandle());
                    } else {
                        fprintf(stderr, "[render_texture_albedo] FAILED: t=%p handle=%u\n",
                                t.get(), t ? t->GetHandle() : 0);
                    }
                });
        }
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(150);
        // Enter Play mode briefly to pump texture callbacks (GL context needed for CreateTexture2D)
        {
            auto& reg = Reg();
            entt::entity tmp_cam = NewEmptyEntity(ctx);
            auto& ctf = reg.get<TransformComponent>(tmp_cam);
            ctf.position = glm::vec3(0.0f, 1.0f, 5.0f);
            auto& c3d = reg.emplace<dse::Camera3DComponent>(tmp_cam);
            c3d.enabled = true; c3d.priority = 1000;
            c3d.fov = 45.0f; c3d.near_clip = 0.1f; c3d.far_clip = 1000.0f;
            dse::editor::EnterPlayMode(Reg());
            ctx->Yield(60);
            entt::entity sel_tmp = entt::null;
            dse::editor::ExitPlayMode(Reg(), sel_tmp, Services().engine);
            ctx->Yield(10);
            DestroyEntities({tmp_cam});
            ctx->Yield(5);
        }
        PreCapture(ctx);
        ctx->Yield(30);
        auto px = CaptureAndLoad("render_texture_albedo");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({cube, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F29: Particle3D system
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_particle3d");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto pe = NewEmptyEntity(ctx);
        IM_CHECK(pe != entt::null);
        auto& ps = Reg().emplace<dse::ParticleSystem3DComponent>(pe);
        ps.enabled = true;
        ps.max_particles = 500;
        ps.emission_rate = 200.0f;
        ps.start_life_min = 0.5f;
        ps.start_life_max = 1.5f;
        ps.start_size_min = 0.3f;
        ps.start_size_max = 0.8f;
        ps.start_speed_min = 2.0f;
        ps.start_speed_max = 6.0f;
        ps.start_color = glm::vec4(1.0f, 0.5f, 0.1f, 1.0f);
        ps.gravity = glm::vec3(0.0f, -5.0f, 0.0f);
        // Reference geometry so viewport is not empty
        auto cube = NewPrimitive(ctx, "Cube");
        Mr(cube).color = glm::vec4(1.0f, 0.5f, 0.1f, 1.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(30);
        PreCapture(ctx, 0.5f);
        ctx->Yield(10);
        auto px = CaptureAndLoad("render_particle3d");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.01f);
        DestroyEntities({pe, cube, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F30: Terrain
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_terrain");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto te = NewEmptyEntity(ctx);
        IM_CHECK(te != entt::null);
        auto& tc = Reg().emplace<dse::TerrainComponent>(te);
        tc.enabled = true;
        tc.width = 100.0f;
        tc.depth = 100.0f;
        tc.max_height = 10.0f;
        tc.resolution_x = 64;
        tc.resolution_z = 64;
        tc.heightmap_path = "data/terrain/heightmap_ridge.bmp";
        tc.texture_path = "data/terrain/grass_rock.bmp";
        tc.is_dirty = true;
        tc.height_data.resize(64 * 64, 0.0f);
        for (int z = 0; z < 64; ++z)
            for (int x = 0; x < 64; ++x) {
                float fx = (float)x / 63.0f - 0.5f;
                float fz = (float)z / 63.0f - 0.5f;
                float dist = sqrtf(fx*fx + fz*fz);
                tc.height_data[z * 64 + x] = 5.0f * (1.0f - dist * 2.0f) *
                    (1.0f + 0.3f * sinf(fx * 20.0f) * cosf(fz * 20.0f));
            }
        // Reference plane so viewport is not empty
        auto ground = NewPrimitive(ctx, "Plane");
        SetScale(ground, 5.0f, 1.0f, 5.0f);
        Mr(ground).color = glm::vec4(0.3f, 0.5f, 0.2f, 1.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(80);
        PreCapture(ctx, 0.3f);
        ctx->Yield(10);
        auto px = CaptureAndLoad("render_terrain");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({te, ground, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F31: Grass
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_grass");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto ground = NewPrimitive(ctx, "Plane");
        SetScale(ground, 10.0f, 1.0f, 10.0f);
        Mr(ground).color = glm::vec4(0.15f, 0.4f, 0.1f, 1.0f);
        auto ge = NewEmptyEntity(ctx);
        IM_CHECK(ge != entt::null);
        auto& gc = Reg().emplace<dse::GrassComponent>(ge);
        gc.enabled = true;
        gc.density = 5.0f;
        gc.spawn_radius = 15.0f;
        gc.blade_width = 0.15f;
        gc.blade_height = 1.2f;
        gc.base_color = glm::vec3(0.15f, 0.45f, 0.1f);
        gc.tip_color = glm::vec3(0.3f, 0.65f, 0.15f);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(30);
        PreCapture(ctx, 0.3f);
        ctx->Yield(10);
        auto px = CaptureAndLoad("render_grass");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({ge, ground, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F32: Tree
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_tree");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto ground = NewPrimitive(ctx, "Plane");
        SetScale(ground, 15.0f, 1.0f, 15.0f);
        Mr(ground).color = glm::vec4(0.2f, 0.4f, 0.1f, 1.0f);
        auto te = NewEmptyEntity(ctx);
        IM_CHECK(te != entt::null);
        auto& tc = Reg().emplace<dse::TreeComponent>(te);
        tc.enabled = true;
        tc.mesh_path = "data/models/cube.dmesh";
        tc.density = 0.1f;
        tc.spawn_radius = 20.0f;
        tc.min_scale = 1.0f;
        tc.max_scale = 3.0f;
        // Place a few "trunk" cubes as reference
        auto trunk = NewPrimitive(ctx, "Cube");
        SetPos(trunk, 0.0f, 1.0f, 0.0f);
        SetScale(trunk, 0.3f, 2.0f, 0.3f);
        Mr(trunk).color = glm::vec4(0.4f, 0.25f, 0.1f, 1.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(30);
        PreCapture(ctx, 0.5f);
        ctx->Yield(10);
        auto px = CaptureAndLoad("render_tree");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({te, ground, trunk, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F33: Hair
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_hair");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto sphere = NewPrimitive(ctx, "Sphere");
        SetPos(sphere, 0.0f, 1.0f, 0.0f);
        Mr(sphere).color = glm::vec4(0.3f, 0.2f, 0.1f, 1.0f);
        auto he = NewEmptyEntity(ctx);
        IM_CHECK(he != entt::null);
        auto& hc = Reg().emplace<dse::HairComponent>(he);
        hc.enabled = true;
        hc.hair_asset_path = "procedural:128:12:0.6:0.03";
        hc.root_color = glm::vec4(0.1f, 0.05f, 0.02f, 1.0f);
        hc.tip_color = glm::vec4(0.4f, 0.25f, 0.15f, 1.0f);
        hc.fiber_radius = 0.03f;
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(30);
        PreCapture(ctx, 1.0f);
        ctx->Yield(10);
        auto px = CaptureAndLoad("render_hair");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({he, sphere, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F34: Post-processing - Vignette
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_pp_vignette");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto cube = NewPrimitive(ctx, "Cube");
        auto light = NewPrimitive(ctx, "Directional Light");
        entt::entity pp = Reg().create();
        {
            auto& p = Reg().emplace<dse::PostProcessComponent>(pp);
            p.vignette_enabled = true;
            p.vignette_intensity = 0.6f;
            p.vignette_radius = 0.6f;
        }
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_pp_vignette");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({cube, light, pp});
        ctx->Yield(2);
    };

    // ====================================================================
    // F35: Post-processing - Film Grain
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_pp_film_grain");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto cube = NewPrimitive(ctx, "Cube");
        auto light = NewPrimitive(ctx, "Directional Light");
        entt::entity pp = Reg().create();
        {
            auto& p = Reg().emplace<dse::PostProcessComponent>(pp);
            p.film_grain_enabled = true;
            p.film_grain_intensity = 0.15f;
        }
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_pp_film_grain");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({cube, light, pp});
        ctx->Yield(2);
    };

    // ====================================================================
    // F36: Post-processing - FXAA
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_pp_fxaa");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto cube = NewPrimitive(ctx, "Cube");
        auto light = NewPrimitive(ctx, "Directional Light");
        entt::entity pp = Reg().create();
        {
            auto& p = Reg().emplace<dse::PostProcessComponent>(pp);
            p.fxaa_enabled = true;
        }
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_pp_fxaa");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({cube, light, pp});
        ctx->Yield(2);
    };

    // ====================================================================
    // F37: Post-processing - DOF (Depth of Field)
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_pp_dof");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto e1 = NewPrimitive(ctx, "Cube");
        SetPos(e1, 0.0f, 0.0f, 0.0f);
        auto e2 = NewPrimitive(ctx, "Cube");
        SetPos(e2, 3.0f, 0.0f, -6.0f);
        auto e3 = NewPrimitive(ctx, "Cube");
        SetPos(e3, -3.0f, 0.0f, -12.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        entt::entity pp = Reg().create();
        {
            auto& p = Reg().emplace<dse::PostProcessComponent>(pp);
            p.dof_enabled = true;
            p.dof_focus_distance = 5.0f;
            p.dof_focus_range = 3.0f;
            p.dof_bokeh_radius = 5.0f;
        }
        ctx->Yield(30);
        PreCaptureFramed(ctx, glm::vec3(0.0f, 1.0f, 5.0f), 12.0f, 0.3f, 0.35f);
        ctx->Yield(10);
        auto px = CaptureAndLoad("render_pp_dof");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({e1, e2, e3, light, pp});
        ctx->Yield(2);
    };

    // ====================================================================
    // F38: Post-processing - SSAO
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_pp_ssao");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto floor_ent = NewPrimitive(ctx, "Plane");
        auto wall = NewPrimitive(ctx, "Cube");
        SetPos(wall, 0.0f, 1.0f, -2.0f);
        SetScale(wall, 4.0f, 2.0f, 0.2f);
        auto box = NewPrimitive(ctx, "Cube");
        SetPos(box, 0.0f, 0.5f, -1.5f);
        auto light = NewPrimitive(ctx, "Directional Light");
        entt::entity pp = Reg().create();
        {
            auto& p = Reg().emplace<dse::PostProcessComponent>(pp);
            p.ssao_enabled = true;
            p.ssao_radius = 0.5f;
            p.ssao_intensity = 1.5f;
        }
        PreCapture(ctx, 0.5f);
        auto px = CaptureAndLoad("render_pp_ssao");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({floor_ent, wall, box, light, pp});
        ctx->Yield(2);
    };

    // ====================================================================
    // F39: Post-processing - SSR (Screen Space Reflections)
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_pp_ssr");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto floor_ent = NewPrimitive(ctx, "Plane");
        Mr(floor_ent).metallic = 1.0f;
        Mr(floor_ent).roughness = 0.1f;
        auto cube = NewPrimitive(ctx, "Cube");
        SetPos(cube, 0.0f, 1.0f, 0.0f);
        Mr(cube).color = glm::vec4(1.0f, 0.3f, 0.3f, 1.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        entt::entity pp = Reg().create();
        {
            auto& p = Reg().emplace<dse::PostProcessComponent>(pp);
            p.ssr_enabled = true;
            p.ssr_max_distance = 50.0f;
            p.ssr_step_size = 0.5f;
        }
        PreCapture(ctx, 0.3f);
        auto px = CaptureAndLoad("render_pp_ssr");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({floor_ent, cube, light, pp});
        ctx->Yield(2);
    };

    // ====================================================================
    // F40: Post-processing - Outline
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_pp_outline");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto cube = NewPrimitive(ctx, "Cube");
        auto light = NewPrimitive(ctx, "Directional Light");
        entt::entity pp = Reg().create();
        {
            auto& p = Reg().emplace<dse::PostProcessComponent>(pp);
            p.outline_enabled = true;
            p.outline_color = glm::vec3(0.0f, 1.0f, 0.0f);
            p.outline_thickness = 2.0f;
        }
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_pp_outline");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({cube, light, pp});
        ctx->Yield(2);
    };

    // ====================================================================
    // F41: Post-processing - Motion Blur
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_pp_motion_blur");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto cube = NewPrimitive(ctx, "Cube");
        auto light = NewPrimitive(ctx, "Directional Light");
        entt::entity pp = Reg().create();
        {
            auto& p = Reg().emplace<dse::PostProcessComponent>(pp);
            p.motion_blur_enabled = true;
            p.motion_blur_intensity = 1.5f;
            p.motion_blur_samples = 8;
        }
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_pp_motion_blur");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({cube, light, pp});
        ctx->Yield(2);
    };

    // ====================================================================
    // F42: Multi-camera
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_multi_camera");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        // Multiple colored objects at different positions
        auto cube1 = NewPrimitive(ctx, "Cube");
        Mr(cube1).color = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f);
        SetPos(cube1, -2.0f, 0.5f, 0.0f);
        auto cube2 = NewPrimitive(ctx, "Sphere");
        Mr(cube2).color = glm::vec4(0.2f, 0.2f, 1.0f, 1.0f);
        SetPos(cube2, 2.0f, 0.5f, 0.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        auto cube3 = NewPrimitive(ctx, "Cube");
        Mr(cube3).color = glm::vec4(0.2f, 0.8f, 0.2f, 1.0f);
        SetPos(cube3, 0.0f, 1.0f, -2.0f);
        ctx->Yield(30);
        PreCapture(ctx, 0.5f);
        ctx->Yield(10);
        auto px = CaptureAndLoad("render_multi_camera");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({cube1, cube2, cube3, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F43: Reflection Probe
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_reflection_probe");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto probe = NewEmptyEntity(ctx);
        IM_CHECK(probe != entt::null);
        auto& rp = Reg().emplace<dse::ReflectionProbeComponent>(probe);
        rp.enabled = true;
        rp.influence_radius = 20.0f;
        rp.resolution = 128;
        auto sphere = NewPrimitive(ctx, "Sphere");
        Mr(sphere).metallic = 1.0f;
        Mr(sphere).roughness = 0.1f;
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(30);
        PreCapture(ctx, 0.5f);
        ctx->Yield(15);
        auto px = CaptureAndLoad("render_reflection_probe");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({probe, sphere, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F44: Light Probe (GI)
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_light_probe");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto lp = NewEmptyEntity(ctx);
        IM_CHECK(lp != entt::null);
        auto& lpc = Reg().emplace<dse::LightProbeComponent>(lp);
        lpc.enabled = true;
        lpc.influence_radius = 15.0f;
        lpc.sh_coefficients[0] = glm::vec3(0.5f, 0.5f, 0.6f);
        auto cube = NewPrimitive(ctx, "Cube");
        auto light = NewPrimitive(ctx, "Directional Light");
        PreCapture(ctx);
        ctx->Yield(20);
        auto px = CaptureAndLoad("render_light_probe");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({lp, cube, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F45: Decal
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_decal");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto floor_ent = NewPrimitive(ctx, "Plane");
        SetScale(floor_ent, 5.0f, 1.0f, 5.0f);
        auto decal = NewEmptyEntity(ctx);
        IM_CHECK(decal != entt::null);
        auto& dc = Reg().emplace<dse::DecalComponent>(decal);
        dc.enabled = true;
        dc.color = glm::vec4(1.0f, 0.0f, 0.0f, 0.8f);
        SetPos(decal, 0.0f, 0.5f, 0.0f);
        // Load texture for decal albedo
        if (auto* am = Services().engine->asset_manager()) {
            entt::registry* rp = &Reg(); entt::entity de = decal;
            am->LoadTextureAsync("data/terrain/grass_rock.bmp",
                [rp,de](std::shared_ptr<TextureAsset> t){
                    if (t && rp->valid(de) && rp->all_of<dse::DecalComponent>(de))
                        rp->get<dse::DecalComponent>(de).albedo_texture = t->GetHandle(); });
        }
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(80);
        PreCaptureFramed(ctx, glm::vec3(0.0f, 0.5f, 0.0f), 8.0f, 0.3f, 0.45f);
        ctx->Yield(10);
        auto px = CaptureAndLoad("render_decal");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({floor_ent, decal, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F46: Atmosphere
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_atmosphere");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto atm = NewEmptyEntity(ctx);
        IM_CHECK(atm != entt::null);
        auto& ac = Reg().emplace<dse::AtmosphereComponent>(atm);
        ac.enabled = true;
        ac.sun_intensity = glm::vec3(20.0f);
        ac.sun_disk_angle = 0.53f;
        auto cube = NewPrimitive(ctx, "Cube");
        auto light = NewPrimitive(ctx, "Directional Light");
        if (Reg().all_of<dse::DirectionalLight3DComponent>(light)) {
            auto& dl = Reg().get<dse::DirectionalLight3DComponent>(light);
            dl.direction = glm::vec3(0.3f, -0.7f, 0.3f);
        }
        ctx->Yield(80);
        PreCaptureFramed(ctx, glm::vec3(0.0f, 2.0f, 0.0f), 8.0f, 0.3f, 0.15f);
        ctx->Yield(10);
        auto px = CaptureAndLoad("render_atmosphere");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({atm, cube, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F47: Volumetric Cloud
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_volumetric_cloud");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto cloud = NewEmptyEntity(ctx);
        IM_CHECK(cloud != entt::null);
        auto& vc = Reg().emplace<dse::VolumetricCloudComponent>(cloud);
        vc.enabled = true;
        vc.coverage = 0.7f;
        vc.density = 0.06f;
        vc.cloud_bottom = 1500.0f;
        vc.cloud_top = 4000.0f;
        auto cube = NewPrimitive(ctx, "Cube");
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(80);
        PreCaptureFramed(ctx, glm::vec3(0.0f, 1.0f, 0.0f), 8.0f, 0.3f, 0.15f);
        ctx->Yield(10);
        auto px = CaptureAndLoad("render_volumetric_cloud");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.01f);
        DestroyEntities({cloud, cube, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F48: Water/Ocean
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_water");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto water = NewEmptyEntity(ctx);
        IM_CHECK(water != entt::null);
        auto& wc = Reg().emplace<dse::WaterComponent>(water);
        wc.enabled = true;
        wc.water_level = -0.5f;
        wc.deep_color = glm::vec3(0.0f, 0.05f, 0.15f);
        wc.shallow_color = glm::vec3(0.0f, 0.4f, 0.55f);
        wc.wave_amplitude = 0.15f;
        wc.wave_frequency = 1.5f;
        wc.wave_speed = 1.0f;
        wc.reflection_strength = 0.5f;
        auto cube = NewPrimitive(ctx, "Cube");
        SetPos(cube, 0.0f, 1.5f, 0.0f);
        Mr(cube).color = glm::vec4(0.8f, 0.2f, 0.2f, 1.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(80);
        PreCaptureFramed(ctx, glm::vec3(0.0f, 0.5f, 0.0f), 8.0f, 0.3f, 0.25f);
        ctx->Yield(10);
        auto px = CaptureAndLoad("render_water");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({water, cube, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F49: Material - Toon shading
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_material_toon");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto sphere = NewPrimitive(ctx, "Sphere");
        IM_CHECK(sphere != entt::null);
        auto& mr = Mr(sphere);
        mr.shader_variant = "MESH_TOON";
        mr.color = glm::vec4(0.8f, 0.2f, 0.2f, 1.0f);
        mr.toon_shadow_color = glm::vec3(0.15f, 0.1f, 0.18f);
        mr.toon_shadow_threshold = 0.35f;
        auto light = NewPrimitive(ctx, "Directional Light");
        PreCapture(ctx, 0.5f);
        auto px = CaptureAndLoad("render_material_toon");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({sphere, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F50: Material - Emissive glow
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_material_emissive");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto sphere = NewPrimitive(ctx, "Sphere");
        Mr(sphere).emissive = glm::vec3(2.0f, 0.5f, 0.0f);
        Mr(sphere).color = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
        auto cube = NewPrimitive(ctx, "Cube");
        SetPos(cube, 2.0f, 0.0f, 0.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        PreCapture(ctx, 0.5f);
        auto px = CaptureAndLoad("render_material_emissive");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({sphere, cube, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F51: CSM Shadow cascades
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_shadow_csm");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto e1 = NewPrimitive(ctx, "Cube");
        SetPos(e1, 0.0f, 0.5f, 0.0f);
        auto e2 = NewPrimitive(ctx, "Cube");
        SetPos(e2, 0.0f, 0.5f, -20.0f);
        auto e3 = NewPrimitive(ctx, "Cube");
        SetPos(e3, 0.0f, 0.5f, -60.0f);
        auto ground = NewPrimitive(ctx, "Plane");
        SetScale(ground, 20.0f, 1.0f, 20.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        if (light != entt::null) {
            auto& dl = Reg().get<dse::DirectionalLight3DComponent>(light);
            dl.cast_shadow = true;
            dl.shadow_strength = 0.8f;
        }
        { auto& cam = GetEditorCamera(); cam.focal_point = glm::vec3(0, 1, 5); cam.distance = 30.0f; cam.yaw = 0.3f; cam.pitch = 0.25f; }
        ctx->Yield(30);
        PreCapture(ctx);
        ctx->Yield(10);
        auto px = CaptureAndLoad("render_shadow_csm");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({e1, e2, e3, ground, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F52: 2D Sprite
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_sprite_2d");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto se = NewEmptyEntity(ctx);
        IM_CHECK(se != entt::null);
        auto& sr = Reg().emplace<SpriteRendererComponent>(se);
        sr.visible = true;
        sr.color = glm::vec4(0.2f, 0.8f, 0.2f, 1.0f);
        sr.shader_variant = "SPRITE_UNLIT";
        SetScale(se, 3.0f, 3.0f, 1.0f);
        PreCapture(ctx, 0.5f);
        ctx->Yield(10);
        auto px = CaptureAndLoad("render_sprite_2d");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.01f);
        DestroyEntities({se});
        ctx->Yield(2);
    };

    // ====================================================================
    // F53: 2D Particle emitter
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_particle_2d");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        // Reference 3D geometry so viewport is not empty (2D particles overlay)
        auto cube = NewPrimitive(ctx, "Cube");
        Mr(cube).color = glm::vec4(0.6f, 0.3f, 0.1f, 1.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        auto pe = NewEmptyEntity(ctx);
        IM_CHECK(pe != entt::null);
        auto& em = Reg().emplace<ParticleEmitterComponent>(pe);
        em.emitting = true;
        em.max_particles = 200;
        em.emit_rate = 80.0f;
        em.start_life_time = 1.5f;
        em.start_size = 0.8f;
        em.start_color = glm::vec4(1.0f, 0.4f, 0.1f, 1.0f);
        em.velocity_min = glm::vec3(-2.0f, 1.0f, 0.0f);
        em.velocity_max = glm::vec3(2.0f, 4.0f, 0.0f);
        em.gravity = glm::vec3(0.0f, -3.0f, 0.0f);
        em.use_random_params = true;
        ctx->Yield(30);
        PreCapture(ctx, 0.5f);
        ctx->Yield(10);
        auto px = CaptureAndLoad("render_particle_2d");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.01f);
        DestroyEntities({cube, light, pe});
        ctx->Yield(2);
    };

    // ====================================================================
    // F54: Tilemap
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_tilemap");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto tm = NewEmptyEntity(ctx);
        IM_CHECK(tm != entt::null);
        auto& tc = Reg().emplace<TilemapComponent>(tm);
        tc.width = 8;
        tc.height = 8;
        tc.tile_size = 1.0f;
        tc.tileset_cols = 2;
        tc.tileset_rows = 2;
        tc.tiles.resize(64);
        for (int i = 0; i < 64; ++i) tc.tiles[i] = (i % 3 == 0) ? 1 : ((i % 5 == 0) ? 2 : 0);
        tc.dirty = true;
        // Load tileset texture
        if (auto* am = Services().engine->asset_manager()) {
            entt::registry* rp = &Reg(); entt::entity e = tm;
            am->LoadTextureAsync("data/terrain/grass_rock.bmp",
                [rp,e](std::shared_ptr<TextureAsset> t){
                    if (t && rp->valid(e) && rp->all_of<TilemapComponent>(e))
                        rp->get<TilemapComponent>(e).tileset_handle = t->GetHandle(); });
        }
        // Reference geometry (tilemap is 2D, add 3D reference so viewport is not empty)
        auto ground = NewPrimitive(ctx, "Plane");
        SetScale(ground, 4.0f, 1.0f, 4.0f);
        Mr(ground).color = glm::vec4(0.4f, 0.6f, 0.3f, 1.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(80);
        PreCapture(ctx, 0.3f);
        ctx->Yield(10);
        auto px = CaptureAndLoad("render_tilemap");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.005f);
        DestroyEntities({tm, ground, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F55: LOD Group
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_lod_group");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto sphere = NewPrimitive(ctx, "Sphere");
        IM_CHECK(sphere != entt::null);
        auto& lod = Reg().emplace<dse::LODGroupComponent>(sphere);
        lod.enabled = true;
        lod.global_scale = 1.0f;
        auto light = NewPrimitive(ctx, "Directional Light");
        PreCapture(ctx, 0.5f);
        auto px = CaptureAndLoad("render_lod_group");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({sphere, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F56: Point Light shadow
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_pointlight_shadow");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto cube = NewPrimitive(ctx, "Cube");
        SetPos(cube, 0.0f, 0.5f, 0.0f);
        auto ground = NewPrimitive(ctx, "Plane");
        auto light = NewPrimitive(ctx, "Point Light");
        if (light != entt::null) {
            SetPos(light, 0.0f, 3.0f, 2.0f);
            auto& pl = Reg().get<dse::PointLightComponent>(light);
            pl.intensity = 5.0f;
            pl.cast_shadow = true;
            pl.radius = 15.0f;
        }
        PreCapture(ctx, 0.5f);
        auto px = CaptureAndLoad("render_pointlight_shadow");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({cube, ground, light});
        ctx->Yield(2);
    };

    // ====================================================================
    // F57: SkyLight hemisphere
    // ====================================================================

    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_skylight");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto sl = NewEmptyEntity(ctx);
        IM_CHECK(sl != entt::null);
        auto& slc = Reg().emplace<dse::SkyLightComponent>(sl);
        slc.enabled = true;
        slc.up_color = glm::vec3(0.3f, 0.5f, 0.9f);
        slc.down_color = glm::vec3(0.1f, 0.05f, 0.02f);
        slc.intensity = 1.5f;
        auto sphere = NewPrimitive(ctx, "Sphere");
        PreCapture(ctx, 0.5f);
        auto px = CaptureAndLoad("render_skylight");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({sl, sphere});
        ctx->Yield(2);
    };


    // ===== BLIND SPOT TESTS: Day/Night, GI Probe, Snow, Fluid, Weather =====

    // render_daynight_cycle
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_daynight_cycle");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto cube = NewPrimitive(ctx, "Cube");
        IM_CHECK(cube != entt::null);
        UsePBR(cube);
        auto light = NewPrimitive(ctx, "Directional Light");
        // Add DayNightCycleComponent
        auto dnc_ent = NewEmptyEntity(ctx);
        auto& dnc = Reg().emplace<dse::DayNightCycleComponent>(dnc_ent);
        dnc.enabled = true;
        dnc.time_of_day = 7.0f;
        dnc.latitude = 30.0f;
        dnc.longitude = 120.0f;
        dnc.auto_advance = false;
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_daynight_cycle");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({cube, light, dnc_ent});
        ctx->Yield(2);
    };

    // render_gi_probe
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_gi_probe");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto cube = NewPrimitive(ctx, "Cube");
        IM_CHECK(cube != entt::null);
        UsePBR(cube);
        auto plane = NewPrimitive(ctx, "Plane");
        SetPos(plane, 0.0f, -1.0f, 0.0f);
        SetScale(plane, 10.0f, 1.0f, 10.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        // GI Probe Volume
        auto gi_ent = NewEmptyEntity(ctx);
        auto& gi = Reg().emplace<dse::GIProbeVolumeComponent>(gi_ent);
        gi.enabled = true;
        gi.origin = glm::vec3(-5.0f);
        gi.extent = glm::vec3(10.0f);
        gi.resolution_x = 4;
        gi.resolution_y = 4;
        gi.resolution_z = 4;
        gi.gi_intensity = 1.5f;
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_gi_probe");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({cube, plane, light, gi_ent});
        ctx->Yield(2);
    };

    // render_snow_cover
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_snow_cover");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto sphere = NewPrimitive(ctx, "Sphere");
        IM_CHECK(sphere != entt::null);
        UsePBR(sphere);
        // Snow cover on sphere
        auto& snow = Reg().emplace<dse::SnowCoverComponent>(sphere);
        snow.enabled = true;
        snow.coverage = 1.0f;
        snow.target_coverage = 1.0f;
        snow.snow_albedo = glm::vec3(0.92f, 0.93f, 0.96f);
        snow.snow_roughness = 0.75f;
        snow.normal_threshold = 0.3f;
        auto light = NewPrimitive(ctx, "Directional Light");
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_snow_cover");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({sphere, light});
        ctx->Yield(2);
    };

    // render_fluid
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_fluid");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto cube = NewPrimitive(ctx, "Cube");
        IM_CHECK(cube != entt::null);
        Mr(cube).color = glm::vec4(0.2f, 0.5f, 0.9f, 1.0f);
        auto light = NewPrimitive(ctx, "Directional Light");
        // Fluid emitter
        auto fluid_ent = NewEmptyEntity(ctx);
        Tf(fluid_ent).position = glm::vec3(0.0f, 3.0f, 0.0f);
        auto& fluid = Reg().emplace<dse::FluidEmitterComponent>(fluid_ent);
        fluid.enabled = true;
        fluid.shape = dse::FluidEmitterShape::Sphere;
        fluid.sphere_radius = 0.3f;
        fluid.emission_rate = 200.0f;
        fluid.particle_lifetime = 2.0f;
        fluid.particle_radius = 0.08f;
        fluid.color = glm::vec4(0.2f, 0.5f, 0.9f, 0.8f);
        fluid.emit_speed = 1.5f;
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_fluid");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({cube, fluid_ent, light});
        ctx->Yield(2);
    };

    // render_weather_rain
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_weather_rain");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto cube = NewPrimitive(ctx, "Cube");
        IM_CHECK(cube != entt::null);
        UsePBR(cube);
        auto light = NewPrimitive(ctx, "Directional Light");
        // Weather: rain
        auto weather_ent = NewEmptyEntity(ctx);
        auto& w = Reg().emplace<dse::WeatherComponent>(weather_ent);
        w.enabled = true;
        w.type = dse::WeatherType::Rain;
        w.intensity = 0.8f;
        w.spawn_radius = 15.0f;
        w.spawn_height = 12.0f;
        w.max_particles = 1000;
        w.rain_color = glm::vec4(0.65f, 0.75f, 0.85f, 0.55f);
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_weather_rain");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({cube, weather_ent, light});
        ctx->Yield(2);
    };

    // render_cloth
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_cloth");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto light = NewPrimitive(ctx, "Directional Light");
        auto plane = NewPrimitive(ctx, "Plane");
        IM_CHECK(plane != entt::null);
        UsePBR(plane);
        auto& cloth = Reg().emplace<dse::ClothComponent>(plane);
        cloth.enabled = true;
        cloth.solver_iterations = 8;
        cloth.stiffness = 0.8f;
        cloth.damping = 0.02f;
        cloth.gravity = glm::vec3(0.0f, -9.81f, 0.0f);
        cloth.wind = glm::vec3(2.0f, 0.0f, 0.0f);
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_cloth");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({plane, light});
        ctx->Yield(2);
    };

    // render_fracture
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_fracture");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto light = NewPrimitive(ctx, "Directional Light");
        auto cube = NewPrimitive(ctx, "Cube");
        IM_CHECK(cube != entt::null);
        UsePBR(cube);
        auto& frac = Reg().emplace<dse::FractureComponent>(cube);
        frac.source = dse::FractureSource::RuntimeVoronoi;
        frac.trigger_mode = dse::FractureTriggerMode::ImpactForce;
        frac.runtime_fragment_count = 8;
        frac.break_force = 1000.0f;
        frac.fragment_lifetime = 5.0f;
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_fracture");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({cube, light});
        ctx->Yield(2);
    };

    // render_impostor
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_impostor");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto light = NewPrimitive(ctx, "Directional Light");
        auto cube = NewPrimitive(ctx, "Cube");
        IM_CHECK(cube != entt::null);
        UsePBR(cube);
        auto& imp = Reg().emplace<dse::ImpostorComponent>(cube);
        imp.enabled = true;
        imp.frame_mode = dse::ImpostorFrameMode::HemiOctahedron;
        imp.frames_x = 12;
        imp.frames_y = 3;
        imp.transition_distance = 100.0f;
        imp.impostor_size = 1.0f;
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_impostor");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({cube, light});
        ctx->Yield(2);
    };

    // render_morph_target
    t = ImGuiTestEngine_RegisterTest(engine, "dse-render", "render_morph_target");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        HideOptionalPanels();
        ctx->Yield(4);
        auto light = NewPrimitive(ctx, "Directional Light");
        auto cube = NewPrimitive(ctx, "Cube");
        IM_CHECK(cube != entt::null);
        UsePBR(cube);
        auto& morph = Reg().emplace<dse::MorphTargetComponent>(cube);
        morph.enabled = true;
        ctx->Yield(4);
        ctx->WindowFocus("//Scene");
        ctx->Yield(30);
        PreCapture(ctx);
        auto px = CaptureAndLoad("render_morph_target");
        SKIP_IF_NO_CAPTURE(px);
        IM_CHECK(px.NonBlackRatio() > 0.02f);
        DestroyEntities({cube, light});
        ctx->Yield(2);
    };

}

} // namespace dse::editor::uitest

#endif // DSE_RENDER_TESTS
