#include "editor_aux_panels.h"

#include "engine/ecs/components_2d.h"
#include "modules/gameplay_2d/localization/localization_system.h"
#include "imgui.h"

#include <fstream>
#include <string>
#include <unordered_map>

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
    ImGui::Begin("Project");
    ImGui::Text("Assets");
    ImGui::Separator();

    const std::filesystem::path base_data_path = GetProjectBaseDataPath();
    std::filesystem::path& current_path = GetCurrentProjectPanelPath();

    if (ImGui::BeginPopupContextWindow("ProjectContextMenu")) {
        if (ImGui::BeginMenu("Create")) {
            if (ImGui::MenuItem("Folder")) {
                std::filesystem::create_directory(current_path / "NewFolder");
            }
            if (ImGui::MenuItem("Lua Script")) {
                std::ofstream ofs(current_path / "NewScript.lua");
                if (ofs.is_open()) {
                    ofs << "-- New Lua Script\n";
                }
            }
            if (ImGui::MenuItem("Material")) {
                std::ofstream ofs(current_path / "NewMaterial.mat");
                if (ofs.is_open()) {
                    ofs << "{\n  \"shader\": \"default\",\n  \"color\": [1,1,1,1]\n}\n";
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }

    if (!std::filesystem::exists(current_path)) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Data path not found: %s", current_path.string().c_str());
    } else {
        if (current_path != base_data_path) {
            if (ImGui::Button("<- Back")) {
                current_path = current_path.parent_path();
            }
            ImGui::Separator();
        }

        try {
            for (const auto& entry : std::filesystem::directory_iterator(current_path)) {
                const auto& path = entry.path();
                const std::string filename = path.filename().string();

                if (entry.is_directory()) {
                    if (ImGui::Selectable((std::string("[DIR] ") + filename).c_str())) {
                        current_path /= path.filename();
                    }
                } else {
                    ImGui::Selectable((std::string("[FILE] ") + filename).c_str());

                    if (ImGui::BeginDragDropSource()) {
                        const std::string relative_path = std::filesystem::relative(path, base_data_path).string();
                        ImGui::SetDragDropPayload("ASSET_PATH", relative_path.c_str(), relative_path.size() + 1);
                        ImGui::Text("%s", filename.c_str());
                        ImGui::EndDragDropSource();
                    }
                }
            }
        } catch (const std::filesystem::filesystem_error& e) {
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Error reading directory: %s", e.what());
        }
    }

    ImGui::End();
}

void DrawConsolePanel() {
    ImGui::Begin("Console");
    ImGui::Text("[Info] Engine initialized successfully.");
    ImGui::Text("[Info] Loaded default scene.");
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[Warning] Missing texture 'skybox_diffuse'.");
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[Error] Failed to load shader 'standard_pbr.glsl'.");
    ImGui::End();
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
