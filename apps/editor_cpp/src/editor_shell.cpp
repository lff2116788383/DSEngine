#include "editor_shell.h"

#include <cstdlib>

#include "imgui.h"
#include "imgui_internal.h"

#include "editor_scene_io.h"

namespace dse::editor {
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

void DrawEditorMainMenu(EditorShellContext& context) {
    if (!ImGui::BeginMenuBar()) {
        return;
    }

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New Scene", nullptr, false, !context.read_only)) {
            context.registry.clear();
            context.selected_entity = entt::null;
        }
        if (ImGui::MenuItem("Open Scene", "Ctrl+O", false, !context.read_only)) {
            LoadScene(context.registry, "scene.json");
            context.selected_entity = entt::null;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Save", "Ctrl+S", false, !context.read_only)) {
            SaveScene(context.registry, "scene.json");
        }
        if (ImGui::MenuItem("Save As...", nullptr, false, !context.read_only)) {
            SaveScene(context.registry, "scene.json");
        }
        if (context.read_only) {
            ImGui::TextDisabled("Play 模式下已禁用场景文件读写。");
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit", "Alt+F4")) {
            std::exit(0);
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Window")) {
        ImGui::EndMenu();
    }
    ImGui::EndMenuBar();
}

} // namespace dse::editor
