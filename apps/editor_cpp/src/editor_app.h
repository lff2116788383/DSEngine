#pragma once

#include <string>
#include <vector>
#include <filesystem>

#include <memory>
#include <entt/entt.hpp>
#include "engine/profiler/cpu_profiler.h"
#include "engine/profiler/memory_profiler.h"
#include "engine/profiler/render_profiler.h"
#include "editor_test_harness.h"
#include "editor_control_server.h"
#include "editor_plugin_manager.h"
#include "editor_chat_panel.h"

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

    // AI Chat Panel
    ChatPanel chat_panel_;
    bool show_chat_panel_ = false;

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

    // Optional panels (hidden by default, toggle via Window menu)
    bool show_localization_preview_ = false;
    bool show_profiler_ = false;
    bool show_animation_ = false;
    bool show_tile_palette_ = false;
    bool show_terrain_editor_ = false;
    bool show_lua_console_ = false;
    bool show_undo_history_ = false;
    bool show_asset_browser_ = false;
    bool show_animation_timeline_ = false;
    bool show_navmesh_ = false;
    bool show_shader_graph_ = false;
    bool show_git_ = false;
    bool show_multi_viewport_ = false;
    bool show_anim_state_machine_ = false;
    bool show_lua_debugger_ = false;

    // Profiler 实例（每帧通过 EditorContext 传递引用）
    dse::profiler::CPUProfiler cpu_profiler_;
    dse::profiler::MemoryProfiler memory_profiler_;
    dse::profiler::RenderProfiler render_profiler_;

    // Gizmo 状态
    int current_gizmo_operation_ = 0;
    int current_gizmo_mode_ = 0;

    // Language preview
    std::vector<std::string> editor_languages_;
    int editor_language_index_ = 0;
};

} // namespace dse::editor
