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
#include "editor_asset_importer.h"
#include "editor_icons.h"
#include "editor_selection.h"
#include "editor_project.h"
#include "editor_preferences_panel.h"
#include "editor_autosave.h"
#include "editor_layout_manager.h"
#include "engine/dse_version.h"
#include "editor_locale.h"

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

    auto& tab_mgr  = SceneTabManager::Get();
    auto& proj_mgr  = ProjectManager::Get();
    auto& undo_mgr  = GetUndoRedoManager();
    bool has_sel     = (ctx.selected_entity != entt::null && ctx.registry.valid(ctx.selected_entity));
    bool editable    = !ctx.read_only;
    bool has_project = proj_mgr.HasOpenProject();

    // ─── File ────────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu(T("File"))) {
        ImGui::TextDisabled("Scene");
        if (ImGui::MenuItem(MDI_ICON_PLUS "  New Scene", "Ctrl+N", false, editable)) {
            tab_mgr.NewScene(ctx.registry);
            ctx.selected_entity = entt::null;
        }
        if (ImGui::MenuItem(MDI_ICON_FOLDER_OPEN "  Open Scene...", "Ctrl+O", false, editable)) {
            std::string path = OpenSceneFileDialog();
            if (!path.empty()) {
                tab_mgr.OpenScene(ctx.registry, path);
                ctx.selected_entity = entt::null;
                EditorSettings settings = LoadEditorSettings();
                AddRecentFile(settings, path);
                SaveEditorSettings(settings);
            }
        }
        if (ImGui::BeginMenu(T("Recent Scenes"), editable)) {
            EditorSettings settings = LoadEditorSettings();
            if (settings.recent_files.empty()) {
                ImGui::TextDisabled("(empty)");
            } else {
                for (const auto& recent : settings.recent_files) {
                    std::filesystem::path p(recent);
                    if (ImGui::MenuItem(p.filename().string().c_str())) {
                        tab_mgr.OpenScene(ctx.registry, recent);
                        ctx.selected_entity = entt::null;
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", recent.c_str());
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem(MDI_ICON_CONTENT_SAVE "  Save Scene", "Ctrl+S", false, editable)) {
            const std::string& current_path = tab_mgr.GetActiveFilePath();
            if (current_path.empty()) {
                std::string path = SaveSceneFileDialog();
                if (!path.empty()) {
                    SaveScene(ctx.registry, path);
                    tab_mgr.SetCurrentPath(path);
                    tab_mgr.MarkClean();
                    AutoSaveManager::Get().OnManualSave();
                    EditorLog(LogLevel::Info, "Scene saved: " + path);
                }
            } else {
                SaveScene(ctx.registry, current_path);
                tab_mgr.MarkClean();
                AutoSaveManager::Get().OnManualSave();
                EditorLog(LogLevel::Info, "Scene saved: " + current_path);
            }
        }
        if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S", false, editable)) {
            std::string path = SaveSceneFileDialog();
            if (!path.empty()) {
                SaveScene(ctx.registry, path);
                tab_mgr.SetCurrentPath(path);
                tab_mgr.MarkClean();
                AutoSaveManager::Get().OnManualSave();
                EditorLog(LogLevel::Info, "Scene saved: " + path);
            }
        }
        ImGui::Separator();
        ImGui::TextDisabled("Project");
        if (ImGui::MenuItem(MDI_ICON_PLUS "  New Project...", nullptr, false, editable)) {
            ImGui::OpenPopup("NewProjectPopup");
        }
        if (ImGui::MenuItem(MDI_ICON_FOLDER_OPEN "  Open Project...", nullptr, false, editable)) {
            std::string path = OpenProjectFileDialog();
            if (!path.empty()) {
                if (proj_mgr.OpenProject(path)) {
                    EditorSettings settings = LoadEditorSettings();
                    settings.last_project_path = proj_mgr.GetProjectRoot().string();
                    AddRecentProject(settings, proj_mgr.GetProjectRoot().string());
                    SaveEditorSettings(settings);
                }
            }
        }
        if (ImGui::BeginMenu(T("Recent Projects"))) {
            EditorSettings settings = LoadEditorSettings();
            if (settings.recent_projects.empty()) {
                ImGui::TextDisabled("(empty)");
            } else {
                for (const auto& recent : settings.recent_projects) {
                    std::filesystem::path root(recent);
                    std::filesystem::path proj_path = root / "project.dseproj";
                    if (ImGui::MenuItem(root.filename().string().c_str())) {
                        if (proj_mgr.OpenProject(proj_path)) {
                            EditorSettings s = LoadEditorSettings();
                            s.last_project_path = recent;
                            AddRecentProject(s, recent);
                            SaveEditorSettings(s);
                        }
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", recent.c_str());
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Clear Recent Projects")) {
                    EditorSettings s = LoadEditorSettings();
                    s.recent_projects.clear();
                    SaveEditorSettings(s);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(MDI_ICON_CONTENT_SAVE "  Save Project", nullptr, false, has_project)) {
            proj_mgr.SaveProject();
        }
        if (ImGui::MenuItem("Close Project", nullptr, false, has_project)) {
            proj_mgr.CloseProject();
            EditorSettings settings = LoadEditorSettings();
            settings.last_project_path.clear();
            SaveEditorSettings(settings);
        }
        ImGui::Separator();
        ImGui::TextDisabled("Build");
        if (ImGui::MenuItem(MDI_ICON_EXPORT "  Build Game...", nullptr, false, editable && has_project)) {
            OpenBuildGameDialog();
        }
        if (ImGui::MenuItem("Import Asset...", nullptr, false, editable)) {
            OpenAssetImporter();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) {
            std::exit(0);
        }
        ImGui::EndMenu();
    }

    // ─── Edit ────────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu(T("Edit"))) {
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
        if (ImGui::MenuItem("Cut", "Ctrl+X", false, has_sel && editable)) {
            CutSelectedEntity(ctx);
        }
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, has_sel && editable)) {
            CopySelectedEntity(ctx);
        }
        if (ImGui::MenuItem("Paste", "Ctrl+V", false, HasEntityClipboard() && editable)) {
            PasteEntity(ctx);
        }
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, has_sel && editable)) {
            DuplicateSelectedEntity(ctx);
        }
        if (ImGui::MenuItem("Delete", "Del", false, has_sel && editable)) {
            DeleteSelectedEntity(ctx);
            SelectionManager::Get().Clear();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Select All", "Ctrl+A", false, editable)) {
            auto& sel = SelectionManager::Get();
            sel.Clear();
            auto view = ctx.registry.view<EditorNameComponent>();
            for (auto e : view) sel.Add(e);
            if (!sel.IsEmpty()) ctx.selected_entity = sel.GetPrimary();
        }
        if (ImGui::MenuItem("Deselect All", nullptr, false, editable)) {
            SelectionManager::Get().Clear();
            ctx.selected_entity = entt::null;
        }
        ImGui::Separator();
        if (show_preferences && ImGui::MenuItem(MDI_ICON_COG "  Preferences...")) {
            *show_preferences = true;
        }
        ImGui::EndMenu();
    }

    // ─── Entity ──────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu(T("Entity"))) {
        if (ImGui::MenuItem(MDI_ICON_PLUS "  Create Empty", nullptr, false, editable)) {
            CreateEmptyEntity(ctx);
        }
        ImGui::Separator();
        if (ImGui::BeginMenu(MDI_ICON_CUBE_OUTLINE "  3D Object", editable)) {
            if (ImGui::MenuItem("Cube"))     CreateEntity3DCube(ctx);
            if (ImGui::MenuItem("Sphere"))   CreateEntity3DSphere(ctx);
            if (ImGui::MenuItem("Plane"))    CreateEntity3DPlane(ctx);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(MDI_ICON_IMAGE "  2D Object", editable)) {
            if (ImGui::MenuItem("Sprite"))   CreateEntity2DSprite(ctx);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(MDI_ICON_LIGHTBULB "  Light", editable)) {
            if (ImGui::MenuItem("Directional Light")) CreateEntity3DDirectionalLight(ctx);
            if (ImGui::MenuItem("Point Light"))       CreateEntity3DPointLight(ctx);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(MDI_ICON_CAMERA "  Camera", nullptr, false, editable)) {
            CreateEntity3DCamera(ctx);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, has_sel && editable)) {
            DuplicateSelectedEntity(ctx);
        }
        if (ImGui::MenuItem("Delete", "Del", false, has_sel && editable)) {
            DeleteSelectedEntity(ctx);
            SelectionManager::Get().Clear();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Focus Selected", "F", false, has_sel)) {
            // Handled via ProcessShortcuts
        }
        ImGui::EndMenu();
    }

    // ─── View ────────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu(T("View"))) {
        bool grid_on = GetShowGrid();
        if (ImGui::MenuItem("Show Grid", nullptr, grid_on)) {
            SetShowGrid(!grid_on);
        }
        ImGui::Separator();
        ImGui::TextDisabled("Snap Settings");
        ImGui::Text("  Translate: %.1f", GetSnapTranslate());
        ImGui::Text("  Rotate:    %.0f" "\xc2\xb0", GetSnapRotate());
        ImGui::Text("  Scale:     %.2f", GetSnapScale());
        ImGui::Separator();
        if (ImGui::MenuItem("Gizmo: Translate", "W")) ctx.current_gizmo_operation = 0;
        if (ImGui::MenuItem("Gizmo: Rotate",    "E")) ctx.current_gizmo_operation = 1;
        if (ImGui::MenuItem("Gizmo: Scale",     "R")) ctx.current_gizmo_operation = 2;
        ImGui::Separator();
        if (ImGui::MenuItem("Local Space", nullptr, ctx.current_gizmo_mode == 0)) ctx.current_gizmo_mode = 0;
        if (ImGui::MenuItem("World Space", nullptr, ctx.current_gizmo_mode == 1)) ctx.current_gizmo_mode = 1;
        ImGui::EndMenu();
    }

    // ─── Window ──────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu(T("Window"))) {
        // Core panels (always present as dock windows)
        ImGui::TextDisabled("Panels");
        if (panels) {
            if (panels->profiler)
                ImGui::MenuItem(MDI_ICON_COG "  Profiler", nullptr, panels->profiler);
            if (panels->animation)
                ImGui::MenuItem(MDI_ICON_ANIMATION "  Animation", nullptr, panels->animation);
            if (panels->tile_palette)
                ImGui::MenuItem("Tile Palette", nullptr, panels->tile_palette);
            if (panels->terrain_editor)
                ImGui::MenuItem(MDI_ICON_TERRAIN "  Terrain Editor", nullptr, panels->terrain_editor);
            if (panels->lua_console)
                ImGui::MenuItem(MDI_ICON_CODE "  Lua Console", nullptr, panels->lua_console);
            if (panels->localization_preview)
                ImGui::MenuItem("Localization Preview", nullptr, panels->localization_preview);
            if (panels->undo_history)
                ImGui::MenuItem("Undo History", nullptr, panels->undo_history);

            ImGui::Separator();
            ImGui::TextDisabled("Advanced");
            if (panels->asset_browser)
                ImGui::MenuItem(MDI_ICON_FOLDER "  Asset Browser", nullptr, panels->asset_browser);
            if (panels->animation_timeline)
                ImGui::MenuItem(MDI_ICON_ANIMATION "  Animation Timeline", nullptr, panels->animation_timeline);
            if (panels->navmesh)
                ImGui::MenuItem(MDI_ICON_MAP_MARKER_PATH "  NavMesh", nullptr, panels->navmesh);
            if (panels->shader_graph)
                ImGui::MenuItem(MDI_ICON_PALETTE "  Shader Graph", nullptr, panels->shader_graph);
            if (panels->git)
                ImGui::MenuItem(MDI_ICON_SOURCE_BRANCH "  Git", nullptr, panels->git);
            if (panels->multi_viewport)
                ImGui::MenuItem(MDI_ICON_VIEW_MODULE "  Multi-Viewport", nullptr, panels->multi_viewport);
            if (panels->anim_state_machine)
                ImGui::MenuItem(MDI_ICON_ANIMATION "  Anim State Machine", nullptr, panels->anim_state_machine);
        }
        ImGui::Separator();
        if (show_chat && ImGui::MenuItem("AI Chat")) {
            *show_chat = true;
        }
        if (show_plugins && ImGui::MenuItem(MDI_ICON_PUZZLE "  Plugins...")) {
            *show_plugins = true;
        }
        ImGui::Separator();
        DrawLayoutMenu();
        if (ImGui::MenuItem(T("Reset Layout"))) {
            ResetEditorLayout();
        }
        ImGui::EndMenu();
    }

    // ─── Build ───────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu("Build")) {
        if (ImGui::MenuItem(MDI_ICON_EXPORT "  Export Windows Build...", "Ctrl+B")) {
            dse::editor::OpenBuildGameDialog();
        }
        ImGui::EndMenu();
    }

    // ─── Help ────────────────────────────────────────────────────────────────
    if (ImGui::BeginMenu(T("Help"))) {
        if (ImGui::MenuItem("About DSEngine")) {
            ImGui::OpenPopup("AboutDSEngine");
        }
        ImGui::Separator();
#if defined(_WIN32)
        if (ImGui::MenuItem(MDI_ICON_MAGNIFY "  Documentation")) {
            ShellExecuteA(nullptr, "open", "https://github.com/lff2116788383/DSEngine", nullptr, nullptr, SW_SHOWNORMAL);
        }
        if (ImGui::MenuItem("Report Issue")) {
            ShellExecuteA(nullptr, "open", "https://github.com/lff2116788383/DSEngine/issues", nullptr, nullptr, SW_SHOWNORMAL);
        }
#endif
        ImGui::Separator();
        ImGui::TextDisabled("Keyboard Shortcuts");
        ImGui::BulletText("Ctrl+N   New Scene");
        ImGui::BulletText("Ctrl+O   Open Scene");
        ImGui::BulletText("Ctrl+S   Save");
        ImGui::BulletText("Ctrl+Z/Y Undo / Redo");
        ImGui::BulletText("Ctrl+D   Duplicate");
        ImGui::BulletText("W/E/R    Translate / Rotate / Scale");
        ImGui::BulletText("F        Focus Selected");
        ImGui::BulletText("Del      Delete");
        ImGui::EndMenu();
    }

    // ── About popup ──────────────────────────────────────────────────────────
    if (ImGui::BeginPopupModal("AboutDSEngine", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.Size > 1 ? ImGui::GetIO().Fonts->Fonts[1] : nullptr);
        ImGui::Text("DSEngine Editor");
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::Text("Version %d.%d.%d", DSE_VERSION_MAJOR, DSE_VERSION_MINOR, DSE_VERSION_PATCH);
        ImGui::Spacing();
        ImGui::Text("Rendering: OpenGL / Vulkan / Direct3D 11");
        ImGui::Text("ECS: entt  |  UI: Dear ImGui  |  Physics: PhysX 4.1");
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(c) 2024-2026 DSEngine Contributors");
        ImGui::Separator();
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // ── New Project popup ────────────────────────────────────────────────────
    {
        static char s_new_proj_name[128] = "MyGame";
        static char s_new_proj_location[512] = "";
        static int s_new_proj_template = 0;

        if (ImGui::BeginPopupModal("NewProjectPopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Create New Project");
            ImGui::Separator();

            ImGui::SetNextItemWidth(300);
            ImGui::InputText("Project Name", s_new_proj_name, sizeof(s_new_proj_name));

            ImGui::SetNextItemWidth(300);
            ImGui::InputText("Location", s_new_proj_location, sizeof(s_new_proj_location));
            ImGui::SameLine();
            if (ImGui::Button("Browse...")) {
                std::string folder = BrowseNewProjectLocationDialog();
                if (!folder.empty()) {
                    strncpy(s_new_proj_location, folder.c_str(), sizeof(s_new_proj_location) - 1);
                    s_new_proj_location[sizeof(s_new_proj_location) - 1] = '\0';
                }
            }

            ImGui::SetNextItemWidth(300);
            const char* template_names[] = { "Empty", "2D Game", "3D Game", "Lua Scripting" };
            ImGui::Combo("Template", &s_new_proj_template, template_names, 4);

            ImGui::Separator();

            bool can_create = (strlen(s_new_proj_name) > 0 && strlen(s_new_proj_location) > 0);
            if (!can_create) ImGui::BeginDisabled();
            if (ImGui::Button("Create", ImVec2(120, 0))) {
                if (proj_mgr.CreateProject(
                        s_new_proj_location,
                        s_new_proj_name,
                        static_cast<ProjectTemplate>(s_new_proj_template))) {
                    EditorSettings settings = LoadEditorSettings();
                    settings.last_project_path = proj_mgr.GetProjectRoot().string();
                    AddRecentProject(settings, proj_mgr.GetProjectRoot().string());
                    SaveEditorSettings(settings);
                    EditorLog(LogLevel::Info, "Created project: " + std::string(s_new_proj_name));
                }
                ImGui::CloseCurrentPopup();
            }
            if (!can_create) ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    ImGui::EndMenuBar();
}

void DrawSceneTabBar(EditorContext& ctx) {
    SceneTabManager::Get().DrawTabBar(ctx.registry);
}

} // namespace dse::editor
