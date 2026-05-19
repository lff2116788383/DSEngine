#include "editor_preferences_panel.h"

#include "imgui.h"
#include "editor_settings.h"
#include "editor_theme.h"
#include "editor_icons.h"
#include "editor_locale.h"

namespace dse::editor {

namespace {
    int s_theme_index = 0; // 0=Dark, 1=Light
    bool s_show_grid = true;
    float s_grid_size = 1.0f;
    int s_grid_lines = 50;
    float s_snap_translate = 0.5f;
    float s_snap_rotate = 15.0f;
    float s_snap_scale = 0.1f;
    bool s_initialized = false;
    bool s_dirty = false;
    float s_save_timer = 0.0f;
    constexpr float kSaveDelaySec = 0.5f;

    void MarkPreferencesDirty() {
        s_dirty = true;
        s_save_timer = kSaveDelaySec;
    }

    void FlushPreferencesIfNeeded(float dt) {
        if (!s_dirty) return;
        s_save_timer -= dt;
        if (s_save_timer > 0.0f) return;
        s_dirty = false;

        EditorSettings settings = LoadEditorSettings();
        settings.theme_index = s_theme_index;
        settings.show_grid = s_show_grid;
        settings.grid_size = s_grid_size;
        settings.grid_lines = s_grid_lines;
        settings.snap_translate = s_snap_translate;
        settings.snap_rotate = s_snap_rotate;
        settings.snap_scale = s_snap_scale;
        SaveEditorSettings(settings);
    }
} // namespace

bool GetShowGrid() { return s_show_grid; }
void SetShowGrid(bool v) { s_show_grid = v; s_dirty = true; }
float GetGridSize() { return s_grid_size; }
int GetGridLines() { return s_grid_lines; }
float GetSnapTranslate() { return s_snap_translate; }
float GetSnapRotate() { return s_snap_rotate; }
float GetSnapScale() { return s_snap_scale; }

void InitPreferencesFromSettings() {
    if (s_initialized) return;
    s_initialized = true;

    EditorSettings settings = LoadEditorSettings();
    s_theme_index = settings.theme_index;
    s_show_grid = settings.show_grid;
    s_grid_size = settings.grid_size;
    s_grid_lines = settings.grid_lines;
    s_snap_translate = settings.snap_translate;
    s_snap_rotate = settings.snap_rotate;
    s_snap_scale = settings.snap_scale;

    if (s_theme_index == 0) {
        ImGui::StyleColorsDark();
        SetupEditorStyle();
    } else {
        SetupEditorStyleLight();
    }
}

int GetCurrentThemeIndex() { return s_theme_index; }

void ToggleEditorTheme() {
    s_theme_index = 1 - s_theme_index;
    if (s_theme_index == 0) {
        ImGui::StyleColorsDark();
        SetupEditorStyle();
    } else {
        SetupEditorStyleLight();
    }
    MarkPreferencesDirty();
}

void DrawPreferencesPanel(bool* p_open) {
    if (!p_open || !*p_open) {
        if (s_dirty) FlushPreferencesIfNeeded(kSaveDelaySec + 1.0f);
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(450, 400), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(T("Preferences"), p_open)) {
        ImGui::End();
        return;
    }

    bool changed = false;

    {
        char hdr_app[64]; snprintf(hdr_app, sizeof(hdr_app), MDI_ICON_PALETTE "  %s", T("Appearance"));
    if (ImGui::CollapsingHeader(hdr_app, ImGuiTreeNodeFlags_DefaultOpen)) {
        const char* themes[] = { T("Dark (Default)"), T("Light") };
        if (ImGui::Combo(T("Theme"), &s_theme_index, themes, 2)) {
            if (s_theme_index == 0) {
                ImGui::StyleColorsDark();
                SetupEditorStyle();
            } else {
                SetupEditorStyleLight();
            }
            changed = true;
        }

        ImGui::Separator();
        ImGui::TextUnformatted(T("Editor Language"));
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", T("Takes effect after restart"));
        static int s_locale_idx = -1;
        if (s_locale_idx < 0) {
            const auto& cur = GetEditorLocale();
            s_locale_idx = (cur == "zh-CN") ? 1 : 0;
        }
        const char* locales[] = { T("English"), T("Chinese Simplified") };
        if (ImGui::Combo("##EditorLocale", &s_locale_idx, locales, 2)) {
            EditorSettings ls = LoadEditorSettings();
            ls.editor_ui_locale = (s_locale_idx == 1) ? "zh-CN" : "en";
            SaveEditorSettings(ls);
        }

        ImGui::Separator();
        if (ImGui::Checkbox(T("Show Grid"), &s_show_grid)) {
            changed = true;
        }
        if (s_show_grid) {
            if (ImGui::DragFloat(T("Grid Size"), &s_grid_size, 0.1f, 0.1f, 100.0f, "%.1f")) changed = true;
            if (ImGui::DragInt(T("Grid Lines"), &s_grid_lines, 1, 5, 200)) changed = true;
        }
    }

    }
    {
        char hdr_snap[64]; snprintf(hdr_snap, sizeof(hdr_snap), MDI_ICON_COG "  %s", T("Snapping"));
    if (ImGui::CollapsingHeader(hdr_snap, ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::DragFloat(T("Translate Snap"), &s_snap_translate, 0.1f, 0.1f, 100.0f, "%.1f")) changed = true;
        if (ImGui::DragFloat(T("Rotate Snap (deg)"), &s_snap_rotate, 1.0f, 1.0f, 180.0f, "%.0f")) changed = true;
        if (ImGui::DragFloat(T("Scale Snap"), &s_snap_scale, 0.01f, 0.01f, 10.0f, "%.2f")) changed = true;
    }

    }
    {
        char hdr_as[64]; snprintf(hdr_as, sizeof(hdr_as), MDI_ICON_CONTENT_SAVE "  %s", T("Auto Save"));
    if (ImGui::CollapsingHeader(hdr_as, ImGuiTreeNodeFlags_DefaultOpen)) {
        EditorSettings as = LoadEditorSettings();
        bool as_changed = false;
        if (ImGui::Checkbox(T("Enable Auto Save"), &as.auto_save_enabled)) as_changed = true;
        if (as.auto_save_enabled) {
            int interval = as.auto_save_interval_sec;
            if (ImGui::SliderInt(T("Interval (sec)"), &interval, 30, 600)) {
                as.auto_save_interval_sec = interval;
                as_changed = true;
            }
        }
        if (as_changed) {
            SaveEditorSettings(as);
        }
    }

    }
    {
        char hdr_kb[80]; snprintf(hdr_kb, sizeof(hdr_kb), MDI_ICON_COG "  %s", T("Keyboard Shortcuts (Read-only)"));
    if (ImGui::CollapsingHeader(hdr_kb, ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Global shortcuts:");
        ImGui::BulletText("Ctrl+Z - Undo");
        ImGui::BulletText("Ctrl+Y / Ctrl+Shift+Z - Redo");
        ImGui::BulletText("Ctrl+S - Save Scene");
        ImGui::BulletText("Ctrl+Shift+S - Save Scene As");
        ImGui::BulletText("Ctrl+O - Open Scene");
        ImGui::BulletText("Ctrl+N - New Scene");
        ImGui::BulletText("Delete - Delete Selected Entity");
        ImGui::BulletText("Ctrl+C - Copy Entity");
        ImGui::BulletText("Ctrl+X - Cut Entity");
        ImGui::BulletText("Ctrl+V - Paste Entity");
        ImGui::BulletText("Ctrl+D - Duplicate Selected Entity");
        ImGui::BulletText("F2 - Rename Selected Entity");
        ImGui::Separator();
        ImGui::TextDisabled("Gizmo shortcuts:");
        ImGui::BulletText("W - Translate");
        ImGui::BulletText("E - Rotate");
        ImGui::BulletText("R - Scale");
        ImGui::BulletText("Q - Hand (No Gizmo)");
        ImGui::Separator();
        ImGui::TextDisabled("Viewport shortcuts:");
        ImGui::BulletText("F - Focus Selected Entity");
    }
    } // keyboard shortcuts block

    if (changed) {
        MarkPreferencesDirty();
    }

    float dt = ImGui::GetIO().DeltaTime;
    FlushPreferencesIfNeeded(dt);

    ImGui::End();
}

} // namespace dse::editor
