/**
 * @file ui_tests_inspector.cpp
 * @brief Inspector（属性）面板用例（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 经 Hierarchy 右键菜单创建一个实体（创建即选中），断言 SelectionManager 非空且
 * Inspector 窗口处于绘制中——即“层级选中 → 属性面板联动”这条编辑器主链路成立。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cmath>

#include <entt/entt.hpp>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_selection.h"
#include "../editor_shared_components.h"  // EditorNameComponent

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"  // TransformComponent（全局命名空间）

namespace dse::editor::uitest {

void RegisterInspectorTests(ImGuiTestEngine* e) {
    // dse-inspector/reflects_selection：建实体 → 主菜单 Edit/Select All（这条会写入多选
    // SelectionManager），断言 SelectionManager 非空且 Inspector 窗口在绘制——即“选中 →
    // 属性面板联动”链路成立。（Hierarchy 右键创建只写 EditorContext 单选态，不写 SelectionManager。）
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-inspector", "reflects_selection");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            OpenHierarchyContextMenu(ctx);
            ctx->ItemClick("Create Empty Entity");
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("Edit/Select All");
            ctx->Yield(2);
            IM_CHECK(!SelectionManager::Get().IsEmpty());
            ImGuiWindow* inspector = FindActiveWindow("Inspector");
            IM_CHECK(inspector != nullptr);
        };
    }

    // dse-inspector/edit_name：建实体并选中 → 在 Inspector 改名字输入框 → 断言 ECS 中
    // 真有实体改成该名（属性面板写回组件这条链路成立）。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-inspector", "edit_name");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            OpenHierarchyContextMenu(ctx);
            ctx->ItemClick("Create Empty Entity");  // 创建即选中 → Inspector 显示该实体
            ctx->Yield();
            // 清掉前序用例（reflects_selection 的 Select All）残留的多选：否则 Inspector 走
            // 多选编辑分支而不渲染单选的 ##Name 字段。创建已置 context.selected_entity，单选态保留。
            SelectionManager::Get().Clear();

            ctx->WindowFocus("//Inspector");
            ctx->SetRef("//Inspector");
            ctx->ItemInputValue("##Name", "DSEUiRenamedEntity");
            ctx->Yield(2);

            entt::registry& reg = Services().engine->pipeline()->world().registry();
            bool found = false;
            for (auto en : reg.view<EditorNameComponent>()) {
                if (reg.get<EditorNameComponent>(en).name == "DSEUiRenamedEntity") {
                    found = true;
                    break;
                }
            }
            IM_CHECK(found);
        };
    }

    // dse-inspector/edit_position：建实体并选中 → 在 Transform 的 X 输入框填特征值 →
    // 断言 ECS 中真有实体的 position.x 等于该值（DragFloat 改值写回组件这条链路成立）。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-inspector", "edit_position");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            OpenHierarchyContextMenu(ctx);
            ctx->ItemClick("Create Empty Entity");
            ctx->Yield();
            SelectionManager::Get().Clear();  // 同 edit_name：去除残留多选，走单选 Transform 渲染分支

            ctx->WindowFocus("//Inspector");
            ctx->SetRef("//Inspector");
            // Transform.Position 的 X 字段：PushID("##pos_undo") → DrawVec3 PushID("##pos") → DragFloat("##x")。
            ctx->ItemInputValue("##pos_undo/##pos/##x", 42.5f);
            ctx->Yield(2);

            entt::registry& reg = Services().engine->pipeline()->world().registry();
            bool found = false;
            for (auto en : reg.view<TransformComponent>()) {
                if (std::abs(reg.get<TransformComponent>(en).position.x - 42.5f) < 0.01f) {
                    found = true;
                    break;
                }
            }
            IM_CHECK(found);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
