/**
 * @file ui_tests_assets.cpp
 * @brief Asset Browser（资源浏览器）面板真实控件级用例（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 先 EnsureAllPanelsVisible 打开资源浏览器，再向其搜索框真实键入文本——覆盖资源
 * 浏览器的文本输入控件可被驱动且不触发断言/崩溃。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

namespace dse::editor::uitest {

void RegisterAssetBrowserTests(ImGuiTestEngine* e) {
    // dse-assets/search_input_typing：向资源浏览器搜索框键入文本（真实控件输入）。
    ImGuiTest* t = IM_REGISTER_TEST(e, "dse-assets", "search_input_typing");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        EnsureAllPanelsVisible();
        ctx->Yield(4);
        ctx->SetRef("//Asset Browser");
        ctx->ItemInputValue("##asset_search", "cube");
    };
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
