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

#include <filesystem>
#include <string>
#include <system_error>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_scene_tabs.h"
#include "../editor_selection.h"

namespace dse::editor::uitest {

namespace { namespace fs = std::filesystem; }

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

    // dse-shortcuts/save_ctrl_s：把当前页签路径设到临时文件并删掉它 → Ctrl+S → 文件被重新写出。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-shortcuts", "save_ctrl_s");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const fs::path dir   = fs::temp_directory_path() / "dse_ui_tests";
            const fs::path scene = dir / "ui_test_ctrl_s.json";
            std::error_code ec;
            fs::create_directories(dir, ec);
            fs::remove(scene, ec);

            // 设当前页签路径 → Ctrl+S 走“直接落盘”分支（路径非空，不弹原生对话框）。
            SceneTabManager::Get().SetCurrentPath(scene.string());

            ctx->SetRef("//DSEngineRoot");
            ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_S);
            ctx->Yield(2);
            IM_CHECK(fs::exists(scene));
        };
    }

    // dse-shortcuts/copy_paste_ctrl_c_v：建实体并选中 → Ctrl+C → Ctrl+V → 粘出一个副本（ECS +1）。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-shortcuts", "copy_paste_ctrl_c_v");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            OpenHierarchyContextMenu(ctx);
            ctx->ItemClick("Create Empty Entity");  // 创建即选中（EditorContext.selected_entity）
            ctx->Yield();
            const int before = CountValidEntities();
            IM_CHECK(before >= 1);

            ctx->SetRef("//DSEngineRoot");
            ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_C);
            ctx->Yield();
            ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_V);
            ctx->Yield(2);
            IM_CHECK_EQ(CountValidEntities(), before + 1);
        };
    }

    // dse-shortcuts/delete_key：建实体并选中 → Delete → 删除（ECS -1）。
    // 先清多选态，确保走“单选 → DeleteSelectedEntity(context.selected_entity)”分支。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-shortcuts", "delete_key");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            OpenHierarchyContextMenu(ctx);
            ctx->ItemClick("Create Empty Entity");
            ctx->Yield();
            const int before = CountValidEntities();
            IM_CHECK(before >= 1);
            SelectionManager::Get().Clear();  // 去除残留多选；context.selected_entity 仍指向新建实体

            ctx->SetRef("//DSEngineRoot");
            ctx->KeyPress(ImGuiKey_Delete);
            ctx->Yield(2);
            IM_CHECK_EQ(CountValidEntities(), before - 1);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
