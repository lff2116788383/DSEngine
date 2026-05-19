#include "editor_project.h"
#include "editor_console_panel.h"
#include "editor_file_dialog.h"

#include <fstream>
#include <sstream>
#include <algorithm>

#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/prettywriter.h>

#include "engine/dse_version.h"

#if defined(_WIN32)
#include <Windows.h>
#include <commdlg.h>
#include <shlobj.h>
#endif

namespace dse::editor {

// ============================================================
// ProjectManager — 单例
// ============================================================

ProjectManager& ProjectManager::Get() {
    static ProjectManager instance;
    return instance;
}

std::filesystem::path ProjectManager::GetAssetDir() const {
    return project_root_ / descriptor_.asset_dir;
}

std::filesystem::path ProjectManager::GetSceneDir() const {
    return project_root_ / descriptor_.scene_dir;
}

std::filesystem::path ProjectManager::GetScriptDir() const {
    return project_root_ / descriptor_.script_dir;
}

// ============================================================
// 项目锁
// ============================================================

bool ProjectManager::TryAcquireLock(const std::filesystem::path& project_root) {
    lock_path_ = project_root / ".lock";

    // 检查是否已被锁定
    if (std::filesystem::exists(lock_path_)) {
        std::ifstream ifs(lock_path_);
        std::string pid_str;
        std::getline(ifs, pid_str);
        ifs.close();

#if defined(_WIN32)
        // 检查持锁进程是否仍然存活
        if (!pid_str.empty()) {
            DWORD pid = static_cast<DWORD>(std::stoul(pid_str));
            HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
            if (proc != nullptr) {
                DWORD exit_code = 0;
                if (GetExitCodeProcess(proc, &exit_code) && exit_code == STILL_ACTIVE) {
                    CloseHandle(proc);
                    return false;  // 另一个编辑器实例正在使用此项目
                }
                CloseHandle(proc);
            }
            // 进程已退出，清理陈旧锁
        }
#endif
    }

    // 写入当前进程 PID
    std::ofstream ofs(lock_path_, std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }
#if defined(_WIN32)
    ofs << GetCurrentProcessId();
#else
    ofs << getpid();
#endif
    ofs.close();
    return true;
}

void ProjectManager::ReleaseLock() {
    if (!lock_path_.empty() && std::filesystem::exists(lock_path_)) {
        std::error_code ec;
        std::filesystem::remove(lock_path_, ec);
    }
    lock_path_.clear();
}

// ============================================================
// 加载/保存 project.dseproj
// ============================================================

bool ProjectManager::LoadDescriptor(const std::filesystem::path& dseproj_path) {
    std::ifstream ifs(dseproj_path);
    if (!ifs.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    ifs.close();

    rapidjson::Document doc;
    if (doc.Parse(content.c_str()).HasParseError() || !doc.IsObject()) {
        return false;
    }

    ProjectDescriptor desc;

    if (doc.HasMember("format_version") && doc["format_version"].IsInt())
        desc.format_version = doc["format_version"].GetInt();
    if (doc.HasMember("name") && doc["name"].IsString())
        desc.name = doc["name"].GetString();
    if (doc.HasMember("version") && doc["version"].IsString())
        desc.version = doc["version"].GetString();
    if (doc.HasMember("engine_version") && doc["engine_version"].IsString())
        desc.engine_version = doc["engine_version"].GetString();
    if (doc.HasMember("description") && doc["description"].IsString())
        desc.description = doc["description"].GetString();
    if (doc.HasMember("entry_script") && doc["entry_script"].IsString())
        desc.entry_script = doc["entry_script"].GetString();
    if (doc.HasMember("default_scene") && doc["default_scene"].IsString())
        desc.default_scene = doc["default_scene"].GetString();
    if (doc.HasMember("asset_dir") && doc["asset_dir"].IsString())
        desc.asset_dir = doc["asset_dir"].GetString();
    if (doc.HasMember("scene_dir") && doc["scene_dir"].IsString())
        desc.scene_dir = doc["scene_dir"].GetString();
    if (doc.HasMember("script_dir") && doc["script_dir"].IsString())
        desc.script_dir = doc["script_dir"].GetString();

    if (doc.HasMember("features") && doc["features"].IsArray()) {
        for (auto& v : doc["features"].GetArray()) {
            if (v.IsString()) {
                desc.features.emplace_back(v.GetString());
            }
        }
    }

    if (doc.HasMember("build") && doc["build"].IsObject()) {
        const auto& build_obj = doc["build"];
        if (build_obj.HasMember("output_dir") && build_obj["output_dir"].IsString())
            desc.build.output_dir = build_obj["output_dir"].GetString();
        if (build_obj.HasMember("target") && build_obj["target"].IsString())
            desc.build.target = build_obj["target"].GetString();
    }

    descriptor_ = std::move(desc);
    return true;
}

bool ProjectManager::SaveDescriptor(const std::filesystem::path& dseproj_path) {
    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    doc.AddMember("format_version", descriptor_.format_version, alloc);
    doc.AddMember("name", rapidjson::Value(descriptor_.name.c_str(), alloc), alloc);
    doc.AddMember("version", rapidjson::Value(descriptor_.version.c_str(), alloc), alloc);
    doc.AddMember("engine_version", rapidjson::Value(descriptor_.engine_version.c_str(), alloc), alloc);
    doc.AddMember("description", rapidjson::Value(descriptor_.description.c_str(), alloc), alloc);

    rapidjson::Value features_arr(rapidjson::kArrayType);
    for (const auto& f : descriptor_.features) {
        features_arr.PushBack(rapidjson::Value(f.c_str(), alloc), alloc);
    }
    doc.AddMember("features", features_arr, alloc);

    doc.AddMember("entry_script", rapidjson::Value(descriptor_.entry_script.c_str(), alloc), alloc);
    doc.AddMember("default_scene", rapidjson::Value(descriptor_.default_scene.c_str(), alloc), alloc);
    doc.AddMember("asset_dir", rapidjson::Value(descriptor_.asset_dir.c_str(), alloc), alloc);
    doc.AddMember("scene_dir", rapidjson::Value(descriptor_.scene_dir.c_str(), alloc), alloc);
    doc.AddMember("script_dir", rapidjson::Value(descriptor_.script_dir.c_str(), alloc), alloc);

    rapidjson::Value build_obj(rapidjson::kObjectType);
    build_obj.AddMember("output_dir", rapidjson::Value(descriptor_.build.output_dir.c_str(), alloc), alloc);
    build_obj.AddMember("target", rapidjson::Value(descriptor_.build.target.c_str(), alloc), alloc);
    doc.AddMember("build", build_obj, alloc);

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::ofstream ofs(dseproj_path, std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }
    ofs << buffer.GetString();
    return true;
}

// ============================================================
// Open / Save / Close
// ============================================================

bool ProjectManager::OpenProject(const std::filesystem::path& dseproj_path) {
    if (is_open_) {
        CloseProject();
    }

    if (!std::filesystem::exists(dseproj_path)) {
        EditorLog(LogLevel::Error, "Project file not found: " + dseproj_path.string());
        return false;
    }

    std::filesystem::path root = dseproj_path.parent_path();

    if (!TryAcquireLock(root)) {
        EditorLog(LogLevel::Error, "Project is already open in another editor instance");
        return false;
    }

    if (!LoadDescriptor(dseproj_path)) {
        EditorLog(LogLevel::Error, "Failed to parse project file: " + dseproj_path.string());
        ReleaseLock();
        return false;
    }

    project_root_ = root;
    is_open_ = true;
    EditorLog(LogLevel::Info, "Opened project: " + descriptor_.name + " (" + root.string() + ")");
    return true;
}

bool ProjectManager::SaveProject() {
    if (!is_open_) {
        return false;
    }
    std::filesystem::path dseproj = project_root_ / "project.dseproj";
    if (!SaveDescriptor(dseproj)) {
        EditorLog(LogLevel::Error, "Failed to save project: " + dseproj.string());
        return false;
    }
    EditorLog(LogLevel::Info, "Project saved: " + descriptor_.name);
    return true;
}

void ProjectManager::CloseProject() {
    if (!is_open_) {
        return;
    }
    EditorLog(LogLevel::Info, "Closing project: " + descriptor_.name);
    ReleaseLock();
    descriptor_ = ProjectDescriptor{};
    project_root_.clear();
    is_open_ = false;
}

// ============================================================
// 创建新项目
// ============================================================

bool ProjectManager::CreateProject(const std::filesystem::path& parent_dir,
                                    const std::string& name,
                                    ProjectTemplate tmpl) {
    std::filesystem::path project_root = parent_dir / name;

    // 目录已存在且非空
    if (std::filesystem::exists(project_root) && !std::filesystem::is_empty(project_root)) {
        EditorLog(LogLevel::Error, "Directory is not empty: " + project_root.string());
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(project_root, ec);
    if (ec) {
        EditorLog(LogLevel::Error, "Failed to create project directory: " + ec.message());
        return false;
    }

    // 生成模板文件
    GenerateTemplate(project_root, name, tmpl);

    // 打开新创建的项目
    return OpenProject(project_root / "project.dseproj");
}

void ProjectManager::GenerateTemplate(const std::filesystem::path& project_root,
                                       const std::string& name,
                                       ProjectTemplate tmpl) {
    // 创建子目录
    std::filesystem::create_directories(project_root / "scenes");
    std::filesystem::create_directories(project_root / "scripts");
    std::filesystem::create_directories(project_root / "assets" / "textures");
    std::filesystem::create_directories(project_root / "assets" / "models");
    std::filesystem::create_directories(project_root / "assets" / "audio");
    std::filesystem::create_directories(project_root / "assets" / "font");

    // 构建 ProjectDescriptor
    ProjectDescriptor desc;
    desc.name = name;

    // 写入 engine_version
    std::ostringstream ver_ss;
    ver_ss << DSE_VERSION_MAJOR << "." << DSE_VERSION_MINOR << "." << DSE_VERSION_PATCH;
    desc.engine_version = ver_ss.str();

    // 按模板设置 features 和入口
    switch (tmpl) {
    case ProjectTemplate::Empty:
        break;
    case ProjectTemplate::Game2D:
        desc.features.push_back("lua_scripting");
        desc.entry_script = "scripts/main.lua";
        break;
    case ProjectTemplate::Game3D:
        desc.features.push_back("lua_scripting");
        desc.entry_script = "scripts/main.lua";
        break;
    case ProjectTemplate::LuaScripting:
        desc.features.push_back("lua_scripting");
        desc.entry_script = "scripts/main.lua";
        break;
    }

    descriptor_ = desc;
    SaveDescriptor(project_root / "project.dseproj");

    // 生成默认场景（空的 JSON 场景）
    {
        std::ofstream ofs(project_root / "scenes" / "main.json", std::ios::trunc);
        ofs << "{\n  \"entities\": []\n}\n";
    }

    // 按模板生成脚本文件
    if (tmpl == ProjectTemplate::Game2D || tmpl == ProjectTemplate::LuaScripting) {
        std::ofstream ofs(project_root / "scripts" / "main.lua", std::ios::trunc);
        ofs << "-- " << name << " entry script\n"
            << "-- DSEngine Lua scripting\n\n"
            << "function on_init()\n"
            << "    print(\"" << name << " initialized\")\n"
            << "end\n\n"
            << "function on_update(dt)\n"
            << "end\n";
    } else if (tmpl == ProjectTemplate::Game3D) {
        std::ofstream ofs(project_root / "scripts" / "main.lua", std::ios::trunc);
        ofs << "-- " << name << " entry script (3D)\n"
            << "-- DSEngine Lua scripting\n\n"
            << "function on_init()\n"
            << "    print(\"" << name << " 3D project initialized\")\n"
            << "    -- TODO: set up camera, lights, meshes\n"
            << "end\n\n"
            << "function on_update(dt)\n"
            << "end\n";
    }

    // .gitignore
    {
        std::ofstream ofs(project_root / ".gitignore", std::ios::trunc);
        ofs << "build/\n"
            << ".lock\n"
            << "*.log\n";
    }
}

// ============================================================
// 文件对话框
// ============================================================

#if defined(_WIN32)

namespace {

std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
        static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size_needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
        static_cast<int>(wide.size()), result.data(), size_needed, nullptr, nullptr);
    return result;
}

} // namespace

std::string OpenProjectFileDialog() {
    OPENFILENAMEW ofn = {};
    wchar_t file_buf[MAX_PATH] = {};

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = L"DSEngine Project (*.dseproj)\0*.dseproj\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = file_buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Open Project";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"dseproj";

    if (GetOpenFileNameW(&ofn)) {
        return WideToUtf8(file_buf);
    }
    return {};
}

std::string BrowseNewProjectLocationDialog() {
    return BrowseFolderDialog("Select Project Location");
}

#else

std::string OpenProjectFileDialog() { return {}; }
std::string BrowseNewProjectLocationDialog() { return {}; }

#endif

} // namespace dse::editor
