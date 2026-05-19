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

    std::vector<std::string> recent_projects;
    std::string last_project_path;
    int max_recent_projects = 5;

    // Preferences (persisted)
    int theme_index = 0;           // 0=Dark, 1=Light
    bool show_grid = true;
    float grid_size = 1.0f;
    int grid_lines = 50;
    float snap_translate = 0.5f;
    float snap_rotate = 15.0f;
    float snap_scale = 0.1f;

    // Scene camera state (persisted)
    float cam_focal_x = 0.0f, cam_focal_y = 0.0f, cam_focal_z = 0.0f;
    float cam_distance = 10.0f;
    float cam_yaw = 0.0f;
    float cam_pitch = 0.3f;
};

/// Load editor settings from bin/editor_settings.json
EditorSettings LoadEditorSettings();

/// Save editor settings to bin/editor_settings.json
void SaveEditorSettings(const EditorSettings& settings);

/// Add a file to the recent files list (deduplicates and trims)
void AddRecentFile(EditorSettings& settings, const std::string& path);

/// Add a project to the recent projects list (deduplicates and trims)
void AddRecentProject(EditorSettings& settings, const std::string& path);

} // namespace dse::editor
