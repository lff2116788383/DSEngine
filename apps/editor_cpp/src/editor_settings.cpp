#include "editor_settings.h"

#include <filesystem>
#include <fstream>
#include <algorithm>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace dse::editor {

namespace {

std::filesystem::path GetSettingsFilePath() {
    std::filesystem::path path;
#if defined(_WIN32)
    std::wstring module_path(MAX_PATH, L'\0');
    const DWORD size = GetModuleFileNameW(nullptr, module_path.data(), static_cast<DWORD>(module_path.size()));
    if (size > 0) {
        module_path.resize(size);
        path = std::filesystem::path(module_path).parent_path();
    }
#endif
    if (path.empty()) {
        path = std::filesystem::current_path();
    }
    if (path.filename() == "bin" || path.filename() == "build_vs2022") {
        // Already in bin or build dir
        if (path.filename() == "build_vs2022") {
            path = path.parent_path() / "bin";
        }
    } else {
        path = path / "bin";
    }
    std::filesystem::create_directories(path);
    return path / "editor_settings.json";
}

} // namespace

EditorSettings LoadEditorSettings() {
    EditorSettings settings;
    const auto file_path = GetSettingsFilePath();

    std::ifstream ifs(file_path);
    if (!ifs.is_open()) {
        return settings;
    }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();

    rapidjson::Document doc;
    if (doc.Parse(content.c_str()).HasParseError() || !doc.IsObject()) {
        return settings;
    }

    if (doc.HasMember("last_scene_path") && doc["last_scene_path"].IsString()) {
        settings.last_scene_path = doc["last_scene_path"].GetString();
    }
    if (doc.HasMember("default_gizmo_operation") && doc["default_gizmo_operation"].IsInt()) {
        settings.default_gizmo_operation = doc["default_gizmo_operation"].GetInt();
    }
    if (doc.HasMember("default_gizmo_mode") && doc["default_gizmo_mode"].IsInt()) {
        settings.default_gizmo_mode = doc["default_gizmo_mode"].GetInt();
    }
    if (doc.HasMember("recent_files") && doc["recent_files"].IsArray()) {
        for (auto& v : doc["recent_files"].GetArray()) {
            if (v.IsString()) {
                settings.recent_files.emplace_back(v.GetString());
            }
        }
    }
    if (doc.HasMember("last_project_path") && doc["last_project_path"].IsString()) {
        settings.last_project_path = doc["last_project_path"].GetString();
    }
    if (doc.HasMember("recent_projects") && doc["recent_projects"].IsArray()) {
        for (auto& v : doc["recent_projects"].GetArray()) {
            if (v.IsString()) {
                settings.recent_projects.emplace_back(v.GetString());
            }
        }
    }

    // Preferences
    if (doc.HasMember("theme_index") && doc["theme_index"].IsInt()) {
        settings.theme_index = doc["theme_index"].GetInt();
    }
    if (doc.HasMember("show_grid") && doc["show_grid"].IsBool()) {
        settings.show_grid = doc["show_grid"].GetBool();
    }
    if (doc.HasMember("snap_translate") && doc["snap_translate"].IsNumber()) {
        settings.snap_translate = doc["snap_translate"].GetFloat();
    }
    if (doc.HasMember("snap_rotate") && doc["snap_rotate"].IsNumber()) {
        settings.snap_rotate = doc["snap_rotate"].GetFloat();
    }
    if (doc.HasMember("snap_scale") && doc["snap_scale"].IsNumber()) {
        settings.snap_scale = doc["snap_scale"].GetFloat();
    }
    if (doc.HasMember("grid_size") && doc["grid_size"].IsNumber()) {
        settings.grid_size = doc["grid_size"].GetFloat();
    }
    if (doc.HasMember("grid_lines") && doc["grid_lines"].IsInt()) {
        settings.grid_lines = doc["grid_lines"].GetInt();
    }

    // Auto-save
    if (doc.HasMember("auto_save_enabled") && doc["auto_save_enabled"].IsBool()) {
        settings.auto_save_enabled = doc["auto_save_enabled"].GetBool();
    }
    if (doc.HasMember("auto_save_interval_sec") && doc["auto_save_interval_sec"].IsInt()) {
        settings.auto_save_interval_sec = doc["auto_save_interval_sec"].GetInt();
    }
    if (doc.HasMember("editor_ui_locale") && doc["editor_ui_locale"].IsString()) {
        settings.editor_ui_locale = doc["editor_ui_locale"].GetString();
    }

    // Scene camera
    if (doc.HasMember("cam_focal_x") && doc["cam_focal_x"].IsNumber()) settings.cam_focal_x = doc["cam_focal_x"].GetFloat();
    if (doc.HasMember("cam_focal_y") && doc["cam_focal_y"].IsNumber()) settings.cam_focal_y = doc["cam_focal_y"].GetFloat();
    if (doc.HasMember("cam_focal_z") && doc["cam_focal_z"].IsNumber()) settings.cam_focal_z = doc["cam_focal_z"].GetFloat();
    if (doc.HasMember("cam_distance") && doc["cam_distance"].IsNumber()) settings.cam_distance = doc["cam_distance"].GetFloat();
    if (doc.HasMember("cam_yaw") && doc["cam_yaw"].IsNumber()) settings.cam_yaw = doc["cam_yaw"].GetFloat();
    if (doc.HasMember("cam_pitch") && doc["cam_pitch"].IsNumber()) settings.cam_pitch = doc["cam_pitch"].GetFloat();

    return settings;
}

void SaveEditorSettings(const EditorSettings& settings) {
    const auto file_path = GetSettingsFilePath();

    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    doc.AddMember("last_scene_path", rapidjson::Value(settings.last_scene_path.c_str(), alloc), alloc);
    doc.AddMember("default_gizmo_operation", settings.default_gizmo_operation, alloc);
    doc.AddMember("default_gizmo_mode", settings.default_gizmo_mode, alloc);

    rapidjson::Value recent_arr(rapidjson::kArrayType);
    for (const auto& path : settings.recent_files) {
        recent_arr.PushBack(rapidjson::Value(path.c_str(), alloc), alloc);
    }
    doc.AddMember("recent_files", recent_arr, alloc);

    doc.AddMember("last_project_path", rapidjson::Value(settings.last_project_path.c_str(), alloc), alloc);

    rapidjson::Value project_arr(rapidjson::kArrayType);
    for (const auto& path : settings.recent_projects) {
        project_arr.PushBack(rapidjson::Value(path.c_str(), alloc), alloc);
    }
    doc.AddMember("recent_projects", project_arr, alloc);

    // Preferences
    doc.AddMember("theme_index", settings.theme_index, alloc);
    doc.AddMember("show_grid", settings.show_grid, alloc);
    doc.AddMember("snap_translate", settings.snap_translate, alloc);
    doc.AddMember("snap_rotate", settings.snap_rotate, alloc);
    doc.AddMember("snap_scale", settings.snap_scale, alloc);
    doc.AddMember("grid_size", settings.grid_size, alloc);
    doc.AddMember("grid_lines", settings.grid_lines, alloc);

    // Auto-save
    doc.AddMember("auto_save_enabled", settings.auto_save_enabled, alloc);
    doc.AddMember("auto_save_interval_sec", settings.auto_save_interval_sec, alloc);
    doc.AddMember("editor_ui_locale", rapidjson::Value(settings.editor_ui_locale.c_str(), alloc), alloc);

    // Scene camera
    doc.AddMember("cam_focal_x", settings.cam_focal_x, alloc);
    doc.AddMember("cam_focal_y", settings.cam_focal_y, alloc);
    doc.AddMember("cam_focal_z", settings.cam_focal_z, alloc);
    doc.AddMember("cam_distance", settings.cam_distance, alloc);
    doc.AddMember("cam_yaw", settings.cam_yaw, alloc);
    doc.AddMember("cam_pitch", settings.cam_pitch, alloc);

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream ofs(file_path, std::ios::trunc);
    if (ofs.is_open()) {
        ofs << buffer.GetString();
    }
}

void AddRecentFile(EditorSettings& settings, const std::string& path) {
    if (path.empty() || path == "Untitled") {
        return;
    }
    // Remove existing entry if present
    auto it = std::find(settings.recent_files.begin(), settings.recent_files.end(), path);
    if (it != settings.recent_files.end()) {
        settings.recent_files.erase(it);
    }
    // Insert at front
    settings.recent_files.insert(settings.recent_files.begin(), path);
    // Trim to max
    if (static_cast<int>(settings.recent_files.size()) > settings.max_recent_files) {
        settings.recent_files.resize(settings.max_recent_files);
    }
}

void AddRecentProject(EditorSettings& settings, const std::string& path) {
    if (path.empty()) {
        return;
    }
    auto it = std::find(settings.recent_projects.begin(), settings.recent_projects.end(), path);
    if (it != settings.recent_projects.end()) {
        settings.recent_projects.erase(it);
    }
    settings.recent_projects.insert(settings.recent_projects.begin(), path);
    if (static_cast<int>(settings.recent_projects.size()) > settings.max_recent_projects) {
        settings.recent_projects.resize(settings.max_recent_projects);
    }
}

} // namespace dse::editor
