/**
 * @file ui_tests_assetmgmt.cpp
 * @brief ⑤ 资源浏览器深度 补测（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 覆盖 Project 面板（editor_aux_panels.cpp::DrawProjectPanel）的真实资产管理工作流，
 * 全部断言磁盘真实状态 / 面板导航状态，而非“能画出来”：
 *   - folder_navigation ：双击文件夹进入子目录、再 "<- Back" 返回根目录（断言面板 current_path）。
 *   - create_folder     ：子目录里右键窗口背景 → Create/Folder → 磁盘多出 NewFolder。
 *   - rename_asset      ：列表项右键 → Rename → 内联输入新名 → 回车 → 磁盘文件被改名。
 *   - delete_asset      ：列表项右键 → Delete → 磁盘文件被删除。
 *
 * Project 面板默认停靠底部、列表可视高度≈0，资源行被裁剪不可命中；用例统一先
 * MakeProjectPanelFloating 把它浮动放大、结束 RestoreProjectPanelDock 复位（避免污染后续布局）。
 * 面板的浏览目录 current_path 是跨用例存活的静态量——每个用例开场用 SetProjectPanelCurrentPath
 * 复位到资产根，避免上一个用例遗留的子目录导致列表为空。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_aux_panels.h"  // ProjectPanelCurrentPath / SetProjectPanelCurrentPath / ...Base
#include "../editor_icons.h"       // MDI_ICON_PACKAGE_VARIANT / MDI_ICON_FILE_OUTLINE

namespace dse::editor::uitest {

namespace {
namespace fs = std::filesystem;

// 清空 Project 面板搜索过滤（dse-assets/search_input_typing 会留下 "cube"，会把测试文件过滤掉）。
void ClearProjectSearch(ImGuiTestContext* ctx) {
    ctx->ItemInputValue("//Project/##project_search", "");
    ctx->Yield(2);
}

// 把面板浏览目录复位到资产根（current_path 是跨用例静态量）。
void ResetProjectNav() {
    SetProjectPanelCurrentPath(ProjectPanelBaseDataPath());
}

// 列表视图某项的绝对 ref：Table("project_list") -> PushID(filename) -> Selectable("<icon>  <filename>")。
std::string ListItemRef(const char* filename, const char* type_icon) {
    return std::string("//Project/project_list/") + filename + "/" + type_icon + "  " + filename;
}

// 在列表项上右键，打开该项的上下文菜单并把 ref 指向弹窗。
void OpenItemContextMenu(ImGuiTestContext* ctx, const std::string& item_ref) {
    ctx->MouseMove(item_ref.c_str());
    ctx->MouseClick(ImGuiMouseButton_Right);
    ctx->SetRef("//$FOCUSED");
}
} // namespace

void RegisterAssetMgmtTests(ImGuiTestEngine* e) {
    // ── ⑤-1 文件夹导航：双击进入子目录 → 出现 marker → "<- Back" 返回根目录 ──────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-assetmgmt", "folder_navigation");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            ResetProjectNav();
            const fs::path base = ProjectPanelBaseDataPath();
            const fs::path dir  = base / "uitest_nav";
            std::error_code ec;
            fs::remove_all(dir, ec);
            fs::create_directories(dir, ec);
            { std::ofstream ofs((dir / "marker.txt").string()); ofs << "x"; }

            MakeProjectPanelFloating(ctx);
            ClearProjectSearch(ctx);
            ctx->Yield(4);
            IM_CHECK(ProjectPanelCurrentPath() == base);  // 起点在资产根

            // 双击文件夹进入子目录。
            const std::string dir_ref = ListItemRef("uitest_nav", MDI_ICON_PACKAGE_VARIANT);
            IM_CHECK(ctx->ItemInfo(dir_ref.c_str()).ID != 0);
            ctx->ItemDoubleClick(dir_ref.c_str());
            ctx->Yield(4);
            IM_CHECK(ProjectPanelCurrentPath() == dir);

            // 子目录里的 marker.txt 被列出；面包屑出现 "<- Back"。
            const std::string marker_ref = ListItemRef("marker.txt", MDI_ICON_FILE_OUTLINE);
            IM_CHECK(ctx->ItemInfo(marker_ref.c_str()).ID != 0);
            IM_CHECK(ctx->ItemInfo("//Project/<- Back").ID != 0);

            // 点 "<- Back" 返回根目录。
            ctx->ItemClick("//Project/<- Back");
            ctx->Yield(4);
            IM_CHECK(ProjectPanelCurrentPath() == base);

            fs::remove_all(dir, ec);
            RestoreProjectPanelDock(ctx);
        };
    }

    // ── ⑤-2 新建文件夹：进子目录 → 右键面包屑行背景 → Create/Folder → 磁盘多出 NewFolder ─
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-assetmgmt", "create_folder");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            ResetProjectNav();
            const fs::path base = ProjectPanelBaseDataPath();
            const fs::path ws   = base / "uitest_mkdir";
            std::error_code ec;
            fs::remove_all(ws, ec);
            fs::create_directories(ws, ec);

            MakeProjectPanelFloating(ctx);
            ClearProjectSearch(ctx);
            ctx->Yield(4);

            // 进入空工作目录。
            const std::string ws_ref = ListItemRef("uitest_mkdir", MDI_ICON_PACKAGE_VARIANT);
            IM_CHECK(ctx->ItemInfo(ws_ref.c_str()).ID != 0);
            ctx->ItemDoubleClick(ws_ref.c_str());
            ctx->Yield(4);
            IM_CHECK(ProjectPanelCurrentPath() == ws);

            // 资源列表用带 ScrollY 的内嵌 Table（独立子窗口）；在列表区域右键命中的是该子窗口，
            // 不会触发挂在 Project 窗口上的 ProjectContextMenu。改在面包屑行（"<- Back" 右侧的
            // 空白文字区）右键——那里属 Project 窗口背景，才能弹出窗口背景菜单。
            const ImGuiTestItemInfo back = ctx->ItemInfo("//Project/<- Back");
            IM_CHECK(back.ID != 0);
            ctx->MouseMoveToPos(ImVec2(back.RectFull.Max.x + 90.0f,
                                       back.RectFull.GetCenter().y));
            ctx->MouseClick(ImGuiMouseButton_Right);
            ctx->Yield(2);
            ctx->SetRef("//$FOCUSED");
            ctx->MenuClick("Create/Folder");
            ctx->Yield(4);

            IM_CHECK(fs::exists(ws / "NewFolder"));

            ResetProjectNav();
            ctx->Yield(2);
            fs::remove_all(ws, ec);
            ctx->SetRef("//Project");
            RestoreProjectPanelDock(ctx);
        };
    }

    // ── ⑤-3 重命名资源：列表项右键 → Rename → 内联输入新名回车 → 磁盘文件改名 ───────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-assetmgmt", "rename_asset");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            ResetProjectNav();
            const fs::path base = ProjectPanelBaseDataPath();
            const fs::path old_file = base / "uitest_rn.txt";
            const fs::path new_file = base / "uitest_rn2.txt";
            std::error_code ec;
            fs::remove(new_file, ec);
            { std::ofstream ofs(old_file.string()); ofs << "rename me"; }

            MakeProjectPanelFloating(ctx);
            ClearProjectSearch(ctx);
            ctx->Yield(4);

            const std::string item_ref = ListItemRef("uitest_rn.txt", MDI_ICON_FILE_OUTLINE);
            IM_CHECK(ctx->ItemInfo(item_ref.c_str()).ID != 0);
            OpenItemContextMenu(ctx, item_ref);
            ctx->ItemClick("Rename");
            ctx->Yield(2);

            // 内联 InputText 在该行作用域内：PushID(filename) -> InputText("##rename_project")。
            ctx->ItemInputValue("//Project/project_list/uitest_rn.txt/##rename_project",
                                "uitest_rn2.txt");
            ctx->Yield(4);

            IM_CHECK(fs::exists(new_file));
            IM_CHECK(!fs::exists(old_file));

            fs::remove(new_file, ec);
            ctx->SetRef("//Project");
            RestoreProjectPanelDock(ctx);
        };
    }

    // ── ⑤-4 删除资源：列表项右键 → Delete → 磁盘文件被删除 ──────────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-assetmgmt", "delete_asset");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            ResetProjectNav();
            const fs::path base = ProjectPanelBaseDataPath();
            const fs::path file = base / "uitest_del.txt";
            std::error_code ec;
            { std::ofstream ofs(file.string()); ofs << "delete me"; }

            MakeProjectPanelFloating(ctx);
            ClearProjectSearch(ctx);
            ctx->Yield(4);

            const std::string item_ref = ListItemRef("uitest_del.txt", MDI_ICON_FILE_OUTLINE);
            IM_CHECK(ctx->ItemInfo(item_ref.c_str()).ID != 0);
            IM_CHECK(fs::exists(file));

            OpenItemContextMenu(ctx, item_ref);
            ctx->ItemClick("Delete");
            ctx->Yield(4);

            IM_CHECK(!fs::exists(file));

            ctx->SetRef("//Project");
            RestoreProjectPanelDock(ctx);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
