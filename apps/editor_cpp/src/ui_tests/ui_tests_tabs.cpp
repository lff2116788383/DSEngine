/**
 * @file ui_tests_tabs.cpp
 * @brief 多场景页签用例（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 走 DSEngineRoot 内的 "##SceneTabs" 真实 TabBar：
 *   - create_switch_close：用 "+" 新建两页签 → 点页签切换（断言 active index）→ 右键 "Close" 关闭一页。
 *   - close_others       ：多页签下右键 "Close Others" → 仅剩当前一页。
 * 页签 ID 用稳定后缀（label "###SceneTabN"），断言以 SceneTabManager 真实状态为准。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cstdio>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_scene_tabs.h"

namespace dse::editor::uitest {

void RegisterSceneTabTests(ImGuiTestEngine* e) {
    // dse-tabs/create_switch_close
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-tabs", "create_switch_close");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            auto& tabs = SceneTabManager::Get();

            // 用 "+" 按钮新建两个空场景页签。
            const int n0 = tabs.GetTabCount();
            ctx->ItemClick("//DSEngineRoot/##SceneTabs/+");
            ctx->Yield(2);
            ctx->ItemClick("//DSEngineRoot/##SceneTabs/+");
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetTabCount(), n0 + 2);

            // 切到第 0 个页签。
            ctx->ItemClick("//DSEngineRoot/##SceneTabs/SceneTab0");
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetActiveIndex(), 0);

            // 切到最后一个页签。
            const int last = tabs.GetTabCount() - 1;
            char ref[80];
            std::snprintf(ref, sizeof(ref), "//DSEngineRoot/##SceneTabs/SceneTab%d", last);
            ctx->ItemClick(ref);
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetActiveIndex(), last);

            // 右键最后一个页签 → Close → 页签 -1。
            const int before_close = tabs.GetTabCount();
            ctx->ItemClick(ref, ImGuiMouseButton_Right);
            ctx->Yield();
            ctx->SetRef("//$FOCUSED");
            ctx->ItemClick("Close");
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetTabCount(), before_close - 1);
        };
    }

    // dse-tabs/close_others：确保 ≥3 页签后右键某页 → "Close Others" → 仅剩一页。
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-tabs", "close_others");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            auto& tabs = SceneTabManager::Get();

            // 补足到至少 3 个页签。
            while (tabs.GetTabCount() < 3) {
                ctx->ItemClick("//DSEngineRoot/##SceneTabs/+");
                ctx->Yield(2);
            }
            IM_CHECK(tabs.GetTabCount() >= 3);

            // 右键第 0 个页签 → Close Others → 仅剩它自己。
            ctx->ItemClick("//DSEngineRoot/##SceneTabs/SceneTab0", ImGuiMouseButton_Right);
            ctx->Yield();
            ctx->SetRef("//$FOCUSED");
            ctx->ItemClick("Close Others");
            ctx->Yield(2);
            IM_CHECK_EQ(tabs.GetTabCount(), 1);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
