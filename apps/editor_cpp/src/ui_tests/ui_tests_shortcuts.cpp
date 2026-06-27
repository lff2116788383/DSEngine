/**
 * @file ui_tests_shortcuts.cpp
 * @brief 键盘快捷键用例（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 直接发按键给被测 ImGui，验证全局快捷键路由（ImGui::Shortcut RouteGlobal）真实生效：
 *   - Ctrl+N：新建场景（页签 +1）
 *   - Ctrl+D：复制选中实体（ECS +1）
 * 这些是“手不离键”的基础操作，必须可用且不崩。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_scene_tabs.h"

namespace dse::editor::uitest {

void RegisterShortcutTests(ImGuiTestEngine* e) {
    // dse-shortcuts/new_scene_ctrl_n：Ctrl+N → 新建场景页签 +1。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-shortcuts", "new_scene_ctrl_n");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            auto& tabs = SceneTabManager::Get();
            const int tabs0 = tabs.GetTabCount();
            ctx->SetRef("//DSEngineRoot");
            ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_N);
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetTabCount(), tabs0 + 1);
        };
    }

    // dse-shortcuts/duplicate_ctrl_d：建实体并选中 → Ctrl+D → 复制（ECS +1）。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-shortcuts", "duplicate_ctrl_d");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            OpenHierarchyContextMenu(ctx);
            ctx->ItemClick("Create Empty Entity");  // 创建即选中（EditorContext.selected_entity）
            ctx->Yield();
            const int before = CountValidEntities();
            IM_CHECK(before >= 1);

            ctx->SetRef("//DSEngineRoot");
            ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_D);
            ctx->Yield(2);
            IM_CHECK_EQ(CountValidEntities(), before + 1);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
