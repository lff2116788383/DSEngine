#include "editor_project_hub.h"
#include "editor_project.h"
#include "editor_settings.h"
#include "editor_console_panel.h"
#include "editor_icons.h"
#include "engine/dse_version.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <filesystem>

namespace dse::editor {

namespace {

static bool s_show_new_project = false;
static char s_new_name[128] = "MyGame";
static char s_new_location[512] = "";
static int  s_new_game_type = 0;   // GameType: Empty=0, Game2D=1, Game3D=2
static int  s_new_scripting = 0;   // ScriptingLanguage: None=0, Lua=1, CSharp=2, Cpp=3

// ─── Helper: accent-colored button ─────────────────────────────────────────
bool AccentButton(const char* label, const ImVec2& size) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.45f, 0.80f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.56f, 0.90f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.38f, 0.70f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return clicked;
}

// ─── Helper: secondary button (subtle) ─────────────────────────────────────
bool SecondaryButton(const char* label, const ImVec2& size) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.22f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.18f, 0.22f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return clicked;
}

// ─── Helper: labeled input field (label above) ─────────────────────────────
void LabeledInput(const char* label, const char* id, char* buf, size_t buf_size, float width) {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", label);
    ImGui::SetNextItemWidth(width);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
    ImGui::InputText(id, buf, buf_size);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

void DrawNewProjectPanel(float content_width) {
    ImGui::Spacing();

    const float field_width = content_width - 20.0f;

    LabeledInput("PROJECT NAME", "##proj_name", s_new_name, sizeof(s_new_name), field_width);
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "LOCATION");
    ImGui::SetNextItemWidth(field_width - 80.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
    ImGui::InputText("##proj_loc", s_new_location, sizeof(s_new_location));
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SameLine();
    if (SecondaryButton("Browse##hub", ImVec2(72, 0))) {
        std::string folder = BrowseNewProjectLocationDialog();
        if (!folder.empty()) {
            strncpy(s_new_location, folder.c_str(), sizeof(s_new_location) - 1);
            s_new_location[sizeof(s_new_location) - 1] = '\0';
        }
    }
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "GAME TYPE");
    ImGui::SetNextItemWidth(field_width);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
    const char* game_types[] = { "Empty", "2D Game", "3D Game" };
    ImGui::Combo("##proj_game_type", &s_new_game_type, game_types, 3);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "SCRIPTING");
    ImGui::SetNextItemWidth(field_width);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
    const char* scripting_opts[] = { "None", "Lua", "C#", "C++" };
    ImGui::Combo("##proj_scripting", &s_new_scripting, scripting_opts, 4);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::Spacing();

    bool can_create = (strlen(s_new_name) > 0 && strlen(s_new_location) > 0);
    if (!can_create) ImGui::BeginDisabled();
    if (AccentButton("  Create Project  ", ImVec2(content_width * 0.5f - 4, 36))) {
        auto& mgr = ProjectManager::Get();
        if (mgr.CreateProject(s_new_location, s_new_name,
                              static_cast<GameType>(s_new_game_type),
                              static_cast<ScriptingLanguage>(s_new_scripting))) {
            EditorSettings settings = LoadEditorSettings();
            settings.last_project_path = mgr.GetProjectRoot().string();
            AddRecentProject(settings, mgr.GetProjectRoot().string());
            SaveEditorSettings(settings);
            s_show_new_project = false;
        }
    }
    if (!can_create) ImGui::EndDisabled();

    ImGui::SameLine();
    if (SecondaryButton("  Back  ", ImVec2(content_width * 0.5f - 4, 36))) {
        s_show_new_project = false;
    }
}

} // namespace

bool DrawProjectHub() {
    auto& mgr = ProjectManager::Get();
    if (mgr.HasOpenProject()) {
        return true;
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
                           | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse
                           | ImGuiWindowFlags_NoBringToFrontOnFocus
                           | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollbar;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
    ImGui::Begin("##ProjectHub", nullptr, flags);
    ImGui::PopStyleColor();

    const float content_width = 480.0f;
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float avail_h = ImGui::GetContentRegionAvail().y;
    const float card_height = s_show_new_project ? 340.0f : 400.0f;
    const float offset_x = (avail_w - content_width) * 0.5f;
    const float offset_y = (avail_h - card_height) * 0.35f;
    if (offset_x > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);
    if (offset_y > 0) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offset_y);

    // ── Card background ────────────────────────────────────────────────────
    ImVec2 card_pos = ImGui::GetCursorScreenPos();
    ImDrawList* bg_dl = ImGui::GetWindowDrawList();
    bg_dl->AddRectFilled(card_pos,
                         ImVec2(card_pos.x + content_width, card_pos.y + card_height),
                         IM_COL32(22, 22, 28, 240), 10.0f);
    bg_dl->AddRect(card_pos,
                   ImVec2(card_pos.x + content_width, card_pos.y + card_height),
                   IM_COL32(60, 60, 75, 180), 10.0f);

    ImGui::SetCursorScreenPos(ImVec2(card_pos.x + 32, card_pos.y + 24));
    ImGui::BeginGroup();

    const float inner_w = content_width - 64;

    // ── Header ─────────────────────────────────────────────────────────────
    {
        ImFont* bold = (ImGui::GetIO().Fonts->Fonts.Size > 1) ? ImGui::GetIO().Fonts->Fonts[1] : nullptr;
        if (bold) ImGui::PushFont(bold);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.88f, 0.95f, 1.0f));
        ImGui::Text(MDI_ICON_CUBE_OUTLINE "  DSEngine");
        ImGui::PopStyleColor();
        if (bold) ImGui::PopFont();
    }
    ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.50f, 1.0f),
                       "Version %d.%d.%d", DSE_VERSION_MAJOR, DSE_VERSION_MINOR, DSE_VERSION_PATCH);
    ImGui::Spacing();

    // Subtle divider
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        bg_dl->AddLine(ImVec2(p.x, p.y), ImVec2(p.x + inner_w, p.y), IM_COL32(55, 55, 70, 200));
        ImGui::Dummy(ImVec2(0, 4));
    }

    if (s_show_new_project) {
        // ── Create New Project form ────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.78f, 0.85f, 1.0f));
        ImGui::Text("Create New Project");
        ImGui::PopStyleColor();
        DrawNewProjectPanel(inner_w);
    } else {
        // ── Action buttons ─────────────────────────────────────────────────
        ImGui::Spacing();
        if (AccentButton(MDI_ICON_PLUS "   New Project...", ImVec2(inner_w, 38))) {
            s_show_new_project = true;
        }
        ImGui::Spacing();
        if (SecondaryButton(MDI_ICON_FOLDER_OPEN "   Open Project...", ImVec2(inner_w, 38))) {
            std::string path = OpenProjectFileDialog();
            if (!path.empty()) {
                if (mgr.OpenProject(path)) {
                    EditorSettings settings = LoadEditorSettings();
                    settings.last_project_path = mgr.GetProjectRoot().string();
                    AddRecentProject(settings, mgr.GetProjectRoot().string());
                    SaveEditorSettings(settings);
                }
            }
        }

        ImGui::Spacing();
        ImGui::Spacing();

        // ── Recent Projects ────────────────────────────────────────────────
        ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.60f, 1.0f), "RECENT PROJECTS");
        {
            ImVec2 p = ImGui::GetCursorScreenPos();
            bg_dl->AddLine(ImVec2(p.x, p.y), ImVec2(p.x + inner_w, p.y), IM_COL32(55, 55, 70, 150));
            ImGui::Dummy(ImVec2(0, 4));
        }

        EditorSettings settings = LoadEditorSettings();
        if (settings.recent_projects.empty()) {
            ImGui::TextDisabled("  No recent projects");
        } else {
            for (size_t i = 0; i < settings.recent_projects.size(); ++i) {
                const auto& proj_path = settings.recent_projects[i];
                std::filesystem::path root(proj_path);
                std::string display = root.filename().string();
                std::string subtitle = proj_path;

                ImGui::PushID(static_cast<int>(i));

                // Hover highlight
                ImVec2 sel_pos = ImGui::GetCursorScreenPos();
                if (ImGui::Selectable("##proj", false, 0, ImVec2(inner_w, 48))) {
                    std::filesystem::path dseproj = root / "project.dseproj";
                    if (mgr.OpenProject(dseproj)) {
                        EditorSettings s = LoadEditorSettings();
                        s.last_project_path = proj_path;
                        AddRecentProject(s, proj_path);
                        SaveEditorSettings(s);
                    }
                }
                bool hovered = ImGui::IsItemHovered();

                ImVec2 item_min = ImGui::GetItemRectMin();
                ImVec2 item_max = ImGui::GetItemRectMax();

                if (hovered) {
                    bg_dl->AddRectFilled(item_min, item_max, IM_COL32(45, 50, 70, 200), 4.0f);
                }

                // Folder icon
                bg_dl->AddText(ImVec2(item_min.x + 10, item_min.y + 8),
                               IM_COL32(100, 140, 220, 255), MDI_ICON_FOLDER);

                // Project name (bold if possible)
                bg_dl->AddText(ImVec2(item_min.x + 34, item_min.y + 6),
                               IM_COL32(225, 225, 235, 255), display.c_str());

                // Path (dimmed, smaller)
                bg_dl->AddText(ImVec2(item_min.x + 34, item_min.y + 26),
                               IM_COL32(100, 100, 110, 255), subtitle.c_str());

                ImGui::PopID();
            }
        }
    }

    ImGui::EndGroup();
    ImGui::End();

    return mgr.HasOpenProject();
}

} // namespace dse::editor
