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
#include <filesystem>
#include <string>
#include <system_error>

#include <entt/entt.hpp>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_context.h"

#include "../editor_project.h"  // ProjectManager

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

void DiscardSceneCloseConfirmIfOpen(ImGuiTestContext* ctx) {
    // 关闭脏页签后会弹出标题为 "Unsaved Changes###SceneCloseConfirm" 的确认框并取得焦点；
    // 把 ref 指向当前焦点窗口，若其中存在 "Don't Save" 则点击丢弃改动完成关闭。无确认框时为 no-op。
    ctx->SetRef("//$FOCUSED");
    if (ctx->ItemExists("Don't Save")) {
        ctx->ItemClick("Don't Save");
        ctx->Yield(2);
    }
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

std::string ProjectAssetBaseDir() {
    namespace fs = std::filesystem;
    auto& pm = ProjectManager::Get();
    fs::path base = pm.HasOpenProject()
        ? fs::path(pm.GetAssetDir())
        : (fs::current_path() / "samples" / "lua" / "data");
    std::error_code ec;
    fs::create_directories(base, ec);
    return base.string();
}

void MakeProjectPanelFloating(ImGuiTestContext* ctx) {
    // 关掉浮动可选面板（避免压住落点），让 Project 刷新目录列表后浮动放大到右侧空白处。
    HideOptionalPanels();
    ctx->Yield(6);
    // UndockWindow 内部用 GetWindowByRef(window_name) 解析窗口，且该名字按“当前 ref”解析。
    // 若调用方先做过别的交互（如右键 Hierarchy 建实体）遗留了非根 ref，"Project" 会被解析成
    // "//<上次ref>/Project" 而找不到窗口 → GetWindowByRef 返回 null → UndockWindow 解引用空指针崩溃
    // （imgui_test_engine 的 UndockWindow 缺少 UndockNode 那样的 null 兜底）。这里把 ref 复位到根、
    // 并用绝对窗口名 "//Project"，保证无论调用顺序如何都按绝对窗口解析，规避该空指针崩溃。
    ctx->SetRef("");
    ctx->UndockWindow("//Project");
    ctx->Yield(2);
    ctx->WindowMove("//Project", ImVec2(420.0f, 80.0f));
    ctx->WindowResize("//Project", ImVec2(520.0f, 520.0f));
    ctx->Yield(2);
}

void RestoreProjectPanelDock(ImGuiTestContext* ctx) {
    // 把 Project 作为标签页停回 Console 所在停靠节点，恢复默认布局，避免污染后续依赖布局的用例。
    // DockInto 的窗口名按当前 ref 解析；若调用方遗留了非根 ref（如 "//Project"），"Project"
    // 会被解析成 "//Project/Project" 而找不到窗口，触发 imgui_te_context 的 docking 断言。
    // 这里先把 ref 复位到根，保证窗口名按绝对窗口解析。
    ctx->SetRef("");
    ctx->DockInto("Project", "Console");
    ctx->Yield(2);
}

void DragProjectAssetOntoScene(ImGuiTestContext* ctx, const char* filename, const char* type_icon) {
    // 列表视图文件项：Table("project_list") -> PushID(filename) -> Selectable("<icon>  <filename>")。
    // ref 须含 table 与 PushID 两层。
    const std::string asset_ref =
        std::string("//Project/project_list/") + filename + "/" + type_icon + "  " + filename;
    const char* scene_ref = "//Hierarchy/Scene";

    const ImGuiTestItemInfo si = ctx->ItemInfo(scene_ref);
    IM_CHECK_SILENT(si.ID != 0);
    const ImGuiTestItemInfo ai = ctx->ItemInfo(asset_ref.c_str());
    IM_CHECK_SILENT(ai.ID != 0);

    ctx->ItemDragAndDrop(asset_ref.c_str(), scene_ref);
    ctx->Yield(2);
}

void ShowFloatingPanel(ImGuiTestContext* ctx, bool* show, const char* window_ref) {
    HideOptionalPanels();
    ctx->Yield(4);
    if (show) *show = true;
    ctx->Yield(4);
    // 这些可选面板首帧以浮动窗出现；移到中部空白并放大，避免内部控件被裁剪/遮挡。
    ctx->WindowMove(window_ref, ImVec2(180.0f, 70.0f));
    ctx->WindowResize(window_ref, ImVec2(940.0f, 580.0f));
    // 置顶到最前：否则 ImGuizmo 每帧建的全屏覆盖窗会盖住面板、截走点击（见 ManualMouseDrag 注释）。
    ctx->WindowFocus(window_ref);
    ctx->Yield(2);
}

void DeselectAll(ImGuiTestContext* ctx) {
    // Hierarchy 面板里“点空白处”会把 selected_entity 置空并清 SelectionManager（见 editor_hierarchy_panel）。
    ctx->WindowFocus("//Hierarchy");
    ctx->Yield();
    ImGuiWindow* h = FindActiveWindow("Hierarchy");
    if (!h) return;
    // 点窗口底部靠下的空白区域：那里位于树节点之下，满足“窗口悬停且无任何 item 悬停”。
    const ImVec2 pos(h->Pos.x + h->Size.x * 0.5f, h->Pos.y + h->Size.y - 10.0f);
    ctx->MouseMoveToPos(pos);
    ctx->MouseClick(ImGuiMouseButton_Left);
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
    // 第二批补测分组（10 块缺口）。这些用例多依赖 Hierarchy 右键创建实体 + Inspector，
    // 自足且会自行清理浮动布局/磁盘文件，排在主链路之后、项目级操作之前。
    RegisterPrefabTests(engine);
    RegisterGizmoTests(engine);
    RegisterAnimationTests(engine);
    RegisterComponentFieldTests(engine);
    RegisterAssetMgmtTests(engine);
    RegisterGraphTests(engine);
    RegisterTerrainTilemapTests(engine);
    RegisterLayoutSettingsTests(engine);
    RegisterMiscEditorTests(engine);
    RegisterMultiSelectTests(engine);
    // 项目级基础操作（新建/打开/保存）放最后：会切换当前打开项目，避免影响前面的用例。
    RegisterProjectTests(engine);
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
