#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace dse::editor {

/// 项目描述符 — 对应 project.dseproj 文件内容
struct ProjectDescriptor {
    int format_version = 1;
    std::string name;
    std::string version = "1.0.0";
    std::string engine_version;
    std::string description;
    std::vector<std::string> features;   // "lua_scripting", "cpp_plugins", ...
    std::string entry_script;            // 相对于项目根目录
    std::string default_scene = "scenes/main.json";
    std::string asset_dir = "assets/";
    std::string scene_dir = "scenes/";
    std::string script_dir = "scripts/";

    struct BuildConfig {
        std::string output_dir = "build/";
        std::string target = "standalone";
    } build;
};

/// 项目模板类型
enum class ProjectTemplate {
    Empty,
    Game2D,
    Game3D,
    LuaScripting,
};

/// 项目管理器 — 管理当前项目的生命周期
class ProjectManager {
public:
    static ProjectManager& Get();

    /// 是否有项目处于打开状态
    bool HasOpenProject() const { return is_open_; }

    /// 获取当前项目描述符（仅在 HasOpenProject 时有效）
    const ProjectDescriptor& GetDescriptor() const { return descriptor_; }

    /// 获取项目根目录（project.dseproj 所在目录）
    const std::filesystem::path& GetProjectRoot() const { return project_root_; }

    /// 获取项目资产目录的绝对路径
    std::filesystem::path GetAssetDir() const;

    /// 获取项目场景目录的绝对路径
    std::filesystem::path GetSceneDir() const;

    /// 获取项目脚本目录的绝对路径
    std::filesystem::path GetScriptDir() const;

    /// 从指定路径加载 project.dseproj，成功返回 true
    bool OpenProject(const std::filesystem::path& dseproj_path);

    /// 保存当前项目描述符到 project.dseproj
    bool SaveProject();

    /// 关闭当前项目（释放 .lock）
    void CloseProject();

    /// 创建新项目目录结构 + project.dseproj，成功后自动打开
    bool CreateProject(const std::filesystem::path& parent_dir,
                       const std::string& name,
                       ProjectTemplate tmpl);

    /// 尝试获取项目锁，失败说明已有编辑器实例打开此项目
    bool TryAcquireLock(const std::filesystem::path& project_root);

    /// 释放项目锁
    void ReleaseLock();

private:
    ProjectManager() = default;

    bool LoadDescriptor(const std::filesystem::path& dseproj_path);
    bool SaveDescriptor(const std::filesystem::path& dseproj_path);
    void GenerateTemplate(const std::filesystem::path& project_root,
                          const std::string& name,
                          ProjectTemplate tmpl);

    ProjectDescriptor descriptor_;
    std::filesystem::path project_root_;
    std::filesystem::path lock_path_;
    bool is_open_ = false;
};

/// 打开 .dseproj 文件的对话框，返回空字符串表示取消
std::string OpenProjectFileDialog();

/// 浏览项目文件夹的对话框（用于 New Project 选位置），返回空字符串表示取消
std::string BrowseNewProjectLocationDialog();

} // namespace dse::editor
