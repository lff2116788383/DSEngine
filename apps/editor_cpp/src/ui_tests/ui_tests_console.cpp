/**
 * @file ui_tests_console.cpp
 * @brief Console panel UI tests (requires DSE_EDITOR_UI_TESTS).
 *
 * Tests console panel features: Clear button, Auto-scroll toggle,
 * Export button, Category filter, Level filter toggles.
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"
#include "../editor_icons.h"

namespace dse::editor::uitest {

void RegisterConsoleTests(ImGuiTestEngine* e) {
    // dse-console/clear_button_click
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-console", "clear_button_click");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            ctx->WindowFocus("//Console");
            ctx->SetRef("//Console");
            ctx->ItemClick("Clear");
        };
    }

    // dse-console/toggle_autoscroll
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-console", "toggle_autoscroll");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            ctx->WindowFocus("//Console");
            ctx->SetRef("//Console");
            ctx->ItemClick("Auto-scroll");
        };
    }

    // dse-console/export_button_click
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-console", "export_button_click");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            ctx->WindowFocus("//Console");
            ctx->SetRef("//Console");
            ctx->ItemClick(MDI_ICON_EXPORT " Export");
        };
    }

    // dse-console/category_filter_combo
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-console", "category_filter_combo");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            ctx->WindowFocus("//Console");
            ctx->SetRef("//Console");
            ctx->ItemClick("##cat_filter");
            ctx->Yield(2);
        };
    }

    // dse-console/level_filter_toggle_info
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-console", "level_filter_toggle_info");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            ctx->WindowFocus("//Console");
            ctx->SetRef("//Console");
            ctx->ItemClick("###info_toggle");
        };
    }

    // dse-console/level_filter_toggle_warn_error
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-console", "level_filter_toggle_warn_error");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            ctx->WindowFocus("//Console");
            ctx->SetRef("//Console");
            ctx->ItemClick("###warn_toggle");
            ctx->ItemClick("###error_toggle");
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
