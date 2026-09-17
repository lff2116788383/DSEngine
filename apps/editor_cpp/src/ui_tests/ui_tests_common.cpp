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
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>

#include <entt/entt.hpp>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_te_context.h"

// 写路径门面：UI 用例的起步工程经与自动化相同的 dsengine_* 工具路径打开。
#include "apps/editor_cpp/core/command_bus.h"
#include "apps/editor_cpp/core/editor_command.h"

#include "../editor_autosave.h"   // AutoSaveManager（起步清恢复弹窗）
#include "../editor_project.h"  // ProjectManager
#include "../editor_scene_io.h"  // LoadScene / SetCurrentScenePath（起步显式加载场景）
#include "../editor_selection.h"  // SelectionManager (ResetUiState)
#include "../editor_shell.h"      // ResetEditorLayout（起步复位布局）

#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"

namespace dse::editor::uitest {

namespace {
// UI 用例的起步工程：与自动化批处理用例共用同一个 testdata 工程，保证两边看到同一份
// 初始场景（4 个实体：MainCamera/Player/Ground/Light），避免再维护第二份夹具。
// 实测启动时工程虽已打开但场景为空，故必须显式装载而非依赖默认状态。
constexpr const char* kUiTestProject = "tests/automation/testdata/projects/simple_2d_game/project.dseproj";
}  // namespace

const char* UiTestProjectPath() { return kUiTestProject; }

bool UiTestProjectReady() {
    const UiTestServices& s = Services();
    if (!s.engine) return false;
    // 工程由 EditorApp::Init 的 DSE_EDITOR_UI_TESTS 分支固定打开（同一个路径常量），
    // 这里只校验结果：不重复 OpenProject（那会 CloseProject+重载场景，反而引入抖动）。
    return dse::editor::ProjectManager::Get().HasOpenProject();
}

void ClearUiTestRunState() {
    namespace fs = std::filesystem;
    // 1) ImGui 布局 ini 两不沾：置空 IniFilename 后既不读也不写（ImGui 在 NewFrame 首次惰性
    //    加载），于是每次跑都从默认布局起步、也不污染开发者本地布局——本地 ini 曾把面板排到
    //    视口之外（实测 Console 点击目标 y=842 > 视口高 720）。
    ImGui::GetIO().IniFilename = nullptr;
    dse::editor::ResetEditorLayout();
    // 2) autosave 恢复：上一轮运行会让工程变脏并被自动保存，下一次启动就弹
    //    "AutoSave Recovery" 抢走焦点（实测 WindowFocus("//Console") 报
    //    "Expected focused window 'Console', but 'AutoSave Recovery' got focus back"），
    //    后续所有 WindowFocus/点击都失败。清掉本工程残留的 autosave 后重查一次以清 pending。
    std::error_code ec;
    const fs::path project_dir = fs::path(kUiTestProject).parent_path();
    const fs::path autosave = project_dir / ".editor" / "autosave";
    const std::uintmax_t removed = fs::remove_all(autosave, ec);
    dse::editor::AutoSaveManager::Get().CheckRecovery();
    // 3) 起步场景：走与自动化完全相同的工具路径打开工程（OpenProjectCmd → dsengine_project_open，
    //    它内部会加载 default_scene）——不要在 harness 里自己 LoadScene，那会让场景页签/脏标记
    //    等状态偏离编辑器正常路径（实测 dse-undo/dse-dragdrop/dse-negative/dse-terrain 各差 1 例）。
    //    打开前删掉派生 .bin 缓存：LoadScene 在 .bin 比 json 新时会优先读缓存，
    //    等于把「上一轮跑出来的状态」带进下一轮。
    int entities = -1;
    if (auto* engine = Services().engine) {
        auto& pm = dse::editor::ProjectManager::Get();
        const std::string scene_rel = pm.GetDescriptor().default_scene;
        if (!scene_rel.empty()) {
            const fs::path scene_path = pm.GetProjectRoot() / scene_rel;
            fs::remove(scene_path.string() + ".bin", ec);
        }
        // 恒定走工具路径装载：编辑器自身启动路径加载出的实体数实测是 3（与夹具的 4 不一致，
        // 取决于它内部走了哪条分支），用例的实体计数断言因此不稳定；工具路径
        // （OpenProjectCmd → dsengine_project_open → LoadScene(default_scene)）结果是确定的 4。
        if (Services().bus) {
            if (!Services().bus->dispatch(dse::editor::core::OpenProjectCmd{kUiTestProject},
                                          *engine).ok) {
                UiDiagLog("[startup] 打开测试工程失败（工具返回 !ok）");
            }
        }
        entities = CountValidEntities();
    }
    UiDiagLog("[startup] cleared_autosave=%llu layout=default(no ini) entities=%d",
              static_cast<unsigned long long>(removed), entities);
}

void UiDiagLog(const char* fmt, ...) {
    static const char* kDiagPath = "bin/ui_test_diag.txt";
    std::FILE* f = std::fopen(kDiagPath, "a");
    if (!f) return;
    va_list args;
    va_start(args, fmt);
    std::vfprintf(f, fmt, args);
    va_end(args);
    std::fputc('\n', f);
    std::fclose(f);
}

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
        s.show_curve_editor, s.show_anim_retarget,
        s.show_preferences, s.show_plugins, s.show_chat, s.show_blueprint,
        s.show_vegetation_brush, s.show_sprite3d_preview, s.show_animation_clip,
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
        s.show_curve_editor, s.show_anim_retarget,
        s.show_preferences, s.show_plugins, s.show_chat, s.show_blueprint,
        s.show_sequencer, s.show_sprite3d_preview, s.show_animation_clip,
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

void ResetUiState(ImGuiTestContext* ctx) {
    // 防止上一用例残留的 UI 状态泄漏到本用例（根因：用例间 hover/弹窗/ref/多选未复位，
    // 导致本用例的 MouseMove/ItemClick 命中上个用例的残留控件）。
    // 顺序：先清多选 → 关弹窗 → 复位 ref → Yield 沉淀。
    //
    // 注意：这里**不再调用 MouseMoveToVoid**。实测（ui_tests_diag.txt 的 [menu] 链）：
    // 上游 MouseMoveToVoid → GetPosOnVoid 在编辑器这种全屏 dockspace + 浮动面板布局里
    // 找不到 void 位置，会**移动窗口**去造一个 void，把待操作的窗口挪走，
    // 使紧随其后的 ItemInfo/点击全部失效（found=1 → found=0）——dse-hierarchy 整组
    // 因此从「点了没反应」变成全红。清 hover 改为由调用方按显式坐标点击来完成。
    SelectionManager::Get().Clear();
    ctx->SetRef("");
    // 按两次 Escape 关闭可能残留的右键菜单/弹窗（按一次可能只关一层）。
    ctx->KeyPress(ImGuiKey_Escape, 2);
    ctx->Yield(2);
}

void OpenHierarchyContextMenu(ImGuiTestContext* ctx) {
    // 复位只清选择/关残留弹窗，**故意不做 MouseMoveToVoid**。
    //
    // 实测（bin/ui_test_diag.txt 的 [menu] 链）：MouseMoveToVoid 会调用上游 GetPosOnVoid，
    // 而编辑器是全屏 dockspace + 浮动面板的布局，找不到「void」位置时它会**移动窗口**去造一个；
    // Hierarchy 面板随之被挪走/裁切，紧随其后的 '//Hierarchy/Scene' 查询立刻失效
    //（found=1 → found=0），右键菜单永远打不开——这就是本组用例此前成片「点了没反应」的根因。
    SelectionManager::Get().Clear();
    ctx->SetRef("");
    ctx->KeyPress(ImGuiKey_Escape, 2);
    ctx->Yield(2);

    // 在常驻、必定存在的 "Scene" 根节点上右键打开窗口上下文菜单（BeginPopupContextWindow
    // 不设 NoOpenOverItems，故在节点上右键同样会弹出窗口菜单）。
    // 先按绝对引用取节点实测矩形，再用显式坐标合成右键（MouseMoveToPos + MouseDown/Up）——
    // 物理落点比 ctx->MouseMove(ref) 更可靠（与 ManualMouseDrag 同样的取舍）。
    //
    // 开窗校验 + 重试：EnsureAllPanelsVisible 之类用例会一次性打开几十个面板，Hierarchy
    // 可能被后绘制的面板在目标坐标上盖住，右键就打不开菜单。因此每次点击后校验
    // OpenPopupStack，未开则重新置顶重试；只在失败路径落盘诊断（避免正常运行时刷文件）。
    ctx->SetRef("");
    for (int attempt = 0; attempt < 3; ++attempt) {
        ctx->WindowFocus("//Hierarchy");
        ctx->Yield(2);
        const ImGuiTestItemInfo node =
            ctx->ItemInfo("//Hierarchy/Scene", ImGuiTestOpFlags_NoError);
        if (node.ID == 0 || node.RectClipped.GetWidth() <= 0.0f ||
            node.RectClipped.GetHeight() <= 0.0f) {
            UiDiagLog("[menu] attempt=%d 节点不可用 ID=%u rect=%.0fx%.0f", attempt, node.ID,
                      node.RectClipped.GetWidth(), node.RectClipped.GetHeight());
            continue;
        }
        const ImVec2 center = node.RectClipped.GetCenter();
        ctx->MouseMoveToPos(center);
        ctx->Yield(2);
        ctx->MouseDown(ImGuiMouseButton_Right);
        ctx->Yield(2);
        ctx->MouseUp(ImGuiMouseButton_Right);
        ctx->Yield(3);
        ImGuiContext& g = *ImGui::GetCurrentContext();
        if (g.OpenPopupStack.Size > 0) {
            ctx->SetRef("//$FOCUSED");
            return;
        }
        UiDiagLog("[menu] attempt=%d 未打开菜单 clicked=(%.0f,%.0f) hovered=%s",
                  attempt, center.x, center.y,
                  g.HoveredWindow ? g.HoveredWindow->Name : "(none)");
    }
    ctx->LogError("OpenHierarchyContextMenu: 右键 3 次仍未打开上下文菜单");
}

void UndockPanel(ImGuiTestContext* ctx, const char* window_name) {
    // 见声明处注释：上游 UndockWindow 解析窗口后无 null 兜底，会空指针崩溃。
    // 先复位 ref 到根（使绝对名 "//Xxx" 稳定解析），再用与上游同样的 GetWindowByRef
    // 确认窗口确实存在后才调用；不存在则跳过，杜绝该崩溃。
    ctx->SetRef("");
    if (ctx->GetWindowByRef(window_name) != nullptr)
        ctx->UndockWindow(window_name);
    ctx->Yield(2);
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
    // ImGui 拖拽投递需要"源激活→跨帧拖动→落点悬停→释放"。
    // 用物理偏移代替 MouseLiftDragThreshold，在后台/无头环境下更可靠。
    ctx->MouseMoveToPos(src);
    ctx->Yield(2);
    ctx->MouseDown(ImGuiMouseButton_Left);
    ctx->Yield(2);

    // 物理偏移 8 像素超越拖拽阈值（默认 ~6px），不依赖 MouseLiftDragThreshold。
    const float nudge = 8.0f;
    const ImVec2 dir(dst.x - src.x, dst.y - src.y);
    const float len = ImSqrt(dir.x * dir.x + dir.y * dir.y);
    const ImVec2 nudge_pos = (len > 0.01f)
        ? ImVec2(src.x + dir.x / len * nudge, src.y + dir.y / len * nudge)
        : ImVec2(src.x + nudge, src.y);
    ctx->MouseMoveToPos(nudge_pos);
    ctx->Yield(2);

    const ImVec2 mid((src.x + dst.x) * 0.5f, (src.y + dst.y) * 0.5f);
    ctx->MouseMoveToPos(mid);
    ctx->Yield(2);
    ctx->MouseMoveToPos(dst);
    ctx->Yield(2);
    ctx->MouseMoveToPos(dst);  // 落点多停两帧，确保目标 BeginDragDropTarget 命中
    ctx->Yield(3);
    ctx->MouseUp(ImGuiMouseButton_Left);
    ctx->Yield(3);
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
    // 经统一的安全封装浮动 Project（内部复位 ref 到根 + null 兜底，规避上游 UndockWindow 空指针崩溃）。
    UndockPanel(ctx, "//Project");
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
    RegisterEditorFeatureTests(engine);
    RegisterBlueprintTests(engine);
    Register2DToolsTests(engine);
    // 项目级基础操作（新建/打开/保存）放最后：会切换当前打开项目，避免影响前面的用例。
    RegisterProjectTests(engine);
    RegisterInspectorSectionTests(engine);
    RegisterToolPanelTests(engine);
    RegisterPanelDeepTests(engine);
#ifdef DSE_RENDER_TESTS
    RegisterRenderValidationTests(engine);
#endif
}

} // namespace dse::editor::uitest

#endif // DSE_EDITOR_UI_TESTS
