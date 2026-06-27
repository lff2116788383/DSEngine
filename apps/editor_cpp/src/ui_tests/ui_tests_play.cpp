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
#include "../editor_scene_tabs.h"
#include "../editor_icons.h"

namespace dse::editor::uitest {

#define DSE_BTN_PLAY "//Toolbar/" MDI_ICON_PLAY "##play"
#define DSE_BTN_STOP "//Toolbar/" MDI_ICON_STOP "##stop"

void RegisterPlayTests(ImGuiTestEngine* e) {
    // dse-play/play_stop_toolbar：点 Play 进入播放态，点 Stop 回到编辑态。
    {
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

    // dse-play/readonly_blocks_edits：进入 Play 模式后 context.read_only=true，编辑类快捷键应被拦下。
    // 以 Ctrl+N（新建场景）为探针：Play 态下页签数不应增加（场景文件操作被 read_only 守卫拦截）；
    // 退出 Play 后同一快捷键应恢复生效。页签数不受游戏逻辑影响，断言确定性强。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-play", "readonly_blocks_edits");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            auto& tabs = SceneTabManager::Get();

            ctx->WindowFocus("//Toolbar");
            if (IsEditorInPlayMode()) {
                ctx->ItemClick(DSE_BTN_STOP);
                ctx->Yield(3);
            }
            IM_CHECK(!IsEditorInPlayMode());

            ctx->ItemClick(DSE_BTN_PLAY);
            ctx->Yield(4);
            IM_CHECK(IsEditorInPlayMode());

            // Play 态：Ctrl+N 应被 read_only 拦截 → 页签数不变。
            const int tabs_play = tabs.GetTabCount();
            ctx->SetRef("//DSEngineRoot");
            ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_N);
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetTabCount(), tabs_play);  // 被拦截，无新建

            // 退出 Play：同一快捷键恢复生效 → 页签 +1。
            ctx->WindowFocus("//Toolbar");
            ctx->ItemClick(DSE_BTN_STOP);
            ctx->Yield(4);
            IM_CHECK(!IsEditorInPlayMode());

            const int tabs_edit = tabs.GetTabCount();
            ctx->SetRef("//DSEngineRoot");
            ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_N);
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetTabCount(), tabs_edit + 1);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
