#include "editor_scene_view_mode.h"
#include "imgui.h"
#include <glad/gl.h>

namespace dse::editor {

const char* SceneViewModeName(SceneViewMode mode) {
    switch (mode) {
        case SceneViewMode::Shaded:          return "Shaded";
        case SceneViewMode::Wireframe:       return "Wireframe";
        case SceneViewMode::ShadedWireframe: return "Shaded+Wire";
        case SceneViewMode::Unlit:           return "Unlit";
        case SceneViewMode::Normals:         return "Normals";
        case SceneViewMode::Depth:           return "Depth";
        case SceneViewMode::AO:             return "AO";
        case SceneViewMode::Overdraw:        return "Overdraw";
        default:                             return "Unknown";
    }
}

SceneViewMode& GetCurrentSceneViewMode() {
    static SceneViewMode mode = SceneViewMode::Shaded;
    return mode;
}

void DrawSceneViewModeOverlay(unsigned int scene_texture,
                               unsigned int depth_texture,
                               unsigned int ssao_texture,
                               unsigned int gbuffer_normal_texture) {
    SceneViewMode mode = GetCurrentSceneViewMode();
    if (mode == SceneViewMode::Shaded) return;

    ImVec2 panel_size = ImGui::GetContentRegionAvail();
    if (panel_size.x < 1.0f || panel_size.y < 1.0f) return;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 win_pos = ImGui::GetCursorScreenPos();

    switch (mode) {
        case SceneViewMode::Wireframe:
            // Full viewport tinted overlay to indicate wireframe mode
            dl->AddRectFilled(win_pos,
                ImVec2(win_pos.x + panel_size.x, win_pos.y + panel_size.y),
                IM_COL32(0, 0, 0, 200));
            dl->AddText(ImVec2(win_pos.x + 8, win_pos.y + 8),
                IM_COL32(0, 255, 0, 255), "Wireframe (GPU: glPolygonMode)");
            break;

        case SceneViewMode::ShadedWireframe:
            // Subtle grid-like overlay to indicate wireframe-on-shaded
            dl->AddText(ImVec2(win_pos.x + 8, win_pos.y + 8),
                IM_COL32(200, 255, 200, 200), "Shaded + Wireframe");
            break;

        case SceneViewMode::Unlit: {
            // Desaturate overlay to hint unlit
            dl->AddRectFilled(win_pos,
                ImVec2(win_pos.x + panel_size.x, win_pos.y + panel_size.y),
                IM_COL32(50, 50, 50, 80));
            dl->AddText(ImVec2(win_pos.x + 8, win_pos.y + 8),
                IM_COL32(255, 220, 100, 255), "Unlit (Albedo Only)");
            break;
        }

        case SceneViewMode::Normals:
            if (gbuffer_normal_texture != 0) {
                ImGui::Image((ImTextureID)(intptr_t)gbuffer_normal_texture, panel_size, ImVec2(0, 1), ImVec2(1, 0));
            } else {
                dl->AddText(ImVec2(win_pos.x + 8, win_pos.y + 8),
                    IM_COL32(128, 128, 255, 255), "Normals (requires GBuffer)");
            }
            break;

        case SceneViewMode::Depth:
            if (depth_texture != 0) {
                ImGui::Image((ImTextureID)(intptr_t)depth_texture, panel_size, ImVec2(0, 1), ImVec2(1, 0));
            } else {
                dl->AddText(ImVec2(win_pos.x + 8, win_pos.y + 8),
                    IM_COL32(200, 200, 200, 255), "Depth (requires depth texture)");
            }
            break;

        case SceneViewMode::AO:
            if (ssao_texture != 0) {
                ImGui::Image((ImTextureID)(intptr_t)ssao_texture, panel_size, ImVec2(0, 1), ImVec2(1, 0));
            } else {
                dl->AddText(ImVec2(win_pos.x + 8, win_pos.y + 8),
                    IM_COL32(200, 200, 200, 255), "AO (requires SSAO pass)");
            }
            break;

        case SceneViewMode::Overdraw:
            dl->AddRectFilled(win_pos,
                ImVec2(win_pos.x + panel_size.x, win_pos.y + panel_size.y),
                IM_COL32(0, 0, 0, 100));
            dl->AddText(ImVec2(win_pos.x + 8, win_pos.y + 8),
                IM_COL32(255, 100, 100, 255), "Overdraw (visualization placeholder)");
            break;

        default:
            break;
    }
}

void DrawSceneViewModeSelector() {
    SceneViewMode& mode = GetCurrentSceneViewMode();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
    ImGui::SetNextItemWidth(100.0f);

    if (ImGui::BeginCombo("##ViewMode", SceneViewModeName(mode), ImGuiComboFlags_NoArrowButton)) {
        for (int i = 0; i < static_cast<int>(SceneViewMode::Count); i++) {
            SceneViewMode m = static_cast<SceneViewMode>(i);
            bool selected = (mode == m);
            if (ImGui::Selectable(SceneViewModeName(m), selected)) {
                mode = m;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::PopStyleVar();
}

} // namespace dse::editor
