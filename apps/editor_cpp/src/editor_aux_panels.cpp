#include "editor_aux_panels.h"

#include "engine/ecs/components_2d.h"
#include "modules/gameplay_2d/localization/localization_system.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "editor_console_panel.h"
#include "editor_icons.h"

#include <fstream>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cctype>

#if defined(_WIN32)
#include <Windows.h>
#include <shellapi.h>
#endif

namespace {

std::filesystem::path GetProjectRootPath() {
    return std::filesystem::current_path().lexically_normal();
}

std::filesystem::path GetProjectBaseDataPath() {
    static std::filesystem::path base_data_path = []() {
        std::filesystem::path p = GetProjectRootPath();
        std::filesystem::path target_path = p / "samples" / "lua" / "data";
        if (!std::filesystem::exists(target_path)) {
            try {
                std::filesystem::create_directories(target_path);
            } catch (...) {
                return p;
            }
        }
        return target_path;
    }();
    return base_data_path;
}

std::filesystem::path& GetCurrentProjectPanelPath() {
    static std::filesystem::path current_path = GetProjectBaseDataPath();
    return current_path;
}

} // namespace

namespace dse::editor {

void DrawProjectPanel() {
    static char s_search_filter[128] = "";
    static bool s_grid_view = false;
    static std::filesystem::path s_rename_target;
    static char s_rename_buf[128] = "";

    ImGui::Begin("Project");

    const std::filesystem::path base_data_path = GetProjectBaseDataPath();
    std::filesystem::path& current_path = GetCurrentProjectPanelPath();

    // Toolbar: Search + View toggle
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80);
    ImGui::InputTextWithHint("##project_search", MDI_ICON_MAGNIFY " Search...", s_search_filter, sizeof(s_search_filter));
    ImGui::SameLine();
    if (ImGui::Button(s_grid_view ? "List" : "Grid", ImVec2(60, 0))) {
        s_grid_view = !s_grid_view;
    }
    ImGui::Separator();

    // Breadcrumb / Back navigation
    if (current_path != base_data_path) {
        if (ImGui::Button("<- Back")) {
            current_path = current_path.parent_path();
        }
        ImGui::SameLine();
        std::string rel = std::filesystem::relative(current_path, base_data_path).string();
        ImGui::TextDisabled("/ %s", rel.c_str());
        ImGui::Separator();
    }

    // Background context menu (create new assets)
    if (ImGui::BeginPopupContextWindow("ProjectContextMenu")) {
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Folder")) {
                std::filesystem::create_directory(current_path / "NewFolder");
            }
            if (ImGui::MenuItem("Lua Script")) {
                std::ofstream ofs(current_path / "NewScript.lua");
                if (ofs.is_open()) ofs << "-- New Lua Script\n";
            }
            if (ImGui::MenuItem("Material")) {
                std::ofstream ofs(current_path / "NewMaterial.mat");
                if (ofs.is_open()) ofs << "{\n  \"shader\": \"default\",\n  \"color\": [1,1,1,1]\n}\n";
            }
            ImGui::EndMenu();
        }
#if defined(_WIN32)
        if (ImGui::MenuItem("Show in Explorer")) {
            ShellExecuteW(nullptr, L"open", current_path.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
#endif
        ImGui::EndPopup();
    }

    if (!std::filesystem::exists(current_path)) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Data path not found: %s", current_path.string().c_str());
    } else {
        // Collect and filter entries
        std::vector<std::filesystem::directory_entry> entries;
        try {
            for (const auto& entry : std::filesystem::directory_iterator(current_path)) {
                const std::string filename = entry.path().filename().string();
                if (s_search_filter[0] != '\0') {
                    std::string lower_name = filename;
                    std::string lower_filter = s_search_filter;
                    for (auto& c : lower_name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    for (auto& c : lower_filter) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (lower_name.find(lower_filter) == std::string::npos) continue;
                }
                entries.push_back(entry);
            }
        } catch (const std::filesystem::filesystem_error&) {}

        // Sort: directories first, then alphabetical
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
            if (a.is_directory() != b.is_directory()) return a.is_directory();
            return a.path().filename().string() < b.path().filename().string();
        });

        if (s_grid_view) {
            // Grid view
            const float cell_size = 80.0f;
            const float panel_width = ImGui::GetContentRegionAvail().x;
            int columns = (std::max)(1, static_cast<int>(panel_width / cell_size));
            int col = 0;

            for (const auto& entry : entries) {
                const auto& path = entry.path();
                const std::string filename = path.filename().string();
                ImGui::PushID(filename.c_str());

                ImGui::BeginGroup();
                ImVec2 p = ImGui::GetCursorScreenPos();
                ImU32 bg_color = entry.is_directory() ? IM_COL32(60, 80, 120, 180) : IM_COL32(60, 60, 70, 180);
                ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + cell_size - 8, p.y + cell_size - 24), bg_color, 4.0f);
                ImGui::Dummy(ImVec2(cell_size - 8, cell_size - 24));

                // Truncate long filenames
                std::string display_name = filename.size() > 10 ? filename.substr(0, 9) + "..." : filename;
                ImGui::TextUnformatted(display_name.c_str());
                ImGui::EndGroup();

                if (ImGui::IsItemClicked() && ImGui::IsMouseDoubleClicked(0) && entry.is_directory()) {
                    current_path /= path.filename();
                }

                // Drag & Drop source for files
                if (!entry.is_directory() && ImGui::BeginDragDropSource()) {
                    const std::string relative_path = std::filesystem::relative(path, base_data_path).string();
                    ImGui::SetDragDropPayload("ASSET_PATH", relative_path.c_str(), relative_path.size() + 1);
                    ImGui::Text("%s", filename.c_str());
                    ImGui::EndDragDropSource();
                }

                // Per-item context menu
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rename")) {
                        s_rename_target = path;
                        std::strncpy(s_rename_buf, filename.c_str(), sizeof(s_rename_buf) - 1);
                        s_rename_buf[sizeof(s_rename_buf) - 1] = '\0';
                    }
                    if (ImGui::MenuItem("Delete")) {
                        try { std::filesystem::remove_all(path); } catch (...) {}
                    }
                    if (ImGui::MenuItem("Copy Path")) {
                        ImGui::SetClipboardText(path.string().c_str());
                    }
#if defined(_WIN32)
                    if (ImGui::MenuItem("Show in Explorer")) {
                        std::wstring cmd = L"/select,\"" + path.wstring() + L"\"";
                        ShellExecuteW(nullptr, L"open", L"explorer.exe", cmd.c_str(), nullptr, SW_SHOWNORMAL);
                    }
#endif
                    ImGui::EndPopup();
                }

                ImGui::PopID();
                col++;
                if (col < columns) {
                    ImGui::SameLine();
                } else {
                    col = 0;
                }
            }
        } else {
            // List view
            for (const auto& entry : entries) {
                const auto& path = entry.path();
                const std::string filename = path.filename().string();

                // Inline rename
                if (s_rename_target == path) {
                    ImGui::SetNextItemWidth(200);
                    if (ImGui::InputText("##rename_project", s_rename_buf, sizeof(s_rename_buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
                        try {
                            std::filesystem::rename(path, path.parent_path() / s_rename_buf);
                        } catch (...) {}
                        s_rename_target.clear();
                    }
                    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                        s_rename_target.clear();
                    }
                    continue;
                }

                if (entry.is_directory()) {
                    if (ImGui::Selectable((std::string(MDI_ICON_PACKAGE_VARIANT " ") + filename).c_str())) {
                        current_path /= path.filename();
                    }
                } else {
                    ImGui::Selectable((std::string(MDI_ICON_IMAGE " ") + filename).c_str());

                    if (ImGui::BeginDragDropSource()) {
                        const std::string relative_path = std::filesystem::relative(path, base_data_path).string();
                        ImGui::SetDragDropPayload("ASSET_PATH", relative_path.c_str(), relative_path.size() + 1);
                        ImGui::Text("%s", filename.c_str());
                        ImGui::EndDragDropSource();
                    }
                }

                // Per-item context menu
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rename")) {
                        s_rename_target = path;
                        std::strncpy(s_rename_buf, filename.c_str(), sizeof(s_rename_buf) - 1);
                        s_rename_buf[sizeof(s_rename_buf) - 1] = '\0';
                    }
                    if (ImGui::MenuItem("Delete")) {
                        try { std::filesystem::remove_all(path); } catch (...) {}
                    }
                    if (ImGui::MenuItem("Copy Path")) {
                        ImGui::SetClipboardText(path.string().c_str());
                    }
#if defined(_WIN32)
                    if (ImGui::MenuItem("Show in Explorer")) {
                        std::wstring cmd = L"/select,\"" + path.wstring() + L"\"";
                        ShellExecuteW(nullptr, L"open", L"explorer.exe", cmd.c_str(), nullptr, SW_SHOWNORMAL);
                    }
#endif
                    ImGui::EndPopup();
                }
            }
        }
    }

    ImGui::End();
}

void DrawConsolePanel() {
    DrawConsolePanelImpl();
}

void DrawLocalizationPreviewPanel(EditorAuxPanelsContext& context) {
    ImGui::Begin("Localization Preview");
    auto& localization = dse::gameplay2d::LocalizationSystem::GetInstance();
    ImGui::Text("Current Language: %s", localization.GetCurrentLanguage().c_str());
    if (context.read_only) {
        ImGui::BeginDisabled(true);
    }
    ImGui::InputText("Preview Key", context.localization_preview_key, context.localization_preview_key_size);
    ImGui::InputText("Fallback", context.localization_preview_fallback, context.localization_preview_fallback_size);

    std::unordered_map<std::string, std::string> preview_params;
    preview_params["lang"] = localization.GetCurrentLanguage();
    preview_params["entity"] = context.selected_entity == entt::null
        ? std::string("None")
        : std::to_string(static_cast<uint32_t>(context.selected_entity));

    const std::string preview_text = localization.GetTextWithParams(
        context.localization_preview_key,
        preview_params,
        context.localization_preview_fallback);

    ImGui::Separator();
    ImGui::TextWrapped("%s", preview_text.c_str());

    if (context.selected_entity != entt::null &&
        context.registry.valid(context.selected_entity) &&
        context.registry.all_of<UILabelComponent>(context.selected_entity)) {
        if (ImGui::Button("Apply To Selected UILabel")) {
            auto& label = context.registry.get<UILabelComponent>(context.selected_entity);
            label.use_localization = true;
            label.localization_key = context.localization_preview_key;
            label.fallback_text = context.localization_preview_fallback;
            label.localization_params = preview_params;
            label.dirty = true;
        }
    } else {
        ImGui::TextDisabled("Select a UILabel entity to apply preview settings.");
    }

    if (context.read_only) {
        ImGui::EndDisabled();
        ImGui::TextDisabled("Play 模式下已禁用本地化预览写入。请退出 Play 后应用到 UILabel。");
    }

    ImGui::End();
}

void DrawAnimationPanel() {
    ImGui::Begin("Animation");
    ImGui::Text("No animated object selected.");
    ImGui::End();
}

void DrawTilePalettePanel(const EditorAuxPanelsContext& context) {
    ImGui::Begin("Tile Palette");
    if (!context.is_2d) {
        ImGui::TextDisabled("Tile Palette is only available in 2D mode.");
    } else {
        ImGui::Button("Active Brush", ImVec2(120, 24)); ImGui::SameLine();
        ImGui::Button("Paint", ImVec2(60, 24)); ImGui::SameLine();
        ImGui::Button("Erase", ImVec2(60, 24));

        ImGui::Separator();
        ImGui::Text("Select a tilemap to start painting.");

        ImVec2 p = ImGui::GetCursorScreenPos();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 6; x++) {
                draw_list->AddRectFilled(
                    ImVec2(p.x + x * 32, p.y + y * 32),
                    ImVec2(p.x + x * 32 + 30, p.y + y * 32 + 30),
                    IM_COL32(80 + (x + y) * 10, 100 + x * 10, 120 + y * 10, 255));
            }
        }
    }
    ImGui::End();
}

} // namespace dse::editor
