/**
 * @file ui_tests_common.cpp
 * @brief UI 测试跨用例共享的工具实现 + 全部用例的注册分发入口（仅 DSE_EDITOR_UI_TESTS 编入）。
 *
 * 用例体（GuiFunc/TestFunc）是函数指针（见 imconfig STD_FUNCTION=0），无法捕获 this，
 * 因此一切共享状态/动作都走这里的自由函数 + Services() 全局只读句柄。
 */
#include "ui_tests_internal.h"

#ifdef DSE_EDITOR_UI_TESTS

#include <cstring>

#include <entt/entt.hpp>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_context.h"

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"

namespace dse::editor::uitest {

int CountValidEntities() {
    auto* engine = Services().engine;
    if (!engine || !engine->pipeline()) return -1;
    entt::registry& registry = engine->pipeline()->world().registry();
    int count = 0;
    for (auto e : registry.storage<entt::entity>()) {
        if (registry.valid(e)) ++count;
    }
    return count;
}

void EnsureAllPanelsVisible() {
    const UiTestServices& s = Services();
    bool* const toggles[] = {
        s.show_localization_preview, s.show_profiler, s.show_animation,
        s.show_tile_palette, s.show_terrain_editor, s.show_lua_console,
        s.show_undo_history, s.show_asset_browser, s.show_animation_timeline,
        s.show_navmesh, s.show_shader_graph, s.show_git, s.show_multi_viewport,
        s.show_anim_state_machine, s.show_lua_debugger, s.show_streaming_debug,
        s.show_curve_editor, s.show_visual_script, s.show_anim_retarget,
        s.show_preferences, s.show_plugins, s.show_chat,
    };
    for (bool* p : toggles)
        if (p) *p = true;
}

void HideOptionalPanels() {
    const UiTestServices& s = Services();
    bool* const toggles[] = {
        s.show_localization_preview, s.show_profiler, s.show_animation,
        s.show_tile_palette, s.show_terrain_editor, s.show_lua_console,
        s.show_undo_history, s.show_asset_browser, s.show_animation_timeline,
        s.show_navmesh, s.show_shader_graph, s.show_git, s.show_multi_viewport,
        s.show_anim_state_machine, s.show_lua_debugger, s.show_streaming_debug,
        s.show_curve_editor, s.show_visual_script, s.show_anim_retarget,
        s.show_preferences, s.show_plugins, s.show_chat,
    };
    for (bool* p : toggles)
        if (p) *p = false;
}

ImGuiWindow* FindActiveWindow(const char* name_or_substr) {
    ImGuiContext& g = *ImGui::GetCurrentContext();
    ImGuiWindow* substr_hit = nullptr;
    for (ImGuiWindow* w : g.Windows) {
        if (!w->WasActive) continue;
        if (std::strcmp(w->Name, name_or_substr) == 0)
            return w;  // 精确匹配优先（避免 "Console" 误命中 "Lua Console"）
        if (!substr_hit && std::strstr(w->Name, name_or_substr) != nullptr)
            substr_hit = w;
    }
    return substr_hit;
}

void OpenHierarchyContextMenu(ImGuiTestContext* ctx) {
    // 在常驻、必定存在的 "Scene" 根节点上右键打开窗口上下文菜单（BeginPopupContextWindow
    // 不设 NoOpenOverItems，故在节点上右键同样会弹出窗口菜单）。相比“点窗口底部留白”，
    // 这条在实体数增多、树撑满窗口时仍稳定——空白处会被树节点占满导致点不中。
    // 用绝对引用 "//Hierarchy/Scene"：避免按当前 ref（上一轮可能停在 "//$FOCUSED" 弹窗）解析错误。
    ctx->WindowFocus("//Hierarchy");
    ctx->MouseMove("//Hierarchy/Scene");
    ctx->MouseClick(ImGuiMouseButton_Right);
    ctx->SetRef("//$FOCUSED");
}

void ManualMouseDrag(ImGuiTestContext* ctx, const ImVec2& src, const ImVec2& dst) {
    // ImGui 拖拽投递需要“源激活→跨帧拖动→落点悬停一帧→释放”，分步并逐帧 Yield 比单帧瞬移更可靠。
    ctx->MouseMoveToPos(src);
    ctx->Yield();
    ctx->MouseDown(ImGuiMouseButton_Left);
    ctx->Yield();
    ctx->MouseLiftDragThreshold();
    ctx->Yield();
    const ImVec2 mid((src.x + dst.x) * 0.5f, (src.y + dst.y) * 0.5f);
    ctx->MouseMoveToPos(mid);
    ctx->Yield();
    ctx->MouseMoveToPos(dst);
    ctx->Yield();
    ctx->MouseMoveToPos(dst);  // 落点多停一帧，确保目标 BeginDragDropTarget 命中
    ctx->Yield(2);
    ctx->MouseUp(ImGuiMouseButton_Left);
    ctx->Yield(2);
}

void RegisterAllUiTests(ImGuiTestEngine* engine) {
    RegisterHarnessSanityTests(engine);
    RegisterPanelRenderTests(engine);
    RegisterHierarchyTests(engine);
    RegisterInspectorTests(engine);
    RegisterConsoleTests(engine);
    RegisterMenuBarTests(engine);
    RegisterAssetBrowserTests(engine);
    // 编辑器主链路基础操作（撤销/播放/场景/快捷键/拖拽）：这些会改场景页签/实体/播放态，
    // 各用例自足、互不依赖，统一排在面板基线之后。
    RegisterUndoTests(engine);
    RegisterPlayTests(engine);
    RegisterSceneTests(engine);
    RegisterShortcutTests(engine);
    RegisterDragDropTests(engine);
    RegisterSceneTabTests(engine);
    // 负向/边界用例（循环父子化被拒、空栈撤销、删根节点不崩）。
    RegisterNegativeTests(engine);
    // 项目级基础操作（新建/打开/保存）放最后：会切换当前打开项目，避免影响前面的用例。
    RegisterProjectTests(engine);
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
