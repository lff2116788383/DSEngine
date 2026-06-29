/**
 * @file ui_tests_gizmo.cpp
 * @brief ImGuizmo 视口变换控制 + 视口内点选/拖拽用例（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 大部分用例覆盖 gizmo 的「操作模式 / 坐标系 / 吸附步长」这些可确定的控制路径（经 View 菜单与
 * 视口工具栏切换，断言 current_gizmo_operation/mode 与 GetSnap* 真改变）。
 *
 * 另外两个用例在视口内做「真实鼠标拖拽」，靠把编辑器相机复位到聚焦原点的确定取景做到无头可复现：
 * lookAt 使 focal_point 必投到视口内容矩形中心，故放在世界原点的实体精确投到中心，落点可计算——
 *   · viewport_marquee_select：从视口空白角拖出框选矩形覆盖该实体 → 断言实体被选中（投影框选，
 *     纯几何、不依赖渲染/颜色 ID 拾取）。
 *   · gizmo_translate_drag：选中该实体、Translate 模式，从 gizmo 中心拖动 → 断言 Transform 真改变
 *     且压入一条「Gizmo Transform」可撤销命令、Undo 能复位（覆盖 ImGuizmo::Manipulate 写回 + 撤销接线）。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cmath>
#include <cstdio>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include <string>

#include "imgui_internal.h"            // ImGuiWindow / ImRect（取 Scene 内容矩形中心）

#include "../editor_preferences_panel.h"  // GetSnapTranslate
#include "../editor_selection.h"
#include "../editor_icons.h"              // MDI_ICON_ARROW_ALL / ROTATE_3D_VARIANT / RESIZE
#include "../editor_scene_camera.h"      // GetEditorCamera（确定性取景）
#include "../editor_shortcuts.h"         // GetUndoRedoManager
#include "../editor_scene_tabs.h"        // SceneTabManager（新建空场景页签隔离）
#include "../editor_multi_viewport.h"   // GetMultiViewportState（复位多视口分屏）
#include "../editor_collider_edit.h"    // GetColliderEditEnabled
#include "../editor_lighting_gizmos.h"  // GetLightingGizmosEnabled

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"        // TransformComponent

#include <vector>

namespace dse::editor::uitest {

namespace {

entt::registry& Reg() { return Services().engine->pipeline()->world().registry(); }

// 经 Hierarchy 右键「Create Empty Entity」建实体（创建即选中 → 置 EditorContext.selected_entity，
// 故视口 gizmo 单选分支可命中），按 registry 差集取回新实体。
entt::entity CreateSelectedEntity(ImGuiTestContext* ctx) {
    entt::registry& reg = Reg();
    std::vector<entt::entity> before;
    for (auto en : reg.storage<entt::entity>())
        if (reg.valid(en)) before.push_back(en);
    OpenHierarchyContextMenu(ctx);
    ctx->ItemClick("Create Empty Entity");
    ctx->Yield();
    for (auto en : reg.storage<entt::entity>()) {
        if (!reg.valid(en)) continue;
        bool seen = false;
        for (auto b : before) if (b == en) { seen = true; break; }
        if (!seen) return en;
    }
    return entt::null;
}

// 把编辑器相机复位到聚焦原点的确定取景。GetViewMatrix 用 lookAt(pos, focal_point=原点)，故无论
// yaw/pitch/distance/aspect 如何，世界原点都投到视口内容矩形中心——放在原点的实体落点可计算。
void ResetEditorCameraToOrigin() {
    auto& cam = dse::editor::GetEditorCamera();
    cam.focal_point = glm::vec3(0.0f);
    cam.yaw = 0.0f;
    cam.pitch = 0.3f;
    cam.distance = 10.0f;
}

// 把测试实体的 Transform 置于世界原点（确保它投到视口中心），推几帧让 transform 系统刷新 local_to_world。
void PlaceAtOriginAndSettle(ImGuiTestContext* ctx, entt::entity ent) {
    entt::registry& reg = Reg();
    if (reg.valid(ent) && reg.all_of<TransformComponent>(ent)) {
        auto& tf = reg.get<TransformComponent>(ent);
        tf.position = glm::vec3(0.0f);
        tf.dirty = true;
    }
    ctx->Yield(3);
}

// 把视口相关的全局开关复位到确定状态：关多视口分屏 / 碰撞体编辑 / 光照 gizmo 覆盖。否则
// 前序用例遗留这些状态会改变 Scene 视口的点选/gizmo 交互分支，使按屏幕坐标投递的无头拖拽落空。
void ForceDeterministicViewState() {
    GetMultiViewportState().enabled = false;
    GetColliderEditEnabled() = false;
    GetLightingGizmosEnabled() = false;
}

// 新建空场景页签做隔离：清掉前序用例在场景里累积的实体（它们带 Transform，会被框选误命中、
// 或令活动选择/删除落到别的实体上）。返回隔离前页签数，配 ExitIsolatedScene 复位。
void EnterIsolatedScene(ImGuiTestContext* ctx, int& out_n0) {
    HideOptionalPanels();
    ctx->Yield(4);
    SceneTabManager& tabs = SceneTabManager::Get();
    out_n0 = tabs.GetTabCount();
    ctx->ItemClick("//DSEngineRoot/##SceneTabs/+");
    ctx->Yield(2);
    IM_CHECK_EQ(tabs.GetTabCount(), out_n0 + 1);
}

// 右键关掉隔离页签 → 恢复原场景快照（测试实体随该页签一并丢弃，无污染）。
void ExitIsolatedScene(ImGuiTestContext* ctx, int n0) {
    SceneTabManager& tabs = SceneTabManager::Get();
    const int last = tabs.GetTabCount() - 1;
    char tabref[80];
    std::snprintf(tabref, sizeof(tabref), "//DSEngineRoot/##SceneTabs/SceneTab%d", tabs.GetTabId(last));
    ctx->ItemClick(tabref, ImGuiMouseButton_Right);
    ctx->Yield();
    ctx->SetRef("//$FOCUSED");
    ctx->ItemClick("Close");
    ctx->Yield(2);
    DiscardSceneCloseConfirmIfOpen(ctx);
    IM_CHECK_EQ(tabs.GetTabCount(), n0);
}

} // namespace

void RegisterGizmoTests(ImGuiTestEngine* e) {
    // dse-gizmo/operation_via_menu：View 菜单切换 Translate/Rotate/Scale → 断言 operation 真改变。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-gizmo", "operation_via_menu");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            int* op = Services().current_gizmo_operation;
            IM_CHECK(op != nullptr);
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("View/Gizmo: Rotate");
            ctx->Yield();
            IM_CHECK_EQ(*op, 1);
            ctx->MenuClick("View/Gizmo: Scale");
            ctx->Yield();
            IM_CHECK_EQ(*op, 2);
            ctx->MenuClick("View/Gizmo: Translate");
            ctx->Yield();
            IM_CHECK_EQ(*op, 0);
        };
    }

    // dse-gizmo/coordinate_space_via_menu：View 菜单切换 World/Local Space → 断言 mode 真改变。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-gizmo", "coordinate_space_via_menu");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            int* mode = Services().current_gizmo_mode;
            IM_CHECK(mode != nullptr);
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("View/World Space");
            ctx->Yield();
            IM_CHECK_EQ(*mode, 1);
            ctx->MenuClick("View/Local Space");
            ctx->Yield();
            IM_CHECK_EQ(*mode, 0);
        };
    }

    // dse-gizmo/operation_via_viewport_toolbar：点 Scene 视口左上的 gizmo 工具栏按钮
    // （##vp_rot/##vp_scl/##vp_trans，见 editor_viewport_panel.cpp）→ 断言 operation 真改变。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-gizmo", "operation_via_viewport_toolbar");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            int* op = Services().current_gizmo_operation;
            IM_CHECK(op != nullptr);

            // 工具栏只在 Scene 视口被绘制（其标签页激活）时出现，先激活 Scene 标签页。
            ctx->WindowFocus("//Scene");
            ctx->Yield(2);
            IM_CHECK(FindActiveWindow("Scene") != nullptr);

            const std::string rot_ref   = std::string("//Scene/") + MDI_ICON_ROTATE_3D_VARIANT + "##vp_rot";
            const std::string scl_ref   = std::string("//Scene/") + MDI_ICON_RESIZE + "##vp_scl";
            const std::string trans_ref = std::string("//Scene/") + MDI_ICON_ARROW_ALL + "##vp_trans";

            ctx->ItemClick(rot_ref.c_str());
            ctx->Yield();
            IM_CHECK_EQ(*op, 1);
            ctx->ItemClick(scl_ref.c_str());
            ctx->Yield();
            IM_CHECK_EQ(*op, 2);
            ctx->ItemClick(trans_ref.c_str());
            ctx->Yield();
            IM_CHECK_EQ(*op, 0);
        };
    }

    // dse-gizmo/viewport_marquee_select：视口内从空白角拖出框选矩形覆盖原点处实体 → 断言其被选中。
    // 框选走纯投影几何（editor_viewport_panel.cpp:694），不依赖渲染/颜色 ID 拾取，故无头确定可复现。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-gizmo", "viewport_marquee_select");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            ForceDeterministicViewState();
            int n0 = 0;
            EnterIsolatedScene(ctx, n0);
            ResetEditorCameraToOrigin();
            const entt::entity ent = CreateSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            PlaceAtOriginAndSettle(ctx, ent);

            // 激活 Scene 标签页并取其内容矩形中心（原点实体投到此处）。
            ctx->WindowFocus("//Scene");
            ctx->Yield(2);
            ImGuiWindow* sw = FindActiveWindow("Scene");
            IM_CHECK(sw != nullptr);
            const ImVec2 center = sw->WorkRect.GetCenter();
            const ImVec2 wmin = sw->WorkRect.Min;
            const ImVec2 wmax = sw->WorkRect.Max;

            // 起点取视口左下角（远离左上 gizmo 工具栏，can_pick 处 ImGuizmo::IsOver 为假 → 框选开始）；
            // 终点取中心右上方，使框选矩形覆盖中心（含原点实体投影）。框选开始后不再复检命中，故
            // 即便拖拽途经 gizmo 也不受影响。
            const ImVec2 src(wmin.x + 20.0f, wmax.y - 20.0f);
            const ImVec2 dst(center.x + 60.0f, center.y - 60.0f);
            ManualMouseDrag(ctx, src, dst);
            ctx->Yield(2);

            IM_CHECK(SelectionManager::Get().Contains(ent));

            ExitIsolatedScene(ctx, n0);
        };
    }

    // dse-gizmo/gizmo_translate_drag：选中原点处实体、Translate 模式，从 gizmo 中心真实拖拽 →
    // 断言 Transform.position 真改变、压入「Gizmo Transform」可撤销命令、Undo 能复位。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-gizmo", "gizmo_translate_drag");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            int* op = Services().current_gizmo_operation;
            IM_CHECK(op != nullptr);
            ForceDeterministicViewState();
            int n0 = 0;
            EnterIsolatedScene(ctx, n0);
            ResetEditorCameraToOrigin();
            const entt::entity ent = CreateSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            PlaceAtOriginAndSettle(ctx, ent);
            *op = 0;  // Translate

            ctx->WindowFocus("//Scene");
            ctx->Yield(2);
            ImGuiWindow* sw = FindActiveWindow("Scene");
            IM_CHECK(sw != nullptr);
            const ImVec2 center = sw->WorkRect.GetCenter();

            entt::registry& reg = Reg();
            IM_CHECK(reg.valid(ent) && reg.all_of<TransformComponent>(ent));
            const glm::vec3 p0 = reg.get<TransformComponent>(ent).position;
            auto& mgr = GetUndoRedoManager();
            const int u0 = mgr.GetUndoCount();

            // 从 gizmo 中心（=原点投影）拖动：命中中心方块（视平面平移），Transform 必改变。
            ManualMouseDrag(ctx, center, ImVec2(center.x + 70.0f, center.y + 35.0f));
            ctx->Yield(2);

            const glm::vec3 p1 = reg.get<TransformComponent>(ent).position;
            const float moved = std::abs(p1.x - p0.x) + std::abs(p1.y - p0.y) + std::abs(p1.z - p0.z);
            IM_CHECK(moved > 1e-3f);                       // 位置确实变了
            IM_CHECK_EQ(mgr.GetUndoCount(), u0 + 1);       // 压入一条命令
            IM_CHECK_STR_EQ(mgr.GetUndoDescription().c_str(), "Gizmo Transform");

            // Undo → 复位回拖拽前位置（验证 gizmo 撤销接线）。
            mgr.Undo();
            ctx->Yield(2);
            const glm::vec3 p2 = reg.get<TransformComponent>(ent).position;
            const float back = std::abs(p2.x - p0.x) + std::abs(p2.y - p0.y) + std::abs(p2.z - p0.z);
            IM_CHECK(back < 1e-3f);

            ExitIsolatedScene(ctx, n0);
        };
    }

    // dse-gizmo/snap_setting_edit：View 菜单里改 Translate 吸附步长 DragFloat → 断言 GetSnapTranslate 持久值改变。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-gizmo", "snap_setting_edit");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("View");          // 打开 View 菜单（保持展开以操作其中的 DragFloat）
            ctx->Yield();
            ctx->SetRef("//$FOCUSED");
            ctx->ItemInputValue("Translate", 2.5f);
            ctx->Yield(2);
            IM_CHECK(std::abs(dse::editor::GetSnapTranslate() - 2.5f) < 0.01f);
            ctx->KeyPress(ImGuiKey_Escape);  // 关闭菜单
            ctx->Yield();
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
