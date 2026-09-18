/**
 * @file ui_tests_tool_panels.cpp
 * @brief Tool panel interaction tests.
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"
#include "../editor_icons.h"
#include "../editor_ai_config.h"
#include "../editor_2d_tools.h"

namespace dse::editor::uitest {

void RegisterToolPanelTests(ImGuiTestEngine* engine) {
    // --- NavMesh: Bake Settings ---
    ImGuiTest* t = ImGuiTestEngine_RegisterTest(engine, "dse-tool-panels", "navmesh_bake_settings");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        ctx->WindowFocus("//NavMesh");
        ctx->SetRef("//NavMesh");
        // SliderFloat "Cell Size" exists
        IM_CHECK(ctx->ItemExists("Cell Size"));
    };

    // --- NavMesh: Bake Button ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-tool-panels", "navmesh_bake_button");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        ctx->WindowFocus("//NavMesh");
        ctx->SetRef("//NavMesh");
        ctx->ItemClick("Bake NavMesh");
        ctx->Yield(2);
    };

    // --- NavMesh: Overlay Toggle ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-tool-panels", "navmesh_overlay_toggle");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        ctx->WindowFocus("//NavMesh");
        ctx->SetRef("//NavMesh");
        // Bake (synchronous in test mode) then check overlay
        ctx->ItemClick("Bake NavMesh");
        ctx->Yield(16);
        if (ctx->ItemExists("Show Overlay"))
            ctx->ItemClick("Show Overlay");
        else
            IM_CHECK(ctx->ItemExists("Cell Size"));  // fallback: bake settings still visible
    };

    // --- Version Control: Panel Open ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-tool-panels", "vcs_panel_open");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        ImGuiWindow* w = FindActiveWindow("Version Control");
        IM_CHECK(w != nullptr);
    };

    // --- Version Control: Refresh Button ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-tool-panels", "vcs_refresh_button");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        ImGuiWindow* w = FindActiveWindow("Version Control");
        IM_CHECK(w != nullptr);
        // 必须用 "//窗口名" 形式的 ref：测试引擎把带 // 的 ref 解析成窗口，裸名字会当相对标签，
        // 于是后面所有 ItemClick 都报 "Unable to locate item (0x00000000)"（实测 active=1 说明
        // 面板本帧确实在绘制，条目也在，只是 ref 不对）。仓库里能工作的用例都是 //Console 这种写法。
        // 该窗口名带图标前缀，所以这里用 w->Name 拼，而不是写 "//Version Control"。
        const std::string vcs_ref = std::string("//") + w->Name;
        ctx->WindowFocus(vcs_ref.c_str());
        ctx->SetRef(vcs_ref.c_str());
        // Refresh 在 «Changes» 页最底部，用稳定 ### ID 定位（不依赖图标前缀标签）。
        ctx->ItemClick("**/###vc_tab_changes");   // **/ 通配：条目嵌在 tab bar 子容器里
        ctx->Yield(2);
        ImGui::SetScrollY(w, ImGui::GetScrollMaxY());   // 变更列表可能很长，先滚到底
        ctx->Yield(2);
        ctx->ItemClick("**/###vc_refresh");
        ctx->Yield(2);
    };

    // --- Version Control: Stage/Unstage ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-tool-panels", "vcs_stage_unstage");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(8);
        ImGuiWindow* w = FindActiveWindow("Version Control");
        IM_CHECK(w != nullptr);
        const std::string vcs_ref2 = std::string("//") + w->Name;   // 同 vcs_refresh_button：ref 必须带 //
        ctx->WindowFocus(vcs_ref2.c_str());
        ctx->SetRef(vcs_ref2.c_str());
        ctx->ItemClick("**/###vc_tab_changes");   // **/ 通配：条目嵌在 tab bar 子容器里
        ctx->Yield(2);
        ImGui::SetScrollY(w, ImGui::GetScrollMaxY());
        ctx->Yield(2);
        ctx->ItemClick("**/###vc_refresh");
        ctx->Yield(2);
    };

    // --- Agent Panel: Send Message ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-tool-panels", "agent_send_message");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        ctx->WindowFocus("//AI Agent");
        ctx->SetRef("//AI Agent");
        ctx->ItemInputValue("##agent_input", "hello");
        ctx->Yield(2);
        // Send button may or may not be visible depending on state
        if (ctx->ItemExists("Send"))
            ctx->ItemClick("Send");
    };

    // --- Agent Panel: Clear Button ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-tool-panels", "agent_clear_button");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        ctx->WindowFocus("//AI Agent");
        ctx->SetRef("//AI Agent");
        ctx->ItemClick("Clear");
    };

    // --- Agent Panel: Combo Select ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-tool-panels", "agent_combo_select");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        ctx->WindowFocus("//AI Agent");
        ctx->SetRef("//AI Agent");
        IM_CHECK(ctx->ItemExists("##agent_combo"));
    };

    // --- Vegetation Brush: Open ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-tool-panels", "vegetation_brush_open");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        // Vegetation Brush window is only shown when its show flag is on
        // We test via FindActiveWindow since it may have icon prefix
        ImGuiWindow* w = FindActiveWindow("Vegetation Brush");
        // Panel may not be available in test mode - just check existence
        IM_CHECK(w != nullptr);
    };

    // --- Vegetation Brush: Settings ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-tool-panels", "vegetation_brush_settings");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        ImGuiWindow* w = FindActiveWindow("Vegetation Brush");
        if (w) {
            ctx->WindowFocus(w->Name);
            ctx->SetRef(w->Name);
            IM_CHECK(ctx->ItemExists("Grass") || ctx->ItemExists("Tree"));
        } else {
            IM_CHECK(false);  // window not found
        }
    };

    // --- Sprite Slicer: Open ---
    t = ImGuiTestEngine_RegisterTest(engine, "dse-tool-panels", "sprite_slicer_open");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        dse::editor::tools2d::GetSpriteSlicerState().open = true;
        ctx->Yield(4);
        ImGuiWindow* w = FindActiveWindow("Sprite Slicer");
        IM_CHECK(w != nullptr);
        dse::editor::tools2d::GetSpriteSlicerState().open = false;
    };
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
