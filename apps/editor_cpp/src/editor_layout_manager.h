#pragma once

#include <string>
#include <vector>

namespace dse::editor {

struct LayoutPreset {
    std::string name;
    std::string ini_path;
};

/// Get the list of saved layout presets
std::vector<LayoutPreset> GetLayoutPresets();

/// Save the current ImGui layout to a named preset
void SaveLayoutPreset(const std::string& name);

/// Load a named layout preset (applies on next frame)
void LoadLayoutPreset(const std::string& name);

/// Delete a named layout preset
void DeleteLayoutPreset(const std::string& name);

/// Draw the layout management sub-menu (inside Window menu)
void DrawLayoutMenu();

} // namespace dse::editor
