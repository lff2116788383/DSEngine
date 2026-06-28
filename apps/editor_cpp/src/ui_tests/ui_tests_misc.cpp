/**
 * @file ui_tests_misc.cpp
 * @brief ⑨ 杂项编辑器特性补测（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 三个真实控件级 + 真断言用例，覆盖 相机导航 / 碰撞体编辑 / Gizmo 操作切换：
 *   - scene_camera_focus_selected：菜单 Entity/Focus Selected → 编辑器相机
 *     focal_point 飞到选中实体位置、distance 收到 5（断言 EditorCamera 状态）。
 *   - collider_box3d_edit_fields：Inspector 加 Box Collider 3D，改 Bounciness/Friction
 *     拖条、翻 Is Trigger 勾选框 → 断言 BoxCollider3DComponent 字段被写回。
 *   - view_menu_gizmo_op_mode：View 菜单切 Gizmo 操作（Translate/Rotate/Scale）与
 *     坐标系（Local/World）→ 断言编辑器 current_gizmo_operation / current_gizmo_mode。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cmath>
#include <vector>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_selection.h"        // SelectionManager
#include "../editor_scene_camera.h"     // EditorCamera / GetEditorCamera / FocusEditorCamera

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"               // TransformComponent（全局命名空间）
#include "engine/ecs/components_3d_physics.h"   // dse::BoxCollider3DComponent

namespace dse::editor::uitest {

namespace {

entt::registry& Reg() { return Services().engine->pipeline()->world().registry(); }

// 右键 Hierarchy → Create Empty Entity（创建即选中），按 registry 差集取回新实体。
entt::entity NewSelectedEntity(ImGuiTestContext* ctx) {
    entt::registry& reg = Reg();
    std::vector<entt::entity> before;
    for (auto en : reg.storage<entt::entity>())
        if (reg.valid(en)) before.push_back(en);
    OpenHierarchyContextMenu(ctx);
    ctx->ItemClick("Create Empty Entity");
    ctx->Yield();
    SelectionManager::Get().Clear();
    for (auto en : reg.storage<entt::entity>()) {
        if (!reg.valid(en)) continue;
        bool seen = false;
        for (auto b : before) if (b == en) { seen = true; break; }
        if (!seen) return en;
    }
    return entt::null;
}

// Inspector「Add Component」按钮 → 弹窗里点对应 MenuItem（label 即组件注册名）。
void AddComponent(ImGuiTestContext* ctx, const char* component_name) {
    ctx->WindowFocus("//Inspector");
    ctx->SetRef("//Inspector");
    ctx->ItemClick("Add Component");
    ctx->Yield();
    ctx->SetRef("//$FOCUSED");
    ctx->ItemClick(component_name);
    ctx->Yield(2);
    ctx->SetRef("//Inspector");
}

// 经 Hierarchy 右键「Delete Entity」删掉当前选中实体（编辑器正规删除路径）。
void DeleteSelectedEntity(ImGuiTestContext* ctx, entt::entity ent) {
    OpenHierarchyContextMenu(ctx);
    ctx->ItemClick("Delete Entity");
    ctx->Yield(2);
    IM_CHECK(!Reg().valid(ent));
}

} // namespace

void RegisterMiscEditorTests(ImGuiTestEngine* e) {
    // ── ⑨-1 相机导航：菜单 Focus Selected 把相机飞到选中实体 ────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-misc", "scene_camera_focus_selected");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NewSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);

            entt::registry& reg = Reg();
            if (!reg.all_of<TransformComponent>(ent)) reg.emplace<TransformComponent>(ent);
            reg.get<TransformComponent>(ent).position = glm::vec3(7.0f, 8.0f, 9.0f);

            // 把相机挪到别处，确保 Focus 真的移动了它。
            EditorCamera& cam = GetEditorCamera();
            cam.focal_point = glm::vec3(0.0f);
            cam.distance = 42.0f;

            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("Entity/Focus Selected");
            ctx->Yield(2);

            IM_CHECK(glm::length(cam.focal_point - glm::vec3(7.0f, 8.0f, 9.0f)) < 0.01f);
            IM_CHECK(std::abs(cam.distance - 5.0f) < 0.01f);

            DeleteSelectedEntity(ctx, ent);
        };
    }

    // ── ⑨-2 碰撞体编辑：Box Collider 3D 字段在 Inspector 真实控件里改 → 写回 ECS ──
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-misc", "collider_box3d_edit_fields");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const entt::entity ent = NewSelectedEntity(ctx);
            IM_CHECK(ent != entt::null);
            AddComponent(ctx, "Box Collider 3D");
            IM_CHECK(Reg().all_of<dse::BoxCollider3DComponent>(ent));

            const bool trig_before = Reg().get<dse::BoxCollider3DComponent>(ent).is_trigger;

            ctx->ItemInputValue("//Inspector/##boxcol3d_bounce", 0.5f);
            ctx->ItemInputValue("//Inspector/##boxcol3d_fric", 0.2f);
            ctx->ItemClick("//Inspector/##boxcol3d_trigger");
            ctx->Yield(2);

            const auto& col = Reg().get<dse::BoxCollider3DComponent>(ent);
            IM_CHECK(std::abs(col.bounciness - 0.5f) < 0.01f);
            IM_CHECK(std::abs(col.friction - 0.2f) < 0.01f);
            IM_CHECK(col.is_trigger != trig_before);

            DeleteSelectedEntity(ctx, ent);
        };
    }

    // ── ⑨-3 View 菜单切 Gizmo 操作/坐标系 → 断言编辑器 gizmo op/mode 状态 ─────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-misc", "view_menu_gizmo_op_mode");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            IM_CHECK(Services().current_gizmo_operation != nullptr);
            IM_CHECK(Services().current_gizmo_mode != nullptr);

            // 已知初始态：Translate / Local。
            *Services().current_gizmo_operation = 0;
            *Services().current_gizmo_mode = 0;

            ctx->SetRef("//DSEngineRoot");

            ctx->MenuClick("View/Gizmo: Rotate");
            ctx->Yield();
            IM_CHECK(*Services().current_gizmo_operation == 1);

            ctx->MenuClick("View/Gizmo: Scale");
            ctx->Yield();
            IM_CHECK(*Services().current_gizmo_operation == 2);

            ctx->MenuClick("View/World Space");
            ctx->Yield();
            IM_CHECK(*Services().current_gizmo_mode == 1);

            ctx->MenuClick("View/Local Space");
            ctx->Yield();
            IM_CHECK(*Services().current_gizmo_mode == 0);

            // 复位到 Translate，避免污染后续依赖默认 gizmo 操作的用例。
            ctx->MenuClick("View/Gizmo: Translate");
            ctx->Yield();
            IM_CHECK(*Services().current_gizmo_operation == 0);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
