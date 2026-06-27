/**
 * @file ui_tests_scene.cpp
 * @brief 场景级基础操作用例：新建场景 / 保存场景（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 与项目操作同属“绝不能崩”的关键路径，走真实 File 菜单控件：
 *   - new_scene ：File → New Scene（无对话框）→ registry 清空、页签 +1、当前页签为 Untitled。
 *   - save_scene：先把当前页签路径设到临时文件（避免触发原生“另存为”对话框），
 *                 再 File → Save Scene 落盘，断言文件确实写出。
 * 断言以 SceneTabManager 真实状态 + 磁盘文件为准；落临时目录，跑前清场。
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
#include "../editor_icons.h"

namespace dse::editor::uitest {

namespace {
namespace fs = std::filesystem;

#define DSE_MENU_NEW_SCENE  "File/" MDI_ICON_PLUS         "  New Scene"
#define DSE_MENU_SAVE_SCENE "File/" MDI_ICON_CONTENT_SAVE "  Save Scene"

} // namespace

void RegisterSceneTests(ImGuiTestEngine* e) {
    // dse-scene/new_scene：File → New Scene → 新建空场景页签（清空 registry）。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-scene", "new_scene");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            auto& tabs = SceneTabManager::Get();
            const int tabs0 = tabs.GetTabCount();
            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick(DSE_MENU_NEW_SCENE);
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetTabCount(), tabs0 + 1);
            IM_CHECK(tabs.GetActiveFilePath().empty());  // 新场景为 Untitled（无路径）
            IM_CHECK_EQ(CountValidEntities(), 0);        // 新空场景：registry 已清空
        };
    }

    // dse-scene/save_scene：把当前页签路径设到临时文件后 File → Save Scene，断言文件写出。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-scene", "save_scene");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const fs::path dir   = fs::temp_directory_path() / "dse_ui_tests";
            const fs::path scene = dir / "ui_test_save_scene.json";
            std::error_code ec;
            fs::create_directories(dir, ec);
            fs::remove(scene, ec);

            // 设定当前页签路径 → Save Scene 走“直接落盘”分支（不弹原生对话框）。
            SceneTabManager::Get().SetCurrentPath(scene.string());

            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick(DSE_MENU_SAVE_SCENE);
            ctx->Yield(2);

            IM_CHECK(fs::exists(scene));
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
