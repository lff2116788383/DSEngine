#pragma once

#include <string>
#include <vector>

namespace dse::editor {

struct EditorSettings {
    std::vector<std::string> recent_files;
    std::string last_scene_path;
    int default_gizmo_operation = 0;  // 0=Translate, 1=Rotate, 2=Scale, -1=Hand
    int default_gizmo_mode = 0;       // 0=Local, 1=World
    int max_recent_files = 10;
};

/// Load editor settings from bin/editor_settings.json
EditorSettings LoadEditorSettings();

/// Save editor settings to bin/editor_settings.json
void SaveEditorSettings(const EditorSettings& settings);

/// Add a file to the recent files list (deduplicates and trims)
void AddRecentFile(EditorSettings& settings, const std::string& path);

} // namespace dse::editor
