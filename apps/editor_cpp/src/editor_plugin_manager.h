#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <memory>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace dse::editor {

// ─── 插件元数据（对应 plugin.json） ─────────────────────────────────────────

struct PluginMetadata {
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::string runtime;       // "python" / "node" / "executable"
    std::string entry;         // 入口文件（相对于插件目录）
    bool requires_ui = false;
    int ui_port = 0;
};

// ─── 插件实例状态 ───────────────────────────────────────────────────────────

enum class PluginState {
    Stopped,
    Running,
    Error
};

struct PluginInstance {
    PluginMetadata metadata;
    std::filesystem::path directory;
    PluginState state = PluginState::Stopped;
    bool enabled = false;
    std::string last_error;

#ifdef _WIN32
    HANDLE process_handle = nullptr;
    DWORD process_id = 0;
#else
    pid_t process_id = 0;
#endif
};

// ─── PluginManager ──────────────────────────────────────────────────────────

class PluginManager {
public:
    PluginManager() = default;
    ~PluginManager();

    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    /// 扫描 plugins/ 目录，加载所有 plugin.json
    void ScanPlugins(const std::filesystem::path& plugins_dir);

    /// 启动插件进程
    bool StartPlugin(size_t index);

    /// 停止插件进程
    void StopPlugin(size_t index);

    /// 停止所有插件
    void StopAll();

    /// 检查插件进程存活状态（主循环中定期调用）
    void PollStatus();

    const std::vector<PluginInstance>& GetPlugins() const { return plugins_; }
    std::vector<PluginInstance>& GetPlugins() { return plugins_; }
    size_t GetPluginCount() const { return plugins_.size(); }

private:
    bool ParsePluginJson(const std::filesystem::path& json_path, PluginMetadata& out);
    std::string ResolveRuntime(const std::string& runtime);

    std::vector<PluginInstance> plugins_;
    std::filesystem::path plugins_dir_;
};

// ─── ImGui Panel ────────────────────────────────────────────────────────────

void DrawPluginManagerPanel(PluginManager& manager);

} // namespace dse::editor
