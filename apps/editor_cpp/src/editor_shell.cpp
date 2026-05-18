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

void BuildDefaultDockLayout(ImGuiID dockspace_id, const ImVec2& viewport_size) {
    static bool first_time = true;
    if (!first_time) {
        return;
    }
    first_time = false;

    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, viewport_size);

    auto dock_id_main = dockspace_id;
    auto dock_id_top = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Up, 0.05f, nullptr, &dock_id_main);
    auto dock_id_bottom = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, 0.30f, nullptr, &dock_id_main);
    auto dock_id_left = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.20f, nullptr, &dock_id_main);
    auto dock_id_right = ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Right, 0.25f, nullptr, &dock_id_main);

    ImGui::DockBuilderDockWindow("Toolbar", dock_id_top);
    ImGui::DockBuilderDockWindow("Hierarchy", dock_id_left);
    ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
    ImGui::DockBuilderDockWindow("Material", dock_id_right);
    ImGui::DockBuilderDockWindow("Project", dock_id_bottom);
    ImGui::DockBuilderDockWindow("Console", dock_id_bottom);
    ImGui::DockBuilderDockWindow("Animation", dock_id_bottom);
    ImGui::DockBuilderDockWindow("Profiler", dock_id_bottom);
    ImGui::DockBuilderDockWindow("Tile Palette", dock_id_bottom);
    ImGui::DockBuilderDockWindow("Scene", dock_id_main);
    ImGui::DockBuilderDockWindow("Game", dock_id_main);

    ImGuiDockNode* node = ImGui::DockBuilderGetNode(dock_id_top);
    if (node) {
        node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoResizeY;
    }

    ImGui::DockBuilderFinish(dockspace_id);
}

} // namespace

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

void DrawEditorMainMenu(EditorContext& ctx, bool* show_preferences, bool* show_plugins) {
    if (!ImGui::BeginMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        auto& tab_mgr = SceneTabManager::Get();
        if (ImGui::MenuItem("New Scene", "Ctrl+N", false, !ctx.read_only)) {
            tab_mgr.NewScene(ctx.registry);
            ctx.selected_entity = entt::null;
        }
        if (ImGui::MenuItem("Open Scene", "Ctrl+O", false, !ctx.read_only)) {
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
        if (ImGui::MenuItem("Save", "Ctrl+S", false, !ctx.read_only)) {
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
            ImGui::TextDisabled("Play 模式下已禁用场景文件读写。");
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Build Game...", nullptr, false, !ctx.read_only)) {
            OpenBuildGameDialog();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) {
            std::exit(0);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        auto& undo_mgr = dse::editor::GetUndoRedoManager();
        std::string undo_label = "Undo";
        if (undo_mgr.CanUndo()) {
            undo_label += " (" + undo_mgr.GetUndoDescription() + ")";
        }
        if (ImGui::MenuItem(undo_label.c_str(), "Ctrl+Z", false, undo_mgr.CanUndo())) {
            undo_mgr.Undo();
        }
        std::string redo_label = "Redo";
        if (undo_mgr.CanRedo()) {
            redo_label += " (" + undo_mgr.GetRedoDescription() + ")";
        }
        if (ImGui::MenuItem(redo_label.c_str(), "Ctrl+Y", false, undo_mgr.CanRedo())) {
            undo_mgr.Redo();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window")) {
        if (show_preferences && ImGui::MenuItem("Preferences")) {
            *show_preferences = true;
        }
        if (show_plugins && ImGui::MenuItem("Plugins")) {
            *show_plugins = true;
        }
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}

void DrawSceneTabBar(EditorContext& ctx) {
    SceneTabManager::Get().DrawTabBar(ctx.registry);
}

} // namespace dse::editor
