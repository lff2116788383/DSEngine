/**
 * @file ui_tests_menubar.cpp
 * @brief 主菜单栏（DSEngineRoot 的 Edit 菜单）真实控件级用例（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 驱动主菜单栏的真实下拉项（无图标前缀的 Edit 菜单），断言以 ECS 实体计数为准。
 * 实体先经 Hierarchy 右键菜单创建（创建即选中，故 Edit/Duplicate、Edit/Delete 可用）。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

namespace dse::editor::uitest {

// 注：Hierarchy 右键“Create Empty Entity”会把新实体写进 EditorContext.selected_entity
// （编辑器单选态，主菜单 Edit 项据此 enable），但不写多选 SelectionManager；故此处不以
// SelectionManager 作前置，改以 ECS 实体计数变化为断言基准。

void RegisterMenuBarTests(ImGuiTestEngine* e) {
    // dse-menubar/edit_duplicate_selected：建实体（自动选中）→ 主菜单 Edit/Duplicate，+1。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-menubar", "edit_duplicate_selected");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            OpenHierarchyContextMenu(ctx);
            ctx->ItemClick("Create Empty Entity");
            const int before = CountValidEntities();
            IM_CHECK(before >= 1);
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("Edit/Duplicate");
            IM_CHECK_EQ(CountValidEntities(), before + 1);
        };
    }

    // dse-menubar/edit_delete_selected：建实体（自动选中）→ 主菜单 Edit/Delete，-1。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-menubar", "edit_delete_selected");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            OpenHierarchyContextMenu(ctx);
            ctx->ItemClick("Create Empty Entity");
            const int before = CountValidEntities();
            IM_CHECK(before >= 1);
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick("Edit/Delete");
            IM_CHECK_EQ(CountValidEntities(), before - 1);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
