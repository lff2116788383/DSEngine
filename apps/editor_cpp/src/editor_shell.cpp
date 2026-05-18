#include "editor_shell.h"

#include <cstdlib>

#include "imgui.h"
#include "imgui_internal.h"

#include "editor_scene_io.h"
#include "editor_shortcuts.h"
#include "editor_file_dialog.h"
#include "editor_console_panel.h"
#include "editor_settings.h"
#include "editor_scene_tabs.h"
#include "editor_build_game.h"
#include "editor_icons.h"
#include "editor_selection.h"
#include "engine/dse_version.h"

#include <filesystem>

#if defined(_WIN32)
#include <Windows.h>
#include <shellapi.h>
#endif

namespace dse::editor {

namespace {
static std::string s_current_scene_path = "Untitled";
} // forward declare for use below

const std::string& GetCurrentScenePath() {
    return s_current_scene_path;
}

void SetCurrentScenePath(const std::string& path) {
    s_current_scene_path = path;
}

namespace {

static bool s_layout_dirty = true;

void BuildDefaultDockLayout(ImGuiID dockspace_id, const ImVec2& viewport_size) {
    if (!s_layout_dirty) {
        return;
    }
    s_layout_dirty = false;

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport_size);

    auto dock_id_main = dockspace_id;
    auto dock_id_top    = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Up,    0.05f, nullptr, &dock_id_main);
    auto dock_id_bottom = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down,  0.24f, nullptr, &dock_id_main);
    auto dock_id_left   = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left,  0.18f, nullptr, &dock_id_main);
    auto dock_id_right  = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.24f, nullptr, &dock_id_main);

    ImGui::DockBuilderDockWindow("Toolbar",              dock_id_top);
    ImGui::DockBuilderDockWindow("Hierarchy",             dock_id_left);
    ImGui::DockBuilderDockWindow("Inspector",             dock_id_right);
    ImGui::DockBuilderDockWindow("Material",              dock_id_right);
    ImGui::DockBuilderDockWindow("Project",               dock_id_bottom);
    ImGui::DockBuilderDockWindow("Console",               dock_id_bottom);
    ImGui::DockBuilderDockWindow("Scene",                 dock_id_main);
    ImGui::DockBuilderDockWindow("Game",                  dock_id_main);

    ImGuiDockNode* node = ImGui::DockBuilderGetNode(dock_id_top);
    if (node) {
        node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoResizeY;
    }

    ImGui::DockBuilderFinish(dockspace_id);
}

} // namespace

void ResetEditorLayout() {
    s_layout_dirty = true;
}

void BeginEditorShell() {
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace Demo", nullptr, window_flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
        const ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        BuildDefaultDockLayout(dockspace_id, viewport->Size);
    }
}

void EndEditorShell() {
    ImGui::End();
}

void DrawEditorMainMenu(EditorContext& ctx, bool* show_preferences, bool* show_plugins, bool* show_chat, const PanelVisibility* panels) {
    if (!ImGui::BeginMenuBar()) {
        return;
    }

    // ─── File ────────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu("File")) {
        auto& tab_mgr = SceneTabManager::Get();
        if (ImGui::MenuItem(MDI_ICON_PLUS "  New Scene", "Ctrl+N", false, !ctx.read_only)) {
            tab_mgr.NewScene(ctx.registry);
            ctx.selected_entity = entt::null;
        }
        if (ImGui::MenuItem(MDI_ICON_FOLDER_OPEN "  Open Scene...", "Ctrl+O", false, !ctx.read_only)) {
            std::string path = dse::editor::OpenSceneFileDialog();
            if (!path.empty()) {
                tab_mgr.OpenScene(ctx.registry, path);
                ctx.selected_entity = entt::null;
                EditorSettings settings = LoadEditorSettings();
                AddRecentFile(settings, path);
                SaveEditorSettings(settings);
            }
        }
        if (ImGui::BeginMenu("Recent Files", !ctx.read_only)) {
            EditorSettings settings = LoadEditorSettings();
            if (settings.recent_files.empty()) {
                ImGui::TextDisabled("No recent files");
            } else {
                for (const auto& recent : settings.recent_files) {
                    if (ImGui::MenuItem(recent.c_str())) {
                        tab_mgr.OpenScene(ctx.registry, recent);
                        ctx.selected_entity = entt::null;
                    }
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(MDI_ICON_CONTENT_SAVE "  Save", "Ctrl+S", false, !ctx.read_only)) {
            const std::string& current_path = tab_mgr.GetActiveFilePath();
            if (current_path.empty()) {
                std::string path = dse::editor::SaveSceneFileDialog();
                if (!path.empty()) {
                    SaveScene(ctx.registry, path);
                    tab_mgr.SetCurrentPath(path);
                    tab_mgr.MarkClean();
                    EditorLog(LogLevel::Info, "Scene saved: " + path);
                }
            } else {
                SaveScene(ctx.registry, current_path);
                tab_mgr.MarkClean();
                EditorLog(LogLevel::Info, "Scene saved: " + current_path);
            }
        }
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, !ctx.read_only)) {
            std::string path = dse::editor::SaveSceneFileDialog();
            if (!path.empty()) {
                SaveScene(ctx.registry, path);
                tab_mgr.SetCurrentPath(path);
                tab_mgr.MarkClean();
                EditorLog(LogLevel::Info, "Scene saved: " + path);
            }
        }
        if (ctx.read_only) {
            ImGui::TextDisabled("Play mode: file operations disabled.");
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) {
            std::exit(0);
        }
        ImGui::EndMenu();
    }

    // ─── Edit ───────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu("Edit")) {
        auto& undo_mgr = dse::editor::GetUndoRedoManager();
        std::string undo_label = "Undo";
        if (undo_mgr.CanUndo()) undo_label += " (" + undo_mgr.GetUndoDescription() + ")";
        if (ImGui::MenuItem(undo_label.c_str(), "Ctrl+Z", false, undo_mgr.CanUndo())) {
            undo_mgr.Undo();
        }
        std::string redo_label = "Redo";
        if (undo_mgr.CanRedo()) redo_label += " (" + undo_mgr.GetRedoDescription() + ")";
        if (ImGui::MenuItem(redo_label.c_str(), "Ctrl+Y", false, undo_mgr.CanRedo())) {
            undo_mgr.Redo();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Select All", "Ctrl+A", false, !ctx.read_only)) {
            auto& sel = SelectionManager::Get();
            sel.Clear();
            auto view = ctx.registry.view<EditorNameComponent>();
            for (auto e : view) sel.Add(e);
            if (!sel.IsEmpty()) ctx.selected_entity = sel.GetPrimary();
        }
        if (ImGui::MenuItem("Deselect All", nullptr, false, !ctx.read_only)) {
            SelectionManager::Get().Clear();
            ctx.selected_entity = entt::null;
        }
        ImGui::Separator();
        if (show_preferences && ImGui::MenuItem(MDI_ICON_COG "  Preferences...")) {
            *show_preferences = true;
        }
        ImGui::EndMenu();
    }

    // ─── Entity ─────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu("Entity")) {
        if (ImGui::MenuItem(MDI_ICON_PLUS "  Create Empty", nullptr, false, !ctx.read_only)) {
            CreateEmptyEntity(ctx);
        }
        ImGui::Separator();
        bool has_sel = (ctx.selected_entity != entt::null && ctx.registry.valid(ctx.selected_entity));
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, has_sel && !ctx.read_only)) {
            DuplicateSelectedEntity(ctx);
            EditorLog(LogLevel::Info, "Entity duplicated");
        }
        if (ImGui::MenuItem("Delete", "Del", false, has_sel && !ctx.read_only)) {
            DeleteSelectedEntity(ctx);
            SelectionManager::Get().Clear();
            EditorLog(LogLevel::Info, "Entity deleted");
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Focus Selected", "F", false, has_sel)) {
            // Handled by ProcessShortcuts — this is a visual reminder
        }
        ImGui::EndMenu();
    }

    // ─── Project ────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu("Project")) {
        if (ImGui::MenuItem(MDI_ICON_EXPORT "  Build Game...", nullptr, false, !ctx.read_only)) {
            OpenBuildGameDialog();
        }
        ImGui::Separator();
        if (show_plugins && ImGui::MenuItem(MDI_ICON_PUZZLE "  Plugins...")) {
            *show_plugins = true;
        }
        ImGui::EndMenu();
    }

    // ─── Window ─────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu("Window")) {
        if (ImGui::BeginMenu("Panels")) {
            if (panels) {
                if (panels->profiler)
                    ImGui::MenuItem("Profiler", nullptr, panels->profiler);
                if (panels->animation)
                    ImGui::MenuItem("Animation", nullptr, panels->animation);
                if (panels->tile_palette)
                    ImGui::MenuItem("Tile Palette", nullptr, panels->tile_palette);
                if (panels->terrain_editor)
                    ImGui::MenuItem("Terrain Editor", nullptr, panels->terrain_editor);
                if (panels->lua_console)
                    ImGui::MenuItem("Lua Console", nullptr, panels->lua_console);
                if (panels->localization_preview)
                    ImGui::MenuItem("Localization Preview", nullptr, panels->localization_preview);
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (show_chat && ImGui::MenuItem("AI Chat")) {
            *show_chat = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Reset Layout")) {
            ResetEditorLayout();
        }
        ImGui::EndMenu();
    }

    // ─── Help ───────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About DSEngine")) {
            ImGui::OpenPopup("AboutDSEngine");
        }
#if defined(_WIN32)
        if (ImGui::MenuItem("Documentation (GitHub)")) {
            ShellExecuteA(nullptr, "open", "https://github.com/lff2116788383/DSEngine", nullptr, nullptr, SW_SHOWNORMAL);
        }
#endif
        ImGui::EndMenu();
    }

    // About popup (must be at menu bar scope to avoid clipping)
    if (ImGui::BeginPopupModal("AboutDSEngine", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("DSEngine Editor");
        ImGui::Separator();
        ImGui::Text("Version: %d.%d.%d", DSE_VERSION_MAJOR, DSE_VERSION_MINOR, DSE_VERSION_PATCH);
        ImGui::Text("C++ Editor with ImGui, entt ECS, OpenGL/Vulkan/DX11");
        ImGui::Text("(c) 2024-2026 DSEngine Contributors");
        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::EndMenuBar();
}

void DrawSceneTabBar(EditorContext& ctx) {
    SceneTabManager::Get().DrawTabBar(ctx.registry);
}

} // namespace dse::editor
