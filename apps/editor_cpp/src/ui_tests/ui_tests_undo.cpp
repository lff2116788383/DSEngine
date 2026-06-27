/**
 * @file ui_tests_undo.cpp
 * @brief 撤销/重做用例（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 编辑器最核心的工作流：经 Hierarchy 右键创建实体（该路径会入撤销栈），
 * 再走主菜单 Edit → Undo / Redo（菜单项标签是动态的 "Undo (描述)"，运行期
 * 取 GetUndoDescription() 拼出精确标签），断言 ECS 实体计数回弹 / 复原。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <string>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_shortcuts.h"  // GetUndoRedoManager

namespace dse::editor::uitest {

void RegisterUndoTests(ImGuiTestEngine* e) {
    // dse-undo/undo_redo_via_menu：创建实体(+1) → Edit/Undo(回到原数) → Edit/Redo(再+1)。
    ImGuiTest* t = IM_REGISTER_TEST(e, "dse-undo", "undo_redo_via_menu");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        auto& mgr = GetUndoRedoManager();

        const int before = CountValidEntities();
        IM_CHECK(before >= 0);

        OpenHierarchyContextMenu(ctx);
        ctx->ItemClick("Create Empty Entity");
        ctx->Yield();
        IM_CHECK_EQ(CountValidEntities(), before + 1);
        IM_CHECK(mgr.CanUndo());

        // Edit 菜单 Undo 标签为动态 "Undo (<描述>)"，按运行期描述拼精确标签。
        const std::string undo_label = std::string("Edit/Undo (") + mgr.GetUndoDescription() + ")";
        ctx->SetRef("//DSEngineRoot");
        ctx->MenuClick(undo_label.c_str());
        ctx->Yield(2);
        IM_CHECK_EQ(CountValidEntities(), before);
        IM_CHECK(mgr.CanRedo());

        const std::string redo_label = std::string("Edit/Redo (") + mgr.GetRedoDescription() + ")";
        ctx->SetRef("//DSEngineRoot");
        ctx->MenuClick(redo_label.c_str());
        ctx->Yield(2);
        IM_CHECK_EQ(CountValidEntities(), before + 1);
    };
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
