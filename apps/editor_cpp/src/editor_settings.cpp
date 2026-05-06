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

} // namespace dse::editor
