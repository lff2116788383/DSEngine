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
static int  s_new_template = 0;

void DrawNewProjectPanel() {
    ImGui::SetNextItemWidth(280);
    ImGui::InputText("Project Name", s_new_name, sizeof(s_new_name));

    ImGui::SetNextItemWidth(280);
    ImGui::InputText("Location", s_new_location, sizeof(s_new_location));
    ImGui::SameLine();
    if (ImGui::Button("Browse...##hub")) {
        std::string folder = BrowseNewProjectLocationDialog();
        if (!folder.empty()) {
            strncpy(s_new_location, folder.c_str(), sizeof(s_new_location) - 1);
            s_new_location[sizeof(s_new_location) - 1] = '\0';
        }
    }

    ImGui::SetNextItemWidth(280);
    const char* templates[] = { "Empty", "2D Game", "3D Game", "Lua Scripting" };
    ImGui::Combo("Template", &s_new_template, templates, 4);

    ImGui::Spacing();

    bool can_create = (strlen(s_new_name) > 0 && strlen(s_new_location) > 0);
    if (!can_create) ImGui::BeginDisabled();
    if (ImGui::Button("Create Project", ImVec2(160, 32))) {
        auto& mgr = ProjectManager::Get();
        if (mgr.CreateProject(s_new_location, s_new_name,
                              static_cast<ProjectTemplate>(s_new_template))) {
            EditorSettings settings = LoadEditorSettings();
            settings.last_project_path = mgr.GetProjectRoot().string();
            AddRecentProject(settings, mgr.GetProjectRoot().string());
            SaveEditorSettings(settings);
            s_show_new_project = false;
        }
    }
    if (!can_create) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Back", ImVec2(80, 32))) {
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

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
    ImGui::Begin("##ProjectHub", nullptr, flags);
    ImGui::PopStyleColor();

    // 居中内容区域
    const float content_width = 500.0f;
    const float avail_w = ImGui::GetContentRegionAvail().x;
    const float offset_x = (avail_w - content_width) * 0.5f;
    if (offset_x > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);

    ImGui::BeginGroup();

    // 标题
    ImGui::Spacing();
    ImGui::Spacing();
    {
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts.Size > 1 ? ImGui::GetIO().Fonts->Fonts[1] : nullptr);
        ImGui::Text("DSEngine");
        ImGui::PopFont();
    }
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                       "Version %d.%d.%d", DSE_VERSION_MAJOR, DSE_VERSION_MINOR, DSE_VERSION_PATCH);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (s_show_new_project) {
        ImGui::Text("Create New Project");
        ImGui::Spacing();
        DrawNewProjectPanel();
    } else {
        // 操作按钮
        if (ImGui::Button(MDI_ICON_PLUS "  New Project...", ImVec2(content_width, 36))) {
            s_show_new_project = true;
        }
        if (ImGui::Button(MDI_ICON_FOLDER_OPEN "  Open Project...", ImVec2(content_width, 36))) {
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

        // Recent Projects
        ImGui::Text("Recent Projects");
        ImGui::Separator();

        EditorSettings settings = LoadEditorSettings();
        if (settings.recent_projects.empty()) {
            ImGui::TextDisabled("No recent projects");
        } else {
            for (size_t i = 0; i < settings.recent_projects.size(); ++i) {
                const auto& proj_path = settings.recent_projects[i];
                std::filesystem::path root(proj_path);
                std::string display = root.filename().string();
                std::string subtitle = proj_path;

                ImGui::PushID(static_cast<int>(i));

                // 项目条目：名称 + 路径
                if (ImGui::Selectable("##proj", false, 0, ImVec2(content_width, 40))) {
                    std::filesystem::path dseproj = root / "project.dseproj";
                    if (mgr.OpenProject(dseproj)) {
                        EditorSettings s = LoadEditorSettings();
                        s.last_project_path = proj_path;
                        AddRecentProject(s, proj_path);
                        SaveEditorSettings(s);
                    }
                }
                // 在 Selectable 上叠绘文字
                ImVec2 item_min = ImGui::GetItemRectMin();
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddText(ImVec2(item_min.x + 8, item_min.y + 4),
                            IM_COL32(220, 220, 220, 255), display.c_str());
                dl->AddText(ImVec2(item_min.x + 8, item_min.y + 22),
                            IM_COL32(120, 120, 120, 255), subtitle.c_str());

                ImGui::PopID();
            }
        }
    }

    ImGui::EndGroup();
    ImGui::End();

    return mgr.HasOpenProject();
}

} // namespace dse::editor
