/**
 * @file ui_tests_console.cpp
 * @brief Console（控制台）面板真实控件级用例（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 点击控制台里始终可见的真实控件（Clear 按钮、Auto-scroll 勾选框）。控件查不到时
 * ItemClick 会让用例失败——以此覆盖控制台工具栏控件可被驱动且不触发断言/崩溃。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

namespace dse::editor::uitest {

void RegisterConsoleTests(ImGuiTestEngine* e) {
    // dse-console/clear_button_click：点击控制台 Clear 按钮（真实控件）。
    // 面板停靠为 tab，非激活 tab 不绘制其内容，故先 WindowFocus 把 Console tab 提到前台。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-console", "clear_button_click");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            ctx->WindowFocus("//Console");
            ctx->SetRef("//Console");
            ctx->ItemClick("Clear");
        };
    }

    // dse-console/toggle_autoscroll：切换 Auto-scroll 勾选框（真实控件）。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-console", "toggle_autoscroll");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            ctx->WindowFocus("//Console");
            ctx->SetRef("//Console");
            ctx->ItemClick("Auto-scroll");
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
