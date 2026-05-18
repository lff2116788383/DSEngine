#pragma once

#include <string>
#include <vector>
#include <filesystem>

#include <memory>
#include <entt/entt.hpp>
#include "editor_test_harness.h"
#include "editor_control_server.h"
#include "editor_plugin_manager.h"

struct GLFWwindow;

namespace dse::runtime {
class EngineInstance;
struct EngineRunConfig;
}

namespace dse::editor {

/// 编辑器应用类：管理窗口、ImGui、引擎实例、主循环生命周期。
/// 替代原 main.cpp 中的大量 static 全局状态。
class EditorApp {
public:
    EditorApp() = default;
    ~EditorApp() = default;

    EditorApp(const EditorApp&) = delete;
    EditorApp& operator=(const EditorApp&) = delete;

    /// 初始化窗口、ImGui、引擎。返回 false 表示失败。
    bool Init(int argc, char* argv[]);

    /// 执行主循环直到窗口关闭或帧数耗尽。
    void Run();

    /// 关闭并释放所有资源。
    void Shutdown();


    /// 获取项目根目录
    static std::filesystem::path GetProjectRootPath();
    /// 获取 bin 输出目录
    static std::filesystem::path GetEditorBinPath();

private:
    void DrawEditorUI(unsigned int scene_texture, unsigned int game_texture);
    void EnsureEditorLocalizationData();

    // Window
    GLFWwindow* window_ = nullptr;

    // Engine
    dse::runtime::EngineInstance* engine_instance_ = nullptr;

    // Control Server (WebSocket JSON-RPC)
    std::unique_ptr<ControlServer> control_server_;

    // Plugin Manager
    PluginManager plugin_manager_;
    bool show_plugins_ = false;

    // Automation / test
    dse::editor::test::EditorTestConfig test_config_{};
    int frames_remaining_ = -1;

    // DrawEditorUI 状态（替代原 static 变量）
    entt::entity selected_entity_{entt::null};
    int last_editor_state_ = 0; // cast of EditorState::Edit
    bool is_2d_ = false;
    bool inspector_active_ = true;
    bool inspector_static_ = false;
    bool sprite_flip_x_ = false;
    bool sprite_flip_y_ = false;
    bool collider_is_trigger_ = false;
    char localization_preview_key_[128] = "editor.preview.status";
    char localization_preview_fallback_[128] = "Language: {lang}";
    bool show_plugins_panel_ = false;
    bool show_preferences_ = false;
};

} // namespace dse::editor
