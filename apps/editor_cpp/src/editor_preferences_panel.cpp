#include "editor_preferences_panel.h"

#include "imgui.h"
#include "editor_settings.h"
#include "editor_theme.h"
#include "editor_icons.h"

namespace dse::editor {

namespace {
    int s_theme_index = 0; // 0=Dark, 1=Light
    bool s_show_grid = true;
    float s_snap_translate = 0.5f;
    float s_snap_rotate = 15.0f;
    float s_snap_scale = 0.1f;
} // namespace

bool GetShowGrid() { return s_show_grid; }
float GetSnapTranslate() { return s_snap_translate; }
float GetSnapRotate() { return s_snap_rotate; }
float GetSnapScale() { return s_snap_scale; }

void DrawPreferencesPanel(bool* p_open) {
    if (!p_open || !*p_open) return;

    ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Preferences", p_open)) {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader(MDI_ICON_PALETTE "  Appearance", ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* themes[] = { "Dark (Default)", "Light" };
        if (ImGui::Combo("Theme", &s_theme_index, themes, 2)) {
            if (s_theme_index == 0) {
                ImGui::StyleColorsDark();
                SetupEditorStyle();
            } else {
                ImGui::StyleColorsLight();
            }
        }

        ImGui::Checkbox("Show Grid", &s_show_grid);
    }

    if (ImGui::CollapsingHeader(MDI_ICON_COG "  Snapping", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Translate Snap", &s_snap_translate, 0.1f, 0.1f, 100.0f, "%.1f");
        ImGui::DragFloat("Rotate Snap (deg)", &s_snap_rotate, 1.0f, 1.0f, 180.0f, "%.0f");
        ImGui::DragFloat("Scale Snap", &s_snap_scale, 0.01f, 0.01f, 10.0f, "%.2f");
    }

    if (ImGui::CollapsingHeader(MDI_ICON_COG "  Keyboard Shortcuts (Read-only)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Global shortcuts:");
        ImGui::BulletText("Ctrl+Z - Undo");
        ImGui::BulletText("Ctrl+Y / Ctrl+Shift+Z - Redo");
        ImGui::BulletText("Ctrl+S - Save Scene");
        ImGui::BulletText("Ctrl+Shift+S - Save Scene As");
        ImGui::BulletText("Ctrl+O - Open Scene");
        ImGui::BulletText("Ctrl+N - New Scene");
        ImGui::BulletText("Delete - Delete Selected Entity");
        ImGui::BulletText("Ctrl+D - Duplicate Selected Entity");
        ImGui::Separator();
        ImGui::TextDisabled("Gizmo shortcuts:");
        ImGui::BulletText("W - Translate");
        ImGui::BulletText("E - Rotate");
        ImGui::BulletText("R - Scale");
        ImGui::BulletText("Q - Hand (No Gizmo)");
    }

    ImGui::End();
}

} // namespace dse::editor
