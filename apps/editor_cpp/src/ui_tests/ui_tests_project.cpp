/**
 * @file ui_tests_project.cpp
 * @brief 项目级基础操作用例：新建 / 打开（旧项目）/ 保存（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 这些是“绝不能崩”的关键路径。尽量走真实 UI 控件：
 *   - new_project   ：File → New Project... 弹窗，填名字+路径，点 Create
 *   - reopen_project：File → Recent Projects → <项目>（此路径不弹原生文件对话框，可无头驱动；
 *                     而菜单 Open Project... 走的是 Windows 原生 OS 文件对话框，无头跑不了）
 *   - save_project  ：File → Save Project（无对话框，直接落盘）
 * 断言以 ProjectManager 的真实状态 + 磁盘上的 project.dseproj 为准。
 *
 * 测试项目落在系统临时目录，跑前清场以保证可重复；用例彼此自足（可单独 filter 跑）。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <filesystem>
#include <string>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_project.h"
#include "../editor_settings.h"
#include "../editor_icons.h"

namespace dse::editor::uitest {

namespace {

namespace fs = std::filesystem;

constexpr const char* kProjName    = "DSEUiTestProject";
constexpr const char* kScratchName = "DSEUiTestScratch";

// 带图标前缀的控件标签需与源处完全一致。
// File 菜单 Save Project（editor_shell.cpp:248）；Project Hub 新建按钮（editor_project_hub.cpp:189，注意三个空格）。
#define DSE_MENU_SAVE_PROJECT "File/" MDI_ICON_CONTENT_SAVE "  Save Project"
#define DSE_HUB_NEW_PROJECT   MDI_ICON_PLUS "   New Project..."

fs::path TestParent() {
    return fs::temp_directory_path() / "dse_ui_tests";
}

// 在磁盘上备好一个干净的测试项目（不改变“当前已打开项目”——CreateProject 会自动打开，
// 故调用后由各用例自行决定最终打开哪个），并登记进 recent_projects 供 reopen 用例走 Recent 菜单。
void EnsureTestProjectOnDiskAndRecent() {
    auto& pm = ProjectManager::Get();
    const fs::path root    = TestParent() / kProjName;
    const fs::path dseproj = root / "project.dseproj";
    if (!fs::exists(dseproj)) {
        std::error_code ec;
        fs::remove_all(root, ec);
        pm.CreateProject(TestParent(), kProjName, ProjectTemplate::Empty);
    }
    EditorSettings s = LoadEditorSettings();
    AddRecentProject(s, root.string());
    SaveEditorSettings(s);
}

} // namespace

void RegisterProjectTests(ImGuiTestEngine* e) {
    // dse-project/new_project：走 Project Hub 的「New Project」表单（真实可用的新建 UI；
    // 注意 File→New Project... 菜单弹窗因 OpenPopup/BeginPopupModal 处于不同 ID 作用域而打不开，
    // 见对 editor_shell.cpp 的说明）。关闭当前项目→Hub 出现→切到新建表单→真实填表→Create。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-project", "new_project");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const fs::path parent = TestParent();
            const fs::path root   = parent / kProjName;
            std::error_code ec;
            fs::remove_all(root, ec);  // CreateProject 要求目标目录不存在或为空

            auto& pm = ProjectManager::Get();
            pm.CloseProject();  // 进入 Project Hub
            ctx->Yield(3);

            ctx->SetRef("//##ProjectHub");
            ctx->ItemClick(DSE_HUB_NEW_PROJECT);  // 切到「Create New Project」表单
            ctx->Yield();
            ctx->ItemInputValue("##proj_name", kProjName);
            ctx->ItemInputValue("##proj_loc", parent.generic_string().c_str());
            ctx->ItemClick("  Create Project  ");
            ctx->Yield(3);

            IM_CHECK(pm.HasOpenProject());
            IM_CHECK_STR_EQ(pm.GetDescriptor().name.c_str(), kProjName);
            IM_CHECK(fs::exists(root / "project.dseproj"));
        };
    }

    // dse-project/save_project：保证有项目打开，删掉 project.dseproj 后走 File→Save Project，
    // 断言文件被重新写出（= Save 真跑且未崩）。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-project", "save_project");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            auto& pm = ProjectManager::Get();
            if (!pm.HasOpenProject()) {
                const fs::path root = TestParent() / kProjName;
                std::error_code ec;
                fs::remove_all(root, ec);
                pm.CreateProject(TestParent(), kProjName, ProjectTemplate::Empty);
            }
            IM_CHECK(pm.HasOpenProject());

            const fs::path dseproj = pm.GetProjectRoot() / "project.dseproj";
            std::error_code ec;
            fs::remove(dseproj, ec);

            ctx->SetRef("//DSEngineRoot");
            ctx->MenuClick(DSE_MENU_SAVE_PROJECT);
            ctx->Yield(2);

            IM_CHECK(fs::exists(dseproj));
        };
    }

    // dse-project/reopen_project：先备好测试项目并进 recent，再切到一个 scratch 项目作为“当前”，
    // 然后走 File→Recent Projects→<项目> 打开旧项目，断言当前项目切回测试项目（= 打开旧项目链路成立、未崩）。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-project", "reopen_project");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            EnsureTestProjectOnDiskAndRecent();

            auto& pm = ProjectManager::Get();
            // 切到 scratch 项目，使“当前项目”不同于待打开的旧项目，从而能验证确实发生了切换。
            const fs::path scratch = TestParent() / kScratchName;
            std::error_code ec;
            fs::remove_all(scratch, ec);
            pm.CreateProject(TestParent(), kScratchName, ProjectTemplate::Empty);
            IM_CHECK_STR_EQ(pm.GetProjectRoot().filename().string().c_str(), kScratchName);

            ctx->SetRef("//DSEngineRoot");
            // Recent 子菜单项标签 = 项目根目录的文件夹名（无图标），即 kProjName。
            ctx->MenuClick("File/Recent Projects/DSEUiTestProject");
            ctx->Yield(2);

            IM_CHECK(pm.HasOpenProject());
            IM_CHECK_STR_EQ(pm.GetProjectRoot().filename().string().c_str(), kProjName);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
