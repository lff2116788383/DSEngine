/**
 * @file ui_tests_panels.cpp
 * @brief 全部编辑器面板的“打开即渲染”覆盖用例（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 目标：自动化覆盖到每一个面板——无需逐个写交互脚本，先保证「面板能在无头下被绘制、
 * 不触发任何 ImGui 断言/崩溃」这一底线。做法：每个面板一条 dse-panels/<slug> 用例，
 * 先 EnsureAllPanelsVisible() 把所有可见性开关置真，Yield 数帧让新开面板完成首帧布局，
 * 再断言对应窗口已出现在 g.Windows 且上一帧仍在绘制（WasActive）。
 *
 * 用例体是函数指针无法捕获，故把“待校验窗口名”塞进 ImGuiTest::UserData，由统一的
 * TestFunc 读取——既共用一份逻辑，又能逐面板给出独立的 pass/fail。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

namespace dse::editor::uitest {

namespace {

struct PanelEntry {
    const char* slug;    ///< 用例名（dse-panels/<slug>）
    const char* window;  ///< 期望出现的窗口名（FindActiveWindow 先精确后子串）
};

// 覆盖编辑器全部可达面板。常驻面板（Hierarchy/Inspector/Project/Console/Material/
// Scene/Game/Toolbar）无条件绘制；其余由 show_* 开关控制，EnsureAllPanelsVisible 置真。
// 带图标前缀的标题（如 Lua Console = "<icon>  Lua Console"）用子串匹配命中。
constexpr PanelEntry kPanels[] = {
    // 常驻面板
    {"hierarchy",            "Hierarchy"},
    {"inspector",            "Inspector"},
    {"project",             "Project"},
    {"console",             "Console"},
    {"material",            "Material"},
    {"scene_viewport",      "Scene"},
    {"game_viewport",       "Game"},
    {"toolbar",             "Toolbar"},
    // 开关控制面板
    {"profiler",            "Profiler"},
    {"animation",           "Animation"},
    {"tile_palette",        "Tile Palette"},
    {"terrain_brush",       "Terrain Brush"},
    {"lua_console",         "Lua Console"},
    {"localization_preview","Localization Preview"},
    {"undo_history",        "Undo History"},
    {"asset_browser",       "Asset Browser"},
    {"animation_timeline",  "Animation Timeline"},
    {"navmesh",             "NavMesh"},
    {"shader_graph",        "Shader Graph"},
    {"git",                 "Git"},
    {"multi_viewport",      "Multi-Viewport"},
    {"anim_state_machine",  "Anim State Machine"},
    {"lua_debugger",        "Lua Debugger"},
    {"streaming_debug",     "Streaming Debug"},
    {"curve_editor",        "Curve Editor"},
    {"visual_script",       "Visual Script"},
    {"anim_retarget",       "Anim Retarget"},
    {"preferences",         "Preferences"},
    {"plugins",             "Plugins"},
    {"ai_chat",             "AI Chat"},
    // New editor feature panels (#2-#9)
    {"vs_debugger",         "Visual Script Debugger"},
    {"animation_clip",      "Animation Clip Editor"},
    {"sequencer",           "Cinematic Sequencer"},
    {"terrain_sculpt",      "Terrain Sculpt Preview"},
    {"world_partition",     "World Partition Editor"},
    {"plugin_hot_reload",   "Plugin Hot Reload"},
    {"version_control",     "Version Control"},
};

} // namespace

void RegisterPanelRenderTests(ImGuiTestEngine* e) {
    for (const PanelEntry& panel : kPanels) {
        ImGuiTest* t = IM_REGISTER_TEST(e, "dse-panels", panel.slug);
        t->UserData = const_cast<char*>(panel.window);
        t->TestFunc = [](ImGuiTestContext* ctx) {
            const char* window_name = static_cast<const char*>(ctx->Test->UserData);
            EnsureAllPanelsVisible();
            // 等若干帧：开关刚置真，面板需要至少一帧才会进入绘制并完成布局。
            ctx->Yield(4);
            ImGuiWindow* w = FindActiveWindow(window_name);
            if (w == nullptr)
                ctx->LogError("panel window not found or not active: '%s'", window_name);
            IM_CHECK(w != nullptr);
        };
    }
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
