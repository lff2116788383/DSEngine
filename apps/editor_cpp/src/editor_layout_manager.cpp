#include "editor_layout_manager.h"
#include "editor_shell.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstring>

namespace dse::editor {

namespace {

std::filesystem::path GetLayoutDir() {
    // Derive from ImGui's IniFilename location (set in editor_app.cpp)
    const char* ini = ImGui::GetIO().IniFilename;
    std::filesystem::path ini_path = ini ? ini : "editor_layout.ini";
    auto dir = ini_path.parent_path() / "layouts";
    std::filesystem::create_directories(dir);
    return dir;
}

std::filesystem::path GetLayoutFilePath(const std::string& name) {
    return GetLayoutDir() / (name + ".ini");
}

char s_save_name_buf[128] = "";

} // namespace

std::vector<LayoutPreset> GetLayoutPresets() {
    std::vector<LayoutPreset> presets;
    auto dir = GetLayoutDir();
    if (!std::filesystem::exists(dir)) return presets;

    for (auto& entry : std::filesystem::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".ini") {
            LayoutPreset p;
            p.name = entry.path().stem().string();
            p.ini_path = entry.path().string();
            presets.push_back(p);
        }
    }
    std::sort(presets.begin(), presets.end(),
        [](const LayoutPreset& a, const LayoutPreset& b) { return a.name < b.name; });
    return presets;
}

void SaveLayoutPreset(const std::string& name) {
    if (name.empty()) return;
    auto path = GetLayoutFilePath(name);

    // ImGui saves current state to its IniFilename automatically,
    // but we can also call SaveIniSettingsToMemory and write it out.
    size_t size = 0;
    const char* data = ImGui::SaveIniSettingsToMemory(&size);
    if (data && size > 0) {
        std::ofstream f(path, std::ios::binary);
        f.write(data, static_cast<std::streamsize>(size));
    }
}

void LoadLayoutPreset(const std::string& name) {
    auto path = GetLayoutFilePath(name);
    if (!std::filesystem::exists(path)) return;

    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return;
    auto size = f.tellg();
    if (size <= 0) return;  // tellg 失败(-1)时若不兜底，static_cast<size_t> 会请求约 SIZE_MAX 字节 → bad_alloc
    f.seekg(0);
    std::string content(static_cast<size_t>(size), '\0');
    f.read(content.data(), size);

    ImGui::LoadIniSettingsFromMemory(content.c_str(), content.size());

    // Force dock rebuild
    ResetEditorLayout();
}

void DeleteLayoutPreset(const std::string& name) {
    auto path = GetLayoutFilePath(name);
    std::filesystem::remove(path);
}

void DrawLayoutMenu() {
    if (ImGui::BeginMenu("Layouts")) {
        auto presets = GetLayoutPresets();

        // Built-in presets
        if (ImGui::MenuItem("Default")) {
            ResetEditorLayout();
        }

        if (!presets.empty()) {
            ImGui::Separator();
            ImGui::TextDisabled("Saved Layouts");
            for (auto& p : presets) {
                if (ImGui::BeginMenu(p.name.c_str())) {
                    if (ImGui::MenuItem("Load")) {
                        LoadLayoutPreset(p.name);
                    }
                    if (ImGui::MenuItem("Delete")) {
                        DeleteLayoutPreset(p.name);
                    }
                    ImGui::EndMenu();
                }
            }
        }

        ImGui::Separator();
        ImGui::TextDisabled("Save Current");
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputText("##LayoutName", s_save_name_buf, sizeof(s_save_name_buf));
        ImGui::SameLine();
        if (ImGui::SmallButton("Save") && s_save_name_buf[0] != '\0') {
            SaveLayoutPreset(s_save_name_buf);
            s_save_name_buf[0] = '\0';
        }

        ImGui::EndMenu();
    }
}

} // namespace dse::editor
