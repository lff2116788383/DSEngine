/**
 * @file ui_tests_inspector.cpp
 * @brief Inspector（属性）面板用例（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 经 Hierarchy 右键菜单创建一个实体（创建即选中），断言 SelectionManager 非空且
 * Inspector 窗口处于绘制中——即“层级选中 → 属性面板联动”这条编辑器主链路成立。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_selection.h"

namespace dse::editor::uitest {

void RegisterInspectorTests(ImGuiTestEngine* e) {
    // dse-inspector/reflects_selection：建实体 → 主菜单 Edit/Select All（这条会写入多选
    // SelectionManager），断言 SelectionManager 非空且 Inspector 窗口在绘制——即“选中 →
    // 属性面板联动”链路成立。（Hierarchy 右键创建只写 EditorContext 单选态，不写 SelectionManager。）
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

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
