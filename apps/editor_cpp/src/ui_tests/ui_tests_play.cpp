/**
 * @file ui_tests_play.cpp
 * @brief 播放/停止（Play/Stop）生命周期用例（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 走工具栏（Toolbar 窗口）真实的播放/停止按钮，断言编辑器状态在 Edit↔Play 间正确切换、
 * 且进出 Play 模式不崩。按钮 ID 与 editor_toolbar.cpp 一致（图标 + "##play"/"##stop"）。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_toolbar.h"  // IsEditorInPlayMode
#include "../editor_icons.h"

namespace dse::editor::uitest {

#define DSE_BTN_PLAY "//Toolbar/" MDI_ICON_PLAY "##play"
#define DSE_BTN_STOP "//Toolbar/" MDI_ICON_STOP "##stop"

void RegisterPlayTests(ImGuiTestEngine* e) {
    // dse-play/play_stop_toolbar：点 Play 进入播放态，点 Stop 回到编辑态。
    ImGuiTest* t = IM_REGISTER_TEST(e, "dse-play", "play_stop_toolbar");
    t->TestFunc = [](ImGuiTestContext* ctx) {
        ctx->WindowFocus("//Toolbar");

        // 前置：确保处于编辑态（若上一用例残留在播放态则先停止）。
        if (IsEditorInPlayMode()) {
            ctx->ItemClick(DSE_BTN_STOP);
            ctx->Yield(3);
        }
        IM_CHECK(!IsEditorInPlayMode());

        ctx->ItemClick(DSE_BTN_PLAY);
        ctx->Yield(4);
        IM_CHECK(IsEditorInPlayMode());

        ctx->ItemClick(DSE_BTN_STOP);
        ctx->Yield(4);
        IM_CHECK(!IsEditorInPlayMode());
    };
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
