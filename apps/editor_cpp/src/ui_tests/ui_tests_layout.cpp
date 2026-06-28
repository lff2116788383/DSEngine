/**
 * @file ui_tests_layout.cpp
 * @brief ⑧ 布局/设置持久化 补测（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 在 "Preferences" 面板的真实控件里改值，断言写穿到磁盘 bin/editor_settings.json：
 *   - 主题 Combo（Dark/Light）→ s_theme_index，延时落盘；面板关闭时强制 flush。
 *   - Show Grid 勾选框 / Translate Snap 拖条 → 同上延时落盘。
 *   - Auto Save 勾选框 / Interval 滑杆 → 每次改动即时 SaveEditorSettings。
 * 断言不靠"控件存在"，而是改完后调用 LoadEditorSettings() 重新读盘，比对字段值——
 * 即"真的写进了 json 并能被重新解析回来"。每个用例结尾把改动复位，避免污染全局设置。
 *
 * 注：FlushPreferencesIfNeeded 用 dt 计时延迟落盘；无头快跑下 dt 不可控，故统一靠
 * "关闭面板触发的强制 flush"（DrawPreferencesPanel 在 *p_open==false 且 dirty 时
 * 以一个大 dt 立即落盘）来确保 theme/grid/snap 写盘。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cmath>

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "../editor_settings.h"            // EditorSettings / LoadEditorSettings
#include "../editor_preferences_panel.h"   // GetCurrentThemeIndex / GetShowGrid / GetSnapTranslate

namespace dse::editor::uitest {

namespace {

// 打开并浮动放大 Preferences 面板，把 ref 指向它。
void OpenPreferences(ImGuiTestContext* ctx) {
    ShowFloatingPanel(ctx, Services().show_preferences, "//Preferences");
    ctx->SetRef("//Preferences");
    ctx->Yield(2);
}

// 关闭 Preferences 面板并让主循环再绘一帧——触发其"关闭时强制落盘"分支。
void ClosePreferences(ImGuiTestContext* ctx) {
    *Services().show_preferences = false;
    ctx->Yield(3);
}

} // namespace

void RegisterLayoutSettingsTests(ImGuiTestEngine* e) {
    // ── ⑧-1 主题 Combo 切换 → 落盘 theme_index，并能复位 ──────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-layout", "preferences_theme_persist");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const int orig = GetCurrentThemeIndex();

            OpenPreferences(ctx);
            ctx->ComboClick("Theme/Light");
            ctx->Yield(2);
            IM_CHECK(GetCurrentThemeIndex() == 1);
            ClosePreferences(ctx);
            IM_CHECK(LoadEditorSettings().theme_index == 1);

            // 切回 Dark 并断言落盘，顺带把全局主题复位到初始值。
            OpenPreferences(ctx);
            ctx->ComboClick("Theme/Dark (Default)");
            ctx->Yield(2);
            IM_CHECK(GetCurrentThemeIndex() == 0);
            ClosePreferences(ctx);
            IM_CHECK(LoadEditorSettings().theme_index == 0);
            (void)orig;
        };
    }

    // ── ⑧-2 Show Grid 勾选框 + Translate Snap 拖条 → 落盘 ────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-layout", "preferences_grid_snap_persist");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const bool orig_grid = GetShowGrid();
            const float orig_snap = GetSnapTranslate();

            OpenPreferences(ctx);
            // 翻转 Show Grid。
            ctx->ItemClick("Show Grid");
            ctx->Yield(2);
            IM_CHECK(GetShowGrid() != orig_grid);
            // 改 Translate Snap（Snapping 头默认展开）。
            ctx->ItemInputValue("Translate Snap", 2.5f);
            ctx->Yield(2);
            IM_CHECK(std::abs(GetSnapTranslate() - 2.5f) < 0.01f);
            ClosePreferences(ctx);
            {
                EditorSettings s = LoadEditorSettings();
                IM_CHECK(s.show_grid != orig_grid);
                IM_CHECK(std::abs(s.snap_translate - 2.5f) < 0.01f);
            }

            // 复位 Show Grid 与 Snap，断言复位也落盘。
            OpenPreferences(ctx);
            ctx->ItemClick("Show Grid");
            ctx->Yield(2);
            ctx->ItemInputValue("Translate Snap", orig_snap);
            ctx->Yield(2);
            IM_CHECK(GetShowGrid() == orig_grid);
            ClosePreferences(ctx);
            {
                EditorSettings s = LoadEditorSettings();
                IM_CHECK(s.show_grid == orig_grid);
                IM_CHECK(std::abs(s.snap_translate - orig_snap) < 0.01f);
            }
        };
    }

    // ── ⑧-3 Auto Save 勾选框 + Interval 滑杆 → 即时落盘 ──────────────────────
    {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-layout", "autosave_settings_persist");
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const EditorSettings orig = LoadEditorSettings();

            OpenPreferences(ctx);

            // Auto Save 块每帧从盘加载、改动即时 SaveEditorSettings；翻转 enable。
            ctx->ItemClick("Enable Auto Save");
            ctx->Yield(2);
            IM_CHECK(LoadEditorSettings().auto_save_enabled != orig.auto_save_enabled);

            // 翻回原值，并在 enable 为真时改 Interval 滑杆，断言落盘后再复位。
            ctx->ItemClick("Enable Auto Save");
            ctx->Yield(2);
            IM_CHECK(LoadEditorSettings().auto_save_enabled == orig.auto_save_enabled);

            if (LoadEditorSettings().auto_save_enabled) {
                ctx->ItemInputValue("Interval (sec)", 300);
                ctx->Yield(2);
                IM_CHECK(LoadEditorSettings().auto_save_interval_sec == 300);
                ctx->ItemInputValue("Interval (sec)", orig.auto_save_interval_sec);
                ctx->Yield(2);
                IM_CHECK(LoadEditorSettings().auto_save_interval_sec == orig.auto_save_interval_sec);
            }

            ClosePreferences(ctx);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
