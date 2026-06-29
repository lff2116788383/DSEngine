/**
 * @file ui_tests_assets.cpp
 * @brief Asset Browser / Project 面板真实控件级用例（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 *   - search_input_typing  ：向资源浏览器搜索框真实键入文本（输入控件可驱动、不崩）。
 *   - drag_asset_to_scene  ：把 Project 面板里的一个 .dscene 资源拖到 Hierarchy 的 "Scene" 根，
 *                            断言场景里多出一个（带 SubSceneComponent 的）实体——覆盖资源→场景拖拽工作流。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_project.h"   // ProjectManager
#include "../editor_icons.h"     // MDI_ICON_IMAGE_MULTIPLE

namespace dse::editor::uitest {

namespace { namespace fs = std::filesystem; }

void RegisterAssetBrowserTests(ImGuiTestEngine* e) {
    // dse-assets/search_input_typing：向资源浏览器搜索框键入文本（真实控件输入）。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-assets", "search_input_typing");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            EnsureAllPanelsVisible();
            ctx->Yield(4);
            ctx->SetRef("//Asset Browser");
            ctx->ItemInputValue("##asset_search", "cube");
        };
    }

    // dse-assets/drag_asset_to_scene：从 Project 面板（列表视图，默认）拖一个 .dscene 资源到
    // Hierarchy 的 "Scene" 根节点 → 触发 ASSET_PATH 落区 → 新建一个子场景实体（ECS +1）。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-assets", "drag_asset_to_scene");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            // Project 面板列出的目录：有项目用项目资产目录，否则回退到 samples/lua/data
            // （与 editor_aux_panels.cpp::GetProjectBaseDataPath 保持一致）。
            auto& pm = ProjectManager::Get();
            fs::path base = pm.HasOpenProject()
                ? pm.GetAssetDir()
                : (fs::current_path() / "samples" / "lua" / "data");
            std::error_code ec;
            fs::create_directories(base, ec);

            // 放一个特征 .dscene 文件，让 Project 面板把它列成可拖拽项。
            const fs::path asset = base / "ui_test_drop.dscene";
            { std::ofstream ofs(asset.string()); ofs << "{}"; }

            // 关掉浮动可选面板，避免压住落点；让面板刷新目录列表。
            HideOptionalPanels();
            ctx->Yield(6);

            // 默认 dock 布局里 Project 停靠在底部，占视口约 15%（1280x720 下列表可视高度≈0），
            // 资源行被裁剪、不可命中。把 Project 浮动出来并放大，确保资源行完整可见、可拖拽；
            // 放在右侧空白处，避免压住左侧 Hierarchy 的落点。经统一安全封装（复位 ref 到根 + null
            // 兜底）浮动，规避上游 UndockWindow 在遗留非根 ref 时按相对名解析不到窗口的空指针崩溃。
            UndockPanel(ctx, "//Project");
            ctx->WindowMove("//Project", ImVec2(420.0f, 80.0f));
            ctx->WindowResize("//Project", ImVec2(520.0f, 520.0f));
            ctx->Yield(2);

            // 列表视图里文件项：Table("project_list") -> PushID(filename) -> Selectable(label)，
            // label = "<icon>  <filename>"（见 editor_aux_panels.cpp:585-627）。ref 须含 table 与
            // PushID 两层，否则定位不到（FindByLabel "**/" 对此项不生效）。
            const std::string asset_ref =
                std::string("//Project/project_list/ui_test_drop.dscene/") +
                MDI_ICON_IMAGE_MULTIPLE "  " + "ui_test_drop.dscene";
            const char* scene_ref = "//Hierarchy/Scene";

            const ImGuiTestItemInfo si = ctx->ItemInfo(scene_ref);
            IM_CHECK(si.ID != 0);
            const ImGuiTestItemInfo ai = ctx->ItemInfo(asset_ref.c_str());
            IM_CHECK(ai.ID != 0);

            const int before = CountValidEntities();

            // 用测试引擎内建拖拽：按 item ID 移动（每帧重定位、自动滚动入视口），稳健触发拖拽源。
            ctx->ItemDragAndDrop(asset_ref.c_str(), scene_ref);
            ctx->Yield(2);

            IM_CHECK_EQ(CountValidEntities(), before + 1);

            fs::remove(asset, ec);

            // 还原布局：上面把 Project 浮动出来改了全局停靠状态，会污染后续依赖默认布局的用例
            //（实测会让 Hierarchy 拖拽落空）。把 Project 重新作为标签页停回 Console 所在停靠节点，
            // 恢复默认布局（Project/Console 同节点）。
            ctx->DockInto("Project", "Console");
            ctx->Yield(2);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
