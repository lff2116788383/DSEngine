#include "editor_status_bar.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "editor_icons.h"
#include "editor_locale.h"

namespace dse::editor {

void DrawStatusBar(EditorContext& context) {
    // Smooth FPS: update display value at most twice per second
    static float s_display_fps = 60.0f;
    static float s_fps_timer = 0.0f;
    {
        const float dt = ImGui::GetIO().DeltaTime;
        s_fps_timer += dt;
        if (s_fps_timer >= 0.5f) {
            s_fps_timer = 0.0f;
            const auto& frame = context.cpu_profiler.GetFrameStats();
            s_display_fps = frame.fps;
        }
    }

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

    // FPS (fixed width to prevent layout jitter)
    ImGui::Text("FPS: %3.0f", s_display_fps);
    ImGui::SameLine(0, 12);
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine(0, 12);

    // Entity count
    const int entity_count = static_cast<int>(context.registry.storage<entt::entity>().in_use());
    ImGui::Text("%s %d", T("Entities:"), entity_count);
    ImGui::SameLine(0, 12);
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine(0, 12);

    // Draw calls
    const auto& render_frame = context.render_profiler.GetCurrentFrameStats();
    ImGui::Text("%s %d", T("Draws:"), render_frame.draw_calls);
    ImGui::SameLine(0, 12);
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine(0, 12);

    // Gizmo tool
    const char* tool_name = T("Hand");
    switch (context.current_gizmo_operation) {
        case 0: tool_name = T("Translate"); break;
        case 1: tool_name = T("Rotate"); break;
        case 2: tool_name = T("Scale"); break;
        default: tool_name = T("Hand"); break;
    }
    ImGui::Text("Tool: %s", tool_name);
    ImGui::SameLine(0, 12);
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine(0, 12);

    // Coordinate space
    ImGui::Text("%s", context.current_gizmo_mode == 0 ? T("Local") : T("World"));

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}

} // namespace dse::editor
