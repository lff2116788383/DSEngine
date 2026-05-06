#include "editor_status_bar.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "editor_icons.h"

namespace dse::editor {

void DrawStatusBar(EditorStatusBarContext& context) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float status_bar_height = 24.0f;

    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - status_bar_height));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, status_bar_height));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 3));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                             ImGuiWindowFlags_NoNav;

    ImGui::Begin("##StatusBar", nullptr, flags);

    // FPS
    const auto& frame = context.cpu_profiler.GetFrameStats();
    ImGui::Text("FPS: %.0f", frame.fps);
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Entity count
    const int entity_count = static_cast<int>(context.registry.storage<entt::entity>().in_use());
    ImGui::Text("Entities: %d", entity_count);
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Draw calls
    const auto& render_frame = context.render_profiler.GetCurrentFrameStats();
    ImGui::Text("Draw Calls: %d", render_frame.draw_calls);
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Gizmo tool
    const char* tool_name = "Hand";
    switch (context.current_gizmo_operation) {
        case 0: tool_name = "Translate"; break;
        case 1: tool_name = "Rotate"; break;
        case 2: tool_name = "Scale"; break;
        default: tool_name = "Hand"; break;
    }
    ImGui::Text("Tool: %s", tool_name);
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Coordinate space
    ImGui::Text("%s", context.current_gizmo_mode == 0 ? "Local" : "World");

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

} // namespace dse::editor
